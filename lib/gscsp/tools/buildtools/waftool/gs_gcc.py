#!/usr/bin/env python
# encoding: utf-8
# Copyright (c) 2013-2017 GomSpace A/S. All rights reserved.
"""
GOMSpace Waf tool extension for: gcc, gxx, gas
Based on options --arch, --part, build tools will locate the correct compiler and settings.
"""

from waflib.Configure import conf
from waflib import Context
from waflib import Logs

import os
import datetime
import sys
import imp
import inspect

import gs_setup_buildtools
import gs.buildtools.compiler_settings
import gs.buildtools.util
# Extended buildtools
try:
    import gs_debpack
except ImportError:
    gs_debpack = None


def recurse_folder_for_wscripts(folder, maxlevel=2, level=1):
    ret = []
    if not os.path.isdir(folder):
        return ret
    if os.path.isfile(os.path.join(folder, 'wscript')):
        ret.append(folder)
    if level >= maxlevel:
        return ret
    for f in os.listdir(folder):
        if f.startswith("."):
            continue
        if os.path.isdir(os.path.join(folder, f)):
            ret += recurse_folder_for_wscripts(os.path.join(folder, f), maxlevel=maxlevel, level=level + 1)
    return ret


def is_not_excluded(ctx, module):
    for ex in ctx.gs_exclude_modules:
        if module.endswith(ex) or (ex + '/') in module:
            return False
    return True


def is_included(ctx, module):
    for o in ctx.gs_include_only_modules:
        if o in module:
            return True
    return False


def gs_recurse(ctx, modules=[], folder=None, include_base=False, exclude=[]):
    """
    Recurse down into client and lib folder and search for wscripts
    """
    if 'gs_exclude_modules' not in ctx.__dict__:
        ctx.gs_exclude_modules = exclude

    if 'gs_include_only_modules' not in ctx.__dict__:
        ctx.gs_include_only_modules = []
        for m in modules:
            ctx.gs_include_only_modules.append(m)

    if folder is None:
        folder = gs.buildtools.util.dir_callee()
    else:
        folder = os.path.abspath(folder)
    if include_base:
        modules += recurse_folder_for_wscripts(folder, maxlevel=0)
    if not gs_is_derived_build(ctx):
        modules += get_modules(ctx, search_folder=folder)
    modules += recurse_folder_for_wscripts(os.path.join(folder, 'client'), maxlevel=1)
    modules += recurse_folder_for_wscripts(os.path.join(folder, 'lib'))

    if ctx.cmd == 'gs_dist':
        modules = remove_modules_without_command(modules, ctx.cmd)

    # Remove modules that are in the exclude list
    modules = [x for x in modules if is_not_excluded(ctx, x)]

    # Remove modules that are not in the only list
    if ctx.gs_include_only_modules:
        modules = [x for x in modules if is_included(ctx, x)]

    ctx.recurse(modules)


def gs_recurse_unit_test(ctx, modules=[], folder='../..', include_base=True):
    """
    Convience recurse function for unit tests
    """
    gs_recurse(ctx, modules, folder, include_base)


def remove_modules_without_command(modules, cmd):
    ret = []
    for m in modules:
        with open(os.path.join(m, 'wscript')) as fp:
            if cmd in fp.read():
                ret.append(m)
    return ret


@conf
def gs_get_build_info(conf, defineList=None, appName=None, tag=None):
    """
    Return build information, e.g. timestamps, revision, etc.
    """
    bi = gs.buildtools.util.get_build_info(appName=appName, tag=tag)
    conf.msg('Repository rev.', bi.revision())
    if defineList:
        for key, value in bi.info().iteritems():
            if value:
                conf.env.append_unique(defineList, 'GS_BUILD_INFO_' + key.upper() + '=\"' + str(value) + '\"')
    return bi


@conf
def gs_write_config_header(conf, configfile='', defines=True, remove=True):
    """
    Extends Waf's write_config_header, by automatically add the folder to global include path.
    """
    conf.write_config_header(configfile=configfile, top=True, defines=defines, remove=remove)

    path = os.path.join(conf.bldnode.abspath(), os.path.dirname(configfile))
    conf.env.append_unique('INCLUDES', path)


@conf
def gs_is_bitbake(conf):
    """
    Return True if running in Bitbake framework
    """
    if conf and 'OptionsContext' not in conf.__class__.__name__:
        if conf.env.BITBAKE:
            return True

    return False


@conf
def gs_is_linux(conf):
    """
    Return True if target platform is linux (posix).
    """
    if 'linux' in conf.env.GS_OS:
        return True
    return False


@conf
def gs_is_freertos(conf):
    """
    Return True if target platform is FreeRTOS.
    """
    if 'freertos' in conf.env.GS_OS:
        return True
    return False


@conf
def gs_get_info(conf):
    """
    Show useful information about the target.
    """
    info = 'arch=[' + str(conf.env.GS_ARCH) + '], part=[' + str(conf.env.GS_PART) + ']'
    return info


def gs_set_coverage_link_flags(kw):
    if 'linkflags' in kw:
        kw['linkflags'].extend(['-fprofile-arcs', '-ftest-coverage'])
    else:
        kw['linkflags'] = ['-fprofile-arcs', '-ftest-coverage']


def gs_set_coverage_compile_flags(kw):
    if 'cflags' in kw:
        kw['cflags'].extend(['-fprofile-arcs', '-ftest-coverage', '-O0'])
    else:
        kw['cflags'] = ['-fprofile-arcs', '-ftest-coverage', '-O0']
    if 'cppflags' in kw:
        kw['cppflags'].extend(['-fprofile-arcs', '-ftest-coverage', '-O0'])
    else:
        kw['cppflags'] = ['-fprofile-arcs', '-ftest-coverage', '-O0']


@conf
def gs_is_build_disabled(conf, keys):
    for k in keys:
        if k in conf.env.GS_BUILD_DISABLE:
            return True
    return False


def _has_no_source(kw):
    key = 'source'
    if (key in kw) and (len(kw[key]) > 0):
        return False
    return True


def _create_header_target(conf, kw, name, base_name):
    key = 'export_includes'
    includes = getattr(kw, key, None)
    if not includes:
        try:
            bt = conf.get_tgen_by_name(base_name + '_h')
            if bt:
                includes = getattr(bt, key, None)
        except Exception:
            pass
    if includes:
        conf(export_includes=includes, name=kw['name'])


@conf
def gs_objects(conf, **kw):

    if 'name' not in kw:
        kw['name'] = kw['target']

    if gs_is_build_disabled(conf, ['objects', kw['name']]):
            return  # build disabled

    # on linux also build coverage objects if enabled
    if gs_is_linux(conf) and conf.env.GS_TEST_COVERAGE:
        gs_set_coverage_compile_flags(kw)

    conf.objects(**kw)


@conf
def gs_stlib(conf, **kw):
    if _has_no_source(kw):
        return  # no source files

    if 'name' not in kw:
        kw['name'] = _rewrite_stlib_name(kw['target'])

    if gs_is_build_disabled(conf, ['stlib', kw['name']]):
        return  # build disabled

    # on linux also build coverage objects if enabled
    if gs_is_linux(conf) and conf.env.GS_TEST_COVERAGE:
        gs_set_coverage_compile_flags(kw)

    # prefix target name to avoid clashes
    key = 'gs_prefix'
    if key not in kw:
        kw[key] = 'gs'
    kw['target'] = kw[key] + kw['target']

    conf.stlib(**kw)


def _rewrite_shlib_name(name):
    return name + '_shlib'


def _rewrite_stlib_name(name):
    return name + '_stlib'


def _rewrite_shlib_use(conf, use):
    mod_use = []
    for u in use:
        if u.endswith('_h'):
            # header use - keep as is
            mod_use.append(u)
        elif 'include' in u:
            # legacy global 'include' target - keep as is
            mod_use.append(u)
        else:
            # assume it is a library - supporting shlib
            mod_use.append(_rewrite_shlib_name(u))
    return mod_use


def _rewrite_shlib_use_to_lib(conf, use):
    mod_use = []
    for u in use:
        if u.endswith('_h'):
            pass
        elif 'include' in u:
            pass
        else:
            if u in ['csp', 'gscsp']:
                mod_use.append(u)
            else:
                # assume it is a library - suporting shlib
                mod_use.append('gs' + u)
    return mod_use


def create_deb_package(conf):
    while conf.gs_deb_spec:
        debpack.create_package(conf.gs_deb_spec.pop())


@conf
def gs_shlib(conf, **kw):
    """
    Build shared library, i.e. .so (if supported by target)
    Build target name ('name') is suffixed with _shlib, to avoid clashes.
    Build target output ('target') is prefixed with gs, to avoid clashes.
    Use relations specified by 'gs_use_shlib' is re-written with _shlib
    or prepended with gs and included as 'lib' if running in bitbake framework.
    """

    if gs_is_linux(conf):

        # suffix build-target name to avoid clashes
        kw['name'] = _rewrite_shlib_name(kw['target'])

        if gs_is_build_disabled(conf, ['shlib', kw['name']]):
            return  # build disabled

        if _has_no_source(kw):
            _create_header_target(conf, kw, kw['name'], kw['target'])
            return  # no source files

        # prefix target name to avoid clashes
        key = 'gs_prefix'
        if key not in kw:
            kw[key] = 'gs'
        kw['target'] = kw[key] + kw['target']

        # re-write use relation to fit what we are building
        key = 'gs_use_shlib'
        if key in kw:
            if gs_is_bitbake(conf):
                if 'lib' not in kw:
                    kw['lib'] = []
                kw['lib'].extend(_rewrite_shlib_use_to_lib(conf, kw[key]))

            if 'use' not in kw:
                kw['use'] = []
            kw['use'].extend(_rewrite_shlib_use(conf, kw[key]))

        if gs_debpack and not gs_is_derived_build(conf) and not gs_is_bitbake(conf) and not conf.options.disable_deb:
            gs_debpack.make_deb_package(conf, **kw)

        conf.shlib(**kw)


@conf
def gs_include(conf, **kw):
    """
    Export the includes and if it is used from the bitbake framework, then install
    the headers and any config headers.
    """
    public_include = kw['name'] + '_h'
    conf(export_includes=kw['includes'], name=public_include)

    if gs_is_bitbake(conf):
        # Install any config headers
        if 'config_header' in kw:
            for header in kw['config_header']:
                conf.install_files(os.path.join(conf.env.PREFIX, 'include'), header)

        # Sort the includes list
        kw['includes'].sort()

        root = []
        del_entries = []

        # Search for any double entries. E.g ['include', 'include/gs']
        for entry in kw['includes']:
            folders = entry.split('/')
            if root.count(folders[0]) == 0 or folders[1] == 'deprecated':
                root.append(folders[0])
            else:
                del_entries.append(entry)

        # Remove any double entries. So the same headers aren't installed different places
        for entry in del_entries:
            kw['includes'].remove(entry)

        # Set the files that should be installed
        for incl in kw['includes']:
            start_dir = conf.path.find_dir(incl)
            conf.install_files(os.path.join(conf.env.PREFIX, 'include'),
                               start_dir.ant_glob('**/*.h', excl=['**/internal']),
                               cwd=start_dir,
                               relative_trick=True)

    return public_include


@conf
def gs_is_derived_build(conf):
    """
    Return True if this target is being build as a dependency to another target.
    """
    if len(conf.stack_path) > 1:
        return True
    return False


@conf
def gs_python_bindings(conf, **kw):

    if gs_is_linux(conf):
        py2_target = kw['target'] + '_py2'
        py3_target = kw['target'] + '_py3'
        includes = []
        if 'includes' in kw:
            includes = kw['includes']

        # python3 bindings
        if conf.env.INCLUDES_PYTHON3:
            kw['includes'] = includes + conf.env.INCLUDES_PYTHON3
            kw['target'] = py3_target
            conf.gs_shlib(**kw)

        # python2 bindings
        if conf.env.INCLUDES_PYTHON2:
            kw['includes'] = includes + conf.env.INCLUDES_PYTHON2
            kw['target'] = py2_target
            conf.gs_shlib(**kw)


def options(conf):
    """
    Build tools options - invoked by load('gs_gcc') in wscript::options()
    """

    gr = conf.add_option_group('buildtools (gs_gcc) options')
    gr.add_option('--arch', action='store', default='x86_64',
                  help='(GS Build) Set architechture [x86, x86_64 (default), sam0, ucr3fp]')
    gr.add_option('--mcu', default=None, help='(GS Build) Set MCU identification (no default)')
    gr.add_option('--part', action='store', default=None, help='(GS Build) Set part identification (no default)')
    gr.add_option('--toolchain', action='store', default=None, help='(GS Build) Set toolchain prefix (no default)')
    gr.add_option('--warning-level', action='store', default='relaxed',
                  help='(GS Build) Set warning level [strict, relaxed (default)]')
    gr.add_option('--build', action='store', default='release',
                  help='(GS Build) Set build type [debug, release (default)]')
    gr.add_option('--build-disable', action='store', default='', help='Disable builds, e.g. stlib,shlib')

    if gs_debpack:
        gr.add_option('--disable-deb', action='store_true', help='Disable Debian packaging')

    # If running in the Bitbake environment, we decouple buildtools and just use waf
    gr.add_option('--bitbake', action='store_true', help='Set build environment to Bitbake')

    conf.load('eclipse')


def _expand_tag(env, key, tag):
    list = env[key]
    new = env[tag]
    if list and new:
        old = '${' + tag + '}'
        for i, e in enumerate(list):
            list[i] = str(e).replace(old, new)


def _get_and_set_compiler_settings(conf, preconfigured):
    settings = gs.buildtools.compiler_settings.get(part=conf.options.part, arch=conf.options.arch,
                                                   mcu=conf.options.mcu, warninglevel=conf.options.warning_level,
                                                   build=conf.options.build)
    conf.msg('Compiler settings', settings['name'])
    for key in settings:

        if (key == 'gs_arch') and ((not preconfigured) or (not conf.options.arch)):
            conf.options.arch = settings[key]
        if (key == 'gs_part') and ((not preconfigured) or (not conf.options.part)):
            conf.options.part = settings[key]
        if (key == 'gs_mcu') and ((not preconfigured) or (not conf.options.mcu)):
            conf.options.mcu = settings[key]
        if (key == 'toolchain') and ((not preconfigured) or (not conf.options.toolchain)):
            conf.options.toolchain = settings[key]

        keyUpper = key.upper()
        if not conf.env[keyUpper] and (key not in {'name', 'parent'}):
            conf.env[keyUpper] = settings[key]

    # expand only keywords we know, waf also uses ${..} syntax
    _expand_tag(conf.env, 'LINKFLAGS', 'GS_PART')
    _expand_tag(conf.env, 'LINKFLAGS', 'GS_MCU')
    _expand_tag(conf.env, 'CFLAGS', 'GS_PART')
    _expand_tag(conf.env, 'CFLAGS', 'GS_MCU')
    _expand_tag(conf.env, 'CXXFLAGS', 'GS_PART')
    _expand_tag(conf.env, 'ASFLAGS', 'GS_PART')
    _expand_tag(conf.env, 'AS', 'TOOLCHAIN')


__gs_gcc_configure_once = False


def configure(conf):
    """
    Build tools configure - invoked by load('gs_gcc') in wscript::configure()
    """

    # Only run configure once
    global __gs_gcc_configure_once
    if __gs_gcc_configure_once:
        return
    __gs_gcc_configure_once = True

    conf.env.GS_BUILD_DISABLE = [x.strip() for x in conf.options.build_disable.split(',')]

    preconfigured = False
    if conf.env.CC:
        # someone already configured - don't mess it up
        preconfigured = True

    # cmocka requires at least 1.9.7
    conf.check_waf_version(mini='2.0.9', maxi='2.0.9')

    conf.env.BITBAKE = False
    if conf.options.bitbake:
        conf.env.BITBAKE = True
        conf.env.GS_OS = 'linux'
    else:
        _get_and_set_compiler_settings(conf, preconfigured)

    conf.msg('options.arch', conf.options.arch)
    conf.msg('options.part', conf.options.part)
    conf.msg('options.toolchain', conf.options.toolchain)
    conf.msg('env.GS_ARCH', conf.env.GS_ARCH)
    conf.msg('env.GS_PART', conf.env.GS_PART)
    conf.msg('env.GS_OS', conf.env.GS_OS)

    if not preconfigured:
        prglist = {'CC': 'gcc', 'CXX': 'g++', 'AR': 'ar', 'AS': 'as', 'SIZE': 'size', 'OBJCOPY': 'objcopy', 'LD': 'ld'}

        if conf.options.toolchain:
            for key, value in prglist.iteritems():
                if not conf.env[key]:
                    conf.env[key] = conf.options.toolchain + value

        conf.load('gcc gxx gas')

        for key, value in prglist.iteritems():
            if conf.env[key]:
                conf.find_program(value, var=key)

    if conf.env.CC_VERSION:
        cc_version = '.'.join(conf.env.CC_VERSION)
        conf.msg('Compiler (CC) version', cc_version)
        if conf.env.EXPECTED_CC_VERSION and (conf.env.EXPECTED_CC_VERSION != cc_version):
            conf.msg('%s.c:0:0: warning: Expected compiler (CC) version' % __file__, conf.env.EXPECTED_CC_VERSION)

    if conf.gs_is_linux():
        # check for python development (pythonX-dev) and sets conf.env.INCLUDES_PYTHONX if found
        conf.check_cfg(package='python2', args='--cflags --libs', atleast_version='2.7', mandatory=False)
        conf.check_cfg(package='python3', args='--cflags --libs', atleast_version='3.5', mandatory=False)


_loaded_modules = []


def get_modules(conf=None, search_folder=None):
    """
    Return list of modules (e.g. dependencies)
    """

    # Don't look for modules if running in bitbake framework
    if gs_is_bitbake(conf):
        return []

    global _loaded_modules
    if len(_loaded_modules) == 0:
        if not conf or not gs_is_derived_build(conf):
            modules = gs.buildtools.util.get_existing_modules(search_folder=search_folder)
            for m in modules:
                if os.path.exists(os.path.join(m, 'wscript')):
                    _loaded_modules.append(m)
    return _loaded_modules


def get_path_to_module(conf, module_name, absolute=False):
    mod = get_modules(conf)
    for m in mod:
        if m.endswith(module_name):
            if absolute:
                return m
            else:
                return os.path.relpath(m, '.')
    return None


@conf
def gs_register_handler(conf, **kw):
    # Setup a handler function
    # handler: Name/identifier for the handler
    # function: The function which should be called by the handler
    # filepath: The file containing the function - relative to the wscript where it is setup
    if ('function' not in kw) or ('filepath' not in kw):
        raise Exception("Please specify all required arguments: [function, filepath]")
    if ('handler' not in kw):
        kw['handler'] = kw['function']
    # Inspect the stack to find the calling file. Waf adds a layer inbetween - hence the [2]
    wscript_path = inspect.stack()[2][1]
    lib_root_folder = os.path.dirname(wscript_path)
    handler = kw['handler']
    fp = os.path.abspath(os.path.join(lib_root_folder, kw['filepath']))
    if not os.path.exists(fp):
        if Logs.verbose > 0:
            Logs.warn("Filepath for handler does not exist! [filepath %s]" % fp)
        else:
            return
    func = kw['function']
    conf.env.append_unique('callback_handlers', handler)
    conf.env['GS_HANDLER_' + handler] = [fp, func]


@conf
def gs_call_handler(conf, **kw):

    # Call a handler function configured using the above setup function
    if 'handler' not in kw:
        raise Exception("Please specify required argument: handler")

    kw['ctx'] = conf

    handler = kw['handler']
    if 'GS_HANDLER_' + handler not in conf.env:
        return

    filepath, function = conf.env['GS_HANDLER_' + handler]

    if not os.path.exists(filepath):
        if Logs.verbose > 0:
            Logs.warn("Filepath for handler does not exist! [filepath %s]" % filepath)
        else:
            return

    modul = imp.load_source(handler, filepath)
    eval('modul.' + function)(**kw)  # call the handler function
