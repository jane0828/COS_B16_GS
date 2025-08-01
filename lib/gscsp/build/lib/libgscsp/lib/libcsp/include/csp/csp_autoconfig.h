/* WARNING! All changes made to this file will be lost! */

#ifndef W_INCLUDE_CSP_CSP_AUTOCONFIG_H_WAF
#define W_INCLUDE_CSP_CSP_AUTOCONFIG_H_WAF

#define GIT_REV "8aeccf5f"
/* #undef CSP_FREERTOS */
#define CSP_POSIX 1
/* #undef CSP_WINDOWS */
/* #undef CSP_MACOSX */
#define CSP_DEBUG 1
#define CSP_USE_RDP 1
#define CSP_USE_CRC32 1
#define CSP_USE_HMAC 1
/* #undef CSP_USE_XTEA */
/* #undef CSP_USE_PROMISC */
#define CSP_USE_QOS 1
#define CSP_USE_DEDUP 1
/* #undef CSP_USE_INIT_SHUTDOWN */
/* #undef CSP_USE_CAN */
/* #undef CSP_USE_I2C */
#define CSP_USE_KISS 1
/* #undef CSP_USE_ZMQHUB */
#define CSP_CONN_MAX 10
#define CSP_CONN_QUEUE_LENGTH 100
#define CSP_FIFO_INPUT 100
#define CSP_MAX_BIND_PORT 31
#define CSP_RDP_MAX_WINDOW 20
#define CSP_PADDING_BYTES 8
#define CSP_CONNECTION_SO 64
#define CSP_LOG_LEVEL_DEBUG 1
#define CSP_LOG_LEVEL_INFO 1
#define CSP_LOG_LEVEL_WARN 1
#define CSP_LOG_LEVEL_ERROR 1
#define CSP_LITTLE_ENDIAN 1
/* #undef CSP_BIG_ENDIAN */
#define CSP_HAVE_STDBOOL_H 1
#define LIBCSP_VERSION "1.5"

#endif /* W_INCLUDE_CSP_CSP_AUTOCONFIG_H_WAF */
