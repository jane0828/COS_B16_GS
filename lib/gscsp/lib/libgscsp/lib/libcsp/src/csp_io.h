/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 GomSpace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk) 

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _CSP_IO_H_
#define _CSP_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <csp/csp.h>

/**
 * Function to transmit a frame without an existing connection structure.
 * This function is used for stateless transmissions
 * @param idout 32bit CSP identifier
 * @param packet pointer to packet,
 * @param ifout pointer to output interface
 * @param timeout a timeout to wait for TX to complete. NOTE: not all underlying drivers supports flow-control.
 * @return returns 1 if successful and 0 otherwise. you MUST free the frame yourself if the transmission was not successful.
 */
void set_now_streaming(int value);
int get_now_streaming();
int csp_send_direct(csp_id_t idout, csp_packet_t * packet, csp_iface_t * ifout, uint32_t timeout);
int csp_send_direct_streaming(csp_id_t idout, csp_packet_t * packet, csp_iface_t * ifout, uint32_t timeout);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _CSP_IO_H_
