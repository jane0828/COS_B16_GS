#ifndef LIBGSCSP_INCLUDE_GS_CSP_DRIVERS_CAN_CAN_H
#define LIBGSCSP_INCLUDE_GS_CSP_DRIVERS_CAN_CAN_H
/* Copyright (c) 2013-2018 GomSpace A/S. All rights reserved. */
/**
   @file

   GomSpace CAN API for CSP.
*/

#include <gs/util/error.h>
#include <csp/csp_types.h>
#include <csp/interfaces/csp_if_can.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
   Default CAN interface name.
*/
#define GS_CSP_CAN_DEFAULT_IF_NAME "CAN"

/**
   Initialize CSP for CAN device.

   @param[in] device CAN device
   @param[in] csp_addr CSP address.
   @param[in] mtu MTU, normally CSP_CAN_MTU.
   @param[in] name name of CSP interface, if NULL #GS_CSP_CAN_DEFAULT_IF_NAME will be used.
   @param[in] set_default_route if \a true, the device will be set as default route.
   @param[out] csp_if the added CSP interface.
   @return #GS_ERROR_EXIST if device already exists.
   @return_gs_error_t
*/
gs_error_t gs_csp_can_init2(uint8_t device, uint8_t csp_addr, uint32_t mtu, const char * name, bool set_default_route, csp_iface_t ** csp_if);

/**
   Initialize CSP for CAN device.

   @param[in] device CAN device
   @param[in] csp_addr CSP address.
   @param[in] mtu MTU, normally CSP_CAN_MTU.
   @param[in] name name of CSP interface, if NULL #GS_CSP_CAN_DEFAULT_IF_NAME will be used.
   @param[out] csp_if the added CSP interface.
   @return #GS_ERROR_EXIST if device already exists.
   @return_gs_error_t
   @see gs_csp_can_init2()
*/
gs_error_t gs_csp_can_init(uint8_t device, uint8_t csp_addr, uint32_t mtu, const char * name, csp_iface_t ** csp_if);

#ifdef __cplusplus
}
#endif
#endif
