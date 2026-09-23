#include "mega65/memory.h"
#include "tls/tls.h"
#include "tls/chain.h"
#include "netutil.h"
#include "ui.h"
#include "rnd.h"
#include "m65_tlskit.h"
#include "lowram.h"
#include "conn.h"

#define FIRST_BYTE_FRAMES 1500     /* 30 s to the first byte: the handshake's arithmetic is inside this */
/* the receive buffer and the send buffer are in low RAM (lowram.h); the
 * send buffer holds a ClientHello (two, after a retry request), the
 * Finished, then one IRC line at a time as a record: 512 + 5 + 1 + 16 */
#define in LOW_IN
#define IN_CAP LOW_IN_CAP
#define out LOW_OUT
#define OUT_CAP LOW_OUT_CAP
#define CHAIN_MAX 5                /* as the bank's (chain_api.c) */

#define t (*(tls *)LOW_TLS)            /* 809 bytes, in the ROM's old screen page (lowram.h) */
static uint8_t tls_on;
static const char *fail_why;
#define fail_text LOW_FAIL
uint8_t conn_chain;
uint16_t conn_secs;
uint8_t conn_certs;
uint8_t conn_retry;
uint8_t conn_cancelled;
void (*conn_on_data)(const unsigned char *p, unsigned int n);

/* The engine's error with the tls.c line that raised it: the diagnosis
 * that found every host-side fault (gemini 5.2). */
static const char *tls_fail(void)
{
  char *p = fail_text, *e = fail_text + LOW_FAIL_CAP - 1;
  p = ui_cat(p, fail_text + 56, tls_error_text(&t));
  p = ui_cat_num(ui_cat(p, e, " (tls.c:"), e, t.error_line);
  p = ui_cat(p, e, ")");
  if (t.error == TLS_E_SIGNATURE) { p = ui_cat(p, e, " why "); p = ui_cat_num(p, e, tk_verify_reason); }
  if (t.error == TLS_E_TAG) p = ui_cat_num(ui_cat(p, e, " rec "), e, t.rec_index);
  *p = 0;
  return fail_text;
}

/* Application data as decrypted, before the record's tag has been
 * checked: fed to the client at once. A record whose tag then fails ends
 * the connection (TLS_E_TAG), so what a forger could achieve is at most
 * one record's worth of lines on the screen before the link drops; a
 * held-back copy of every record would cost a buffer the size of the
 * largest record, 16 KB, and this client has no such room. Said here so
 * the trade is a known one (5.15). */
static void on_data(void *ctx, const uint8_t *p, uint16_t n) { (void)ctx; if (conn_on_data) conn_on_data(p, n); }
static void on_record_end(void *ctx, uint8_t keep) { (void)ctx; (void)keep; }
static const tls_hooks hooks = { 0, on_data, on_record_end };

/* Sends what the engine has queued; 0 if the connection is gone. */
static uint8_t push_out(void)
{
  unsigned int n;
  while (tls_out_len(&t)) {
    n = net_send(tls_out_data(&t), tls_out_len(&t));
    if (n) tls_out_consumed(&t, (uint16_t)n);
    else { net_poll(); if (!net_alive()) return 0; }
  }
  return 1;
}

/* Pumps the handshake until `done` or a bound; returns 0 on the bound
 * or a cancel, with fail_why set. */
static uint8_t pump(uint8_t (*done)(void), unsigned int bound)
{
  unsigned int n;
  net_frames = 0;
  for (;;) {
    net_poll();
    n = net_recv(in, IN_CAP);
    if (n) { tls_in(&t, in, (uint16_t)n); net_frames = 0; rnd_stir((uint8_t)n); }
    if (!push_out()) { fail_why = "the connection was lost"; return 0; }
    if (tls_state(&t) == TLS_FAILED) { fail_why = tls_fail(); return 0; }
    if (done()) return 1;
    if (!n && !net_alive()) return 1;              /* the peer is gone: the caller judges */
    if (net_frames > bound) { fail_why = "no answer from the server"; return 0; }
    if (ui_key() == KEY_STOP) { fail_why = "cancelled"; conn_cancelled = 1; return 0; }
  }
}

static uint8_t handshake_done(void) { return (uint8_t)(tls_state(&t) != TLS_WAIT_SH && tls_state(&t) != TLS_WAIT_HS); }

/* ---- the chain: the client's half over the bank's store -------------------- */

static void store_read(void *ctx, uint16_t off, uint8_t *dst, uint16_t n)
{
  tk_cert_read((uint16_t)((uint16_t)(uintptr_t)ctx + off), dst, n);
}

static const char *chain_word(uint8_t r)
{
  switch (r) {
  case CHAIN_MALFORMED: return "a certificate could not be read";
  case CHAIN_BAD_SIG: return "a certificate's signature is WRONG";
  case CHAIN_NO_ANCHOR: return "no carried root signs this chain";
  case CHAIN_BAD_NAME: return "the certificate is not for this host";
  case CHAIN_EXPIRED: return "a certificate is out of date (or the clock is)";
  case CHAIN_UNSUPPORTED: return "an ECDSA chain, which this client cannot check";
  }
  return "the chain could not be checked";
}

static uint8_t chain_check(const char *host, const char *now)
{
  uint16_t off[CHAIN_MAX], len[CHAIN_MAX];
  chain_ref ch[CHAIN_MAX];
  uint8_t n, i, r;
  n = chain_split(store_read, 0, tk_cert_len(), off, len, CHAIN_MAX);
  conn_certs = n;
  for (i = 0; i < n; i++) {
    ch[i].read = store_read;
    ch[i].ctx = (void *)(uintptr_t)off[i];         /* the hook adds it: each certificate begins at zero */
    ch[i].len = len[i];
  }
  r = chain_policy(ch, n, host, now);              /* the name and the dates: free */
  if (r != CHAIN_OK) return r;
  return tk_chain();                               /* the signatures and the anchor: the bank's arithmetic */
}

/* ---- the connection ------------------------------------------------------ */

uint8_t conn_open(const char *host, const unsigned char ip[4], unsigned int port, uint8_t use_tls, const char *now, const char **err)
{
  uint16_t frames;
  conn_chain = 255; conn_secs = 0; fail_why = 0; conn_retry = 0; conn_cancelled = 0;
  tls_on = 0;
  /* A refused connection is this server saying no, and another of its
   * addresses may still say yes; a server that never answers is worth
   * one more try too. Neither is a reason to give up on the name (5.21). */
  if (!net_connect(ip, port, err)) { conn_retry = 1; return 0; }
  if (!use_tls) return 1;
  tls_on = 1;
  ui_status("the handshake...", 0);
  if (!tls_start(&t, &hooks, host, out, OUT_CAP)) { net_abort(); *err = "the send buffer is too small"; return 0; }
  if (!pump(handshake_done, FIRST_BYTE_FRAMES) || tls_state(&t) != TLS_VERIFYING) {
    net_abort(); *err = fail_why ? fail_why : "the server hung up during the handshake";
    conn_retry = (uint8_t)(t.error != TLS_E_ALERT);   /* an alert is the server's considered no; a silence or a hang-up is not */
    return 0;
  }
  /* the client's Finished goes out first, then the signature is checked:
   * the seconds of arithmetic run against the server's registration
   * timer, not its handshake timer (gemini 5.6); nothing else is sent
   * until it and the chain have passed */
  if (!push_out()) { net_abort(); *err = "the connection was lost"; return 0; }
  ui_status("the server's signature...", 0);
  tls_verify(&t);
  if (tls_state(&t) != TLS_CONNECTED) { net_abort(); *err = tls_fail(); return 0; }
  ui_status("the certificate chain...", 0);
  conn_chain = chain_check(host, now);
  if (conn_chain != CHAIN_OK) {
    net_abort(); *err = chain_word(conn_chain);
    conn_retry = (uint8_t)(conn_chain == CHAIN_UNSUPPORTED);   /* another of this name's servers may be one we can check (5.10, 5.21) */
    return 0;
  }
  frames = (uint16_t)(tk_frames_keys + tk_frames_verify + tk_frames_chain);   /* at 50 a second, as whole seconds */
  while (frames >= 50) { frames = (uint16_t)(frames - 50); conn_secs++; }     /* a loop, not a division (gemini 5.15) */
  *err = 0;
  return 1;
}

void conn_pump(void)
{
  unsigned int n;
  net_poll();
  n = net_recv(in, IN_CAP);
  if (!n) return;
  rnd_stir((uint8_t)n);
  if (tls_on) { tls_in(&t, in, (uint16_t)n); push_out(); }
  else if (conn_on_data) conn_on_data(in, n);
}

uint8_t conn_send(const unsigned char *p, unsigned int n)
{
  if (!tls_on) {
    unsigned int sent;
    while (n) {
      sent = net_send(p, n);
      if (sent) { p += sent; n -= sent; }
      else { net_poll(); if (!net_alive()) return 0; }
    }
    return 1;
  }
  if (tls_state(&t) != TLS_CONNECTED) return 0;
  if (!push_out()) return 0;                       /* the buffer empty: the record goes at its start */
  if (!tls_write(&t, p, (uint16_t)n)) return 0;    /* longer than the buffer: never, for a 512-byte line */
  return push_out();
}

uint8_t conn_alive(void)
{
  if (!net_alive()) return 0;
  return (uint8_t)(!tls_on || tls_state(&t) == TLS_CONNECTED);
}

void conn_close(void)
{
  if (tls_on && tls_state(&t) == TLS_CONNECTED) { tls_close(&t); push_out(); }
  net_close();
}

void conn_abort(void)
{
  tls_on = 0;
  net_abort();
}
