/* One connection to an IRC server, plain or over TLS, behind one shape:
 * the client moves lines and never sees a record. Over TLS the handshake
 * runs here, the server's signature is checked by the bank, and the
 * certificate chain is checked in two halves, the name and the dates
 * here and the signatures in the bank (chain.h; REQUIREMENTS.md 5.15). */
#ifndef CONN_H
#define CONN_H
#include <stdint.h>

/* Connects to ip:port; `host` is the name dialled, for the TLS server
 * name and the certificate. `now` is the clock as twelve digits for the
 * dates, or null to leave them unchecked. 0 with *err on failure, and
 * then conn_chain says how the chain fared if that is what failed
 * (CHAIN_*; 255 when the chain was never reached). */
uint8_t conn_open(const char *host, const unsigned char ip[4], unsigned int port, uint8_t use_tls, const char *now, const char **err);
extern uint8_t conn_chain;
extern uint16_t conn_secs;       /* seconds the handshake's arithmetic took: the keys, the signature, the chain */
extern uint8_t conn_certs;       /* how many certificates the server sent */
extern uint8_t conn_retry;       /* the failure was this server's, not this name's: another address is worth a try (5.21) */
extern uint8_t conn_cancelled;   /* the failure was RUN/STOP: the user wants out, not another server (5.21) */

/* Polls the network and delivers what arrived, as plaintext, through
 * conn_on_data; the caller's loop calls it every pass. */
void conn_pump(void);
extern void (*conn_on_data)(const unsigned char *p, unsigned int n);

/* n bytes out; 0 if the connection is gone. */
uint8_t conn_send(const unsigned char *p, unsigned int n);
uint8_t conn_alive(void);
/* A polite end: close_notify if TLS, then the socket. */
void conn_close(void);
/* Drop it, at once. */
void conn_abort(void);

#endif
