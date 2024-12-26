# Install script for directory: /home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsgp4s.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsgp4s.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsgp4s.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/libsgp4s.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsgp4s.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsgp4s.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libsgp4s.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SGP4" TYPE FILE FILES
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/CoordGeodetic.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/CoordTopocentric.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/DateTime.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/DecayedException.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/Eci.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/Globals.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/Observer.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/OrbitalElements.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/SatelliteException.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/SGP4.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/SolarPosition.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/TimeSpan.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/TleException.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/Tle.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/Util.h"
    "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/Vector.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/canyval/Desktop/MIMAN/JDM/MIMAN_GS/lib/libsgp4/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
