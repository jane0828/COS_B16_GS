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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* CSP includes */
#include <csp/csp.h>
#include <csp/csp_error.h>
#include <csp/csp_endian.h>
#include <csp/csp_crc32.h>
#include <csp/csp_rtable.h>
#include <csp/interfaces/csp_if_lo.h>

#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_queue.h>
#include <csp/arch/csp_semaphore.h>
#include <csp/arch/csp_time.h>
#include <csp/arch/csp_malloc.h>

#include <csp/crypto/csp_hmac.h>
#include <csp/crypto/csp_xtea.h>

#include <csp/delay.h>
#include <csp/switch.h>

#include "csp_io.h"
#include "csp_port.h"
#include "csp_conn.h"
#include "csp_route.h"
#include "csp_promisc.h"
#include "csp_qfifo.h"
#include "transport/csp_transport.h"

/** CSP address of this node */
static uint8_t csp_my_address;

/* Hostname, model and build revision */
static const char *csp_hostname = NULL;
static const char *csp_model = NULL;
static const char *csp_revision = GIT_REV;
extern int csp_tx_wait_print_counter;

#ifdef CSP_USE_PROMISC
extern csp_queue_handle_t csp_promisc_queue;
#endif

int streaming = 0;

void set_now_streaming(int value)
{
	streaming = value;
	csp_log_protocol("FTP: Set Streaming Value : %d", streaming);

}

int get_now_streaming()
{
	return streaming;
}


void csp_set_address(uint8_t addr)
{
	csp_my_address = addr;
}

uint8_t csp_get_address(void)
{
	return csp_my_address;
}

void csp_set_hostname(const char *hostname)
{
	csp_hostname = hostname;
}

const char *csp_get_hostname(void)
{
	return csp_hostname;
}

void csp_set_model(const char *model)
{
	csp_model = model;
}

const char *csp_get_model(void)
{
	return csp_model;
}

void csp_set_revision(const char *revision)
{
	csp_revision = revision;
}

const char *csp_get_revision(void)
{
	return csp_revision;
}

int csp_init(unsigned char address) {

	int ret;

	/* Initialize CSP */
	csp_set_address(address);

	ret = csp_conn_init();
	if (ret != CSP_ERR_NONE)
		return ret;

	ret = csp_port_init();
	if (ret != CSP_ERR_NONE)
		return ret;

	ret = csp_qfifo_init();
	if (ret != CSP_ERR_NONE)
		return ret;

	/* Loopback */
	csp_iflist_add(&csp_if_lo);

	/* Register loopback route */
	csp_route_set(csp_get_address(), &csp_if_lo, CSP_NODE_MAC);

	/* Also register loopback as default, until user redefines default route */
	csp_route_set(CSP_DEFAULT_ROUTE, &csp_if_lo, CSP_NODE_MAC);

	return CSP_ERR_NONE;

}

csp_socket_t * csp_socket(uint32_t opts) {
	
	/* Validate socket options */
#ifndef CSP_USE_RDP
	if (opts & CSP_SO_RDPREQ) {
		csp_log_error("Attempt to create socket that requires RDP, but CSP was compiled without RDP support");
		return NULL;
	}
#endif

#ifndef CSP_USE_XTEA
	if (opts & CSP_SO_XTEAREQ) {
		csp_log_error("Attempt to create socket that requires XTEA, but CSP was compiled without XTEA support");
		return NULL;
	}
#endif

#ifndef CSP_USE_HMAC
	if (opts & CSP_SO_HMACREQ) {
		csp_log_error("Attempt to create socket that requires HMAC, but CSP was compiled without HMAC support");
		return NULL;
	} 
#endif

#ifndef CSP_USE_CRC32
	if (opts & CSP_SO_CRC32REQ) {
		csp_log_error("Attempt to create socket that requires CRC32, but CSP was compiled without CRC32 support");
		return NULL;
	} 
#endif
	
	/* Drop packet if reserved flags are set */
	if (opts & ~(CSP_SO_RDPREQ | CSP_SO_XTEAREQ | CSP_SO_HMACREQ | CSP_SO_CRC32REQ | CSP_SO_CONN_LESS)) {
		csp_log_error("Invalid socket option");
		return NULL;
	}

	/* Use CSP buffers instead? */
	csp_socket_t * sock = csp_conn_allocate(CONN_SERVER);
	if (sock == NULL)
		return NULL;

	/* If connectionless, init the queue to a pre-defined size
	 * if not, the user must init the queue using csp_listen */
	if (opts & CSP_SO_CONN_LESS) {
		sock->socket = csp_queue_create(CSP_CONN_QUEUE_LENGTH, sizeof(csp_packet_t *));
		if (sock->socket == NULL) {
			csp_close(sock);
			return NULL;
                }
	} else {
		sock->socket = NULL;
	}
	sock->opts = opts;

	return sock;

}

csp_conn_t * csp_accept(csp_socket_t * sock, uint32_t timeout) {

	if (sock == NULL)
		return NULL;

	if (sock->socket == NULL)
		return NULL;

	csp_conn_t * conn;
	if (csp_queue_dequeue(sock->socket, &conn, timeout) == CSP_QUEUE_OK)
		return conn;

	return NULL;

}



csp_packet_t * csp_read(csp_conn_t * conn, uint32_t timeout) {

	csp_packet_t * packet = NULL;
	timeout *= 2;

	if (conn == NULL || conn->state != CONN_OPEN)
		return NULL;

#ifdef CSP_USE_QOS
	int prio, event;
	if (csp_queue_dequeue(conn->rx_event, &event, timeout) != CSP_QUEUE_OK)
		return NULL;

	for (prio = 0; prio < CSP_RX_QUEUES; prio++)
		if (csp_queue_dequeue(conn->rx_queue[prio], &packet, 0) == CSP_QUEUE_OK)
			break;
#else
	if (csp_queue_dequeue(conn->rx_queue[0], &packet, timeout) != CSP_QUEUE_OK)
		return NULL;
#endif

	return packet;

}

int csp_send_direct(csp_id_t idout, csp_packet_t * packet, csp_iface_t * ifout, uint32_t timeout) {
	

	if (packet == NULL) {
		csp_log_error("csp_send_direct called with NULL packet");
		goto err;
	}

	if ((ifout == NULL) || (ifout->nexthop == NULL)) {
		csp_log_error("No route to host: %#08x", idout.ext);
		goto err;
	}
	if(!get_now_streaming())
	{
		switch_to_tx(idout.dst);
		usleep(switch_delay(idout.dst));
	}
// #ifdef CSP_USE_RDP
// 	if (idout.flags & CSP_FRDP) {
// 		int res = CSP_ERR_NONE;
// 		if(res = mim_send_cmp_all(false))
// 			return res;
// 	}
// #endif
	csp_log_packet("OUT: S %u, D %u, Dp %u, Sp %u, Pr %u, Fl 0x%02X, Sz %u VIA: %s",
		idout.src, idout.dst, idout.dport, idout.sport, idout.pri, idout.flags, packet->length, ifout->name);
	csp_tx_wait_print_counter = 0;
	/* Copy identifier to packet (before crc, xtea and hmac) */
	packet->id.ext = idout.ext;

#ifdef CSP_USE_PROMISC
	/* Loopback traffic is added to promisc queue by the router */
	if (idout.dst != csp_get_address() && idout.src == csp_get_address()) {
		packet->id.ext = idout.ext;
		csp_promisc_add(packet);
	}
#endif

	/* Only encrypt packets from the current node */
	if (idout.src == csp_get_address()) {
		/* Append HMAC */
		if (idout.flags & CSP_FHMAC) {
#ifdef CSP_USE_HMAC
			/* Calculate and add HMAC (does not include header for backwards compatability with csp1.x) */
			if (csp_hmac_append(packet, false) != 0) {
				/* HMAC append failed */
				csp_log_warn("HMAC append failed!");
				goto tx_err;
			}
#else
			csp_log_warn("Attempt to send packet with HMAC, but CSP was compiled without HMAC support. Discarding packet");
			goto tx_err;
#endif
		}

		/* Append CRC32 */
		if (idout.flags & CSP_FCRC32) {
#ifdef CSP_USE_CRC32
			/* Calculate and add CRC32 (does not include header for backwards compatability with csp1.x) */
			if (csp_crc32_append(packet, false) != 0) {
				/* CRC32 append failed */
				csp_log_warn("CRC32 append failed!");
				goto tx_err;
			}
#else
			csp_log_warn("Attempt to send packet with CRC32, but CSP was compiled without CRC32 support. Sending without CRC32r");
			idout.flags &= ~(CSP_FCRC32);
#endif
		}

		if (idout.flags & CSP_FXTEA) {
#ifdef CSP_USE_XTEA
			/* Create nonce */
			uint32_t nonce, nonce_n;
			nonce = (uint32_t)rand();
			nonce_n = csp_hton32(nonce);
			memcpy(&packet->data[packet->length], &nonce_n, sizeof(nonce_n));

			/* Create initialization vector */
			uint32_t iv[2] = {nonce, 1};

			/* Encrypt data */
			if (csp_xtea_encrypt(packet->data, packet->length, iv) != 0) {
				/* Encryption failed */
				csp_log_warn("Encryption failed! Discarding packet");
				goto tx_err;
			}

			packet->length += sizeof(nonce_n);
#else
			csp_log_warn("Attempt to send XTEA encrypted packet, but CSP was compiled without XTEA support. Discarding packet");
			goto tx_err;
#endif
		}
	}

	/* Store length before passing to interface */
	uint16_t bytes = packet->length;
	uint16_t mtu = ifout->mtu;

	if (mtu > 0 && bytes > mtu)
		goto tx_err;

	if ((*ifout->nexthop)(ifout, packet, timeout) != CSP_ERR_NONE)
		goto tx_err;

	// usleep(tx_delay(packet->length, idout.dst));
	ifout->tx++;
	ifout->txbytes += bytes;
	if(get_now_streaming() == 0 || get_now_streaming == 2)
	{
		switch_to_rx(idout.dst);
		usleep(switch_delay(idout.dst));
	}
	usleep(switch_delay(idout.dst));
	return CSP_ERR_NONE;
	

tx_err:
	ifout->tx_error++;
err:
	return CSP_ERR_TX;
}

int csp_send(csp_conn_t * conn, csp_packet_t * packet, uint32_t timeout) {

	int ret;

	if ((conn == NULL) || (packet == NULL) || (conn->state != CONN_OPEN)) {
		csp_log_error("Invalid call to csp_send");
		return 0;
	}

#ifdef CSP_USE_RDP
	if (conn->idout.flags & CSP_FRDP) {
		if (csp_rdp_send(conn, packet, timeout) != CSP_ERR_NONE) {
			csp_iface_t * ifout = csp_rtable_find_iface(conn->idout.dst);
			if (ifout != NULL)
				ifout->tx_error++;
			csp_log_warn("RDP send failed!");
			return 0;
		}
	}
#endif

	csp_iface_t * ifout = csp_rtable_find_iface(conn->idout.dst);
	ret = csp_send_direct(conn->idout, packet, ifout, timeout);

	return (ret == CSP_ERR_NONE) ? 1 : 0;

}

int csp_send_streaming(csp_conn_t * conn, csp_packet_t * packet, uint32_t timeout) {

	int ret;

	if ((conn == NULL) || (packet == NULL) || (conn->state != CONN_OPEN)) {
		csp_log_error("Invalid call to csp_send");
		return 0;
	}

	// printf("packet length : %d\n", packet->length);


#ifdef CSP_USE_RDP
	if (conn->idout.flags & CSP_FRDP) {
		if (csp_rdp_send(conn, packet, timeout) != CSP_ERR_NONE) {
			csp_iface_t * ifout = csp_rtable_find_iface(conn->idout.dst);
			if (ifout != NULL)
				ifout->tx_error++;
			csp_log_warn("RDP send failed!");
			return 0;
		}
	}
#endif

	csp_iface_t * ifout = csp_rtable_find_iface(conn->idout.dst);
	ret = csp_send_direct_streaming(conn->idout, packet, ifout, timeout);

	return (ret == CSP_ERR_NONE) ? 1 : 0;

}

int csp_send_prio(uint8_t prio, csp_conn_t * conn, csp_packet_t * packet, uint32_t timeout) {
	conn->idout.pri = prio;
	return csp_send(conn, packet, timeout);
}

int csp_transaction_persistent(csp_conn_t * conn, uint32_t timeout, void * outbuf, int outlen, void * inbuf, int inlen) {

	int size = (inlen > outlen) ? inlen : outlen;
	csp_packet_t * packet = csp_buffer_get(size);
	if (packet == NULL)
		return 0;

	/* Copy the request */
	if (outlen > 0 && outbuf != NULL)
		memcpy(packet->data, outbuf, outlen);
	packet->length = outlen;

	if (!csp_send(conn, packet, timeout)) {
		csp_buffer_free(packet);
		return 0;
	}

	/* If no reply is expected, return now */
	if (inlen == 0)
		return 1;

	packet = csp_read(conn, timeout);
	if (packet == NULL)
		return 0;

	if ((inlen != -1) && ((int)packet->length != inlen)) {
		csp_log_error("Reply length %u expected %u", packet->length, inlen);
		csp_buffer_free(packet);
		return 0;
	}

	memcpy(inbuf, packet->data, packet->length);
	int length = packet->length;
	csp_buffer_free(packet);
	return length;

}

int csp_transaction(uint8_t prio, uint8_t dest, uint8_t port, uint32_t timeout, void * outbuf, int outlen, void * inbuf, int inlen) {
    return csp_transaction2(prio, dest, port, timeout, outbuf, outlen, inbuf, inlen, 0);
}

int csp_transaction2(uint8_t prio, uint8_t dest, uint8_t port, uint32_t timeout, void * outbuf, int outlen, void * inbuf, int inlen, uint32_t opts) {

	csp_conn_t * conn = csp_connect(prio, dest, port, 0, opts);
	if (conn == NULL)
		return 0;

	int status = csp_transaction_persistent(conn, timeout, outbuf, outlen, inbuf, inlen);

	csp_close(conn);

	return status;

}

csp_packet_t * csp_recvfrom(csp_socket_t * socket, uint32_t timeout) {

	if ((socket == NULL) || (!(socket->opts & CSP_SO_CONN_LESS)))
		return NULL;

	csp_packet_t * packet = NULL;
	csp_queue_dequeue(socket->socket, &packet, timeout);

	return packet;

}

int csp_sendto(uint8_t prio, uint8_t dest, uint8_t dport, uint8_t src_port, uint32_t opts, csp_packet_t * packet, uint32_t timeout) {

	packet->id.flags = 0;

	if (opts & CSP_O_RDP) {
		csp_log_error("Attempt to create RDP packet on connection-less socket");
		return CSP_ERR_INVAL;
	}

	if (opts & CSP_O_HMAC) {
#ifdef CSP_USE_HMAC
		packet->id.flags |= CSP_FHMAC;
#else
		csp_log_error("Attempt to create HMAC authenticated packet, but CSP was compiled without HMAC support");
		return CSP_ERR_NOTSUP;
#endif
	}

	if (opts & CSP_O_XTEA) {
#ifdef CSP_USE_XTEA
		packet->id.flags |= CSP_FXTEA;
#else
		csp_log_error("Attempt to create XTEA encrypted packet, but CSP was compiled without XTEA support");
		return CSP_ERR_NOTSUP;
#endif
	}

	if (opts & CSP_O_CRC32) {
#ifdef CSP_USE_CRC32
		packet->id.flags |= CSP_FCRC32;
#else
		csp_log_error("Attempt to create CRC32 validated packet, but CSP was compiled without CRC32 support");
		return CSP_ERR_NOTSUP;
#endif
	}

	packet->id.dst = dest;
	packet->id.dport = dport;
	packet->id.src = csp_get_address();
	packet->id.sport = src_port;
	packet->id.pri = prio;

	csp_iface_t * ifout = csp_rtable_find_iface(dest);
	if (csp_send_direct(packet->id, packet, ifout, timeout) != CSP_ERR_NONE)
		return CSP_ERR_NOTSUP;
	
	return CSP_ERR_NONE;

}

int csp_sendto_reply(csp_packet_t * request_packet, csp_packet_t * reply_packet, uint32_t opts, uint32_t timeout) {
	if (request_packet == NULL)
		return CSP_ERR_INVAL;

	return csp_sendto(request_packet->id.pri, request_packet->id.src, request_packet->id.sport, request_packet->id.dport, opts, reply_packet, timeout);
}

int csp_send_direct_streaming(csp_id_t idout, csp_packet_t * packet, csp_iface_t * ifout, uint32_t timeout)
{
	if (packet == NULL) {
		csp_log_error("csp_send_direct called with NULL packet");
		goto err;
	}

	if ((ifout == NULL) || (ifout->nexthop == NULL)) {
		csp_log_error("No route to host: %#08x", idout.ext);
		goto err;
	}
	csp_log_packet("OUT: S %u, D %u, Dp %u, Sp %u, Pr %u, Fl 0x%02X, Sz %u VIA: %s",
		idout.src, idout.dst, idout.dport, idout.sport, idout.pri, idout.flags, packet->length, ifout->name);
	csp_tx_wait_print_counter = 0;
	/* Copy identifier to packet (before crc, xtea and hmac) */
	packet->id.ext = idout.ext;

#ifdef CSP_USE_PROMISC
	/* Loopback traffic is added to promisc queue by the router */
	if (idout.dst != csp_get_address() && idout.src == csp_get_address()) {
		packet->id.ext = idout.ext;
		csp_promisc_add(packet);
	}
#endif

	/* Only encrypt packets from the current node */
	if (idout.src == csp_get_address()) {
		/* Append HMAC */
		if (idout.flags & CSP_FHMAC) {
#ifdef CSP_USE_HMAC
			/* Calculate and add HMAC (does not include header for backwards compatability with csp1.x) */
			if (csp_hmac_append(packet, false) != 0) {
				/* HMAC append failed */
				csp_log_warn("HMAC append failed!");
				goto tx_err;
			}
#else
			csp_log_warn("Attempt to send packet with HMAC, but CSP was compiled without HMAC support. Discarding packet");
			goto tx_err;
#endif
		}

		/* Append CRC32 */
		if (idout.flags & CSP_FCRC32) {
#ifdef CSP_USE_CRC32
			/* Calculate and add CRC32 (does not include header for backwards compatability with csp1.x) */
			if (csp_crc32_append(packet, false) != 0) {
				/* CRC32 append failed */
				csp_log_warn("CRC32 append failed!");
				goto tx_err;
			}
#else
			csp_log_warn("Attempt to send packet with CRC32, but CSP was compiled without CRC32 support. Sending without CRC32r");
			idout.flags &= ~(CSP_FCRC32);
#endif
		}

		if (idout.flags & CSP_FXTEA) {
#ifdef CSP_USE_XTEA
			/* Create nonce */
			uint32_t nonce, nonce_n;
			nonce = (uint32_t)rand();
			nonce_n = csp_hton32(nonce);
			memcpy(&packet->data[packet->length], &nonce_n, sizeof(nonce_n));

			/* Create initialization vector */
			uint32_t iv[2] = {nonce, 1};

			/* Encrypt data */
			if (csp_xtea_encrypt(packet->data, packet->length, iv) != 0) {
				/* Encryption failed */
				csp_log_warn("Encryption failed! Discarding packet");
				goto tx_err;
			}

			packet->length += sizeof(nonce_n);
#else
			csp_log_warn("Attempt to send XTEA encrypted packet, but CSP was compiled without XTEA support. Discarding packet");
			goto tx_err;
#endif
		}
	}

	/* Store length before passing to interface */
	uint16_t bytes = packet->length;
	uint16_t mtu = ifout->mtu;

	if (mtu > 0 && bytes > mtu)
		goto tx_err;

	if ((*ifout->nexthop)(ifout, packet, timeout) != CSP_ERR_NONE)
		goto tx_err;

	// usleep(tx_delay(packet->length, idout.dst));
	ifout->tx++;
	ifout->txbytes += bytes;
	return CSP_ERR_NONE;
	

tx_err:
	ifout->tx_error++;
err:
	return CSP_ERR_TX;
}