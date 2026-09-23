/* IRC over TLS on the host, against a real server: the proof that the
 * whole stack holds together before any of it goes near the MEGA65.
 *
 *   build/host/tls_irc_host HOST[:PORT] [--ip A.B.C.D] [--nick NAME] [--quiet]
 *
 * Connects, runs the TLS 1.3 handshake with gemini's engine (answering
 * a HelloRetryRequest with a P-256 share, which is what OFTC wants:
 * REQUIREMENTS.md 5.12), verifies the server's certificate chain to an
 * anchor this client carries with the host name and today's date
 * checked, registers, answers the server's PINGs, waits for the 001
 * welcome, and leaves with a QUIT.
 *
 * Libera's round-robin serves ECDSA P-384 from some servers, which this
 * client recognises and cannot verify (5.10). The policy the user chose
 * is to reconnect rather than support P-384: when the chain is refused
 * as unsupported and no address was pinned, the name is resolved again
 * and the connection tried again, up to three times. --ip pins one
 * address and disables that.
 *
 * Exit 0 only if the chain verified AND the 001 arrived over it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "tls/tls.h"
#include "tls/chain.h"
#include "tls/roots.h"
#include "irc.h"
#include "clock.h"

static int quiet;

/* ---- received application data, a record at a time -------------------- */

static uint8_t rx[8192];
static uint16_t rx_len, rx_committed;
static uint8_t got_001, got_ping, quit_sent;
static char nick[17] = "m65tls";
static uint16_t lines_in;

static void send_line(int fd, tls *t, const char *s);

static void h_on_data(void *ctx, const uint8_t *p, uint16_t n)
{
  (void)ctx;
  if ((size_t)rx_len + n > sizeof rx) n = (uint16_t)(sizeof rx - rx_len);
  memcpy(rx + rx_len, p, n); rx_len = (uint16_t)(rx_len + n);
}

/* Data is handed over before its record's tag is checked, so nothing is
 * acted on until the record is kept; a record taken back is dropped to
 * the mark, exactly as the engine's contract says. */
static void h_on_record_end(void *ctx, uint8_t keep)
{
  (void)ctx;
  if (keep) rx_committed = rx_len; else rx_len = rx_committed;
}

static const tls_hooks hooks = { 0, h_on_data, h_on_record_end };

static int send_all(int fd, tls *t)
{
  while (tls_out_len(t)) {
    ssize_t k = write(fd, tls_out_data(t), tls_out_len(t));
    if (k <= 0) return 0;
    tls_out_consumed(t, (uint16_t)k);
  }
  return 1;
}

static void send_line(int fd, tls *t, const char *s)
{
  char line[520];
  size_t n = strlen(s);
  if (n > 510) n = 510;
  memcpy(line, s, n); memcpy(line + n, "\r\n", 2);
  if (!quiet) printf("  -> %s\n", s);
  if (!tls_write(t, (const uint8_t *)line, (uint16_t)(n + 2))) { fprintf(stderr, "tls_write: no room\n"); return; }
  send_all(fd, t);
}

/* Complete lines out of the committed bytes; the tail waits for more. */
static void handle_lines(int fd, tls *t)
{
  uint16_t start = 0, i;
  for (i = 0; i < rx_committed; i++) {
    if (rx[i] != '\n') continue;
    {
      static char line[IRC_LINE_MAX + 1];
      irc_msg m;
      uint16_t n = (uint16_t)(i - start);
      if (n && rx[i - 1] == '\r') n--;
      if (n > IRC_LINE_MAX) n = IRC_LINE_MAX;
      memcpy(line, rx + start, n); line[n] = 0;
      lines_in++;
      if (!quiet) printf("  <- %s\n", line);
      if (irc_parse(line, &m)) {
        if (irc_is(&m, "PING")) {
          char pong[520]; snprintf(pong, sizeof pong, "PONG :%s", m.trailing ? m.trailing : (m.nparams ? m.params[0] : ""));
          send_line(fd, t, pong); got_ping = 1;
        } else if (m.numeric == 1) {
          got_001 = 1;
        } else if (m.numeric == 433) {
          size_t k = strlen(nick); if (k < 15) { nick[k] = '2'; nick[k + 1] = 0; } else nick[k - 1]++;
          { char b[40]; snprintf(b, sizeof b, "NICK %s", nick); send_line(fd, t, b); }
        }
      }
    }
    start = (uint16_t)(i + 1);
  }
  if (start) {
    memmove(rx, rx + start, rx_len - start);
    rx_len = (uint16_t)(rx_len - start); rx_committed = (uint16_t)(rx_committed - start);
  }
}

/* ---- the certificate store, read through the kit ---------------------- */

typedef struct { uint16_t base; } slot;
static void store_read(void *ctx, uint16_t off, uint8_t *dst, uint16_t n)
{
  const slot *s = (const slot *)ctx;
  tk_cert_read((uint16_t)(s->base + off), dst, n);
}

static const char *chain_word(uint8_t r)
{
  switch (r) {
  case CHAIN_OK: return "verified to a carried anchor";
  case CHAIN_MALFORMED: return "MALFORMED: a certificate could not be walked";
  case CHAIN_BAD_SIG: return "BAD SIGNATURE";
  case CHAIN_NO_ANCHOR: return "sound but ends at no anchor we carry";
  case CHAIN_BAD_NAME: return "NOT FOR THIS HOST";
  case CHAIN_EXPIRED: return "EXPIRED or not yet valid";
  case CHAIN_UNSUPPORTED: return "signed with ECDSA, a scheme this client recognises but cannot verify (5.10)";
  }
  return "?";
}

/* One connection: resolve, connect, handshake, verify the chain.
 * Returns the socket on CHAIN_OK; -1 on a handshake failure, which is
 * final; -2 when the chain was refused, with *why saying how. */
static int connect_and_verify(const char *host, const char *port, const char *ip, const char *now, tls *t, uint8_t *out, uint16_t out_cap, uint8_t *why)
{
  static uint8_t in[2048];
  struct addrinfo hints, *ai;
  int fd;
  char shown[64];

  memset(&hints, 0, sizeof hints); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_INET;
  if (getaddrinfo(ip ? ip : host, port, &hints, &ai)) { fprintf(stderr, "%s: no such host\n", ip ? ip : host); return -1; }
  fd = socket(ai->ai_family, ai->ai_socktype, 0);
  if (connect(fd, ai->ai_addr, ai->ai_addrlen)) { perror("connect"); freeaddrinfo(ai); return -1; }
  getnameinfo(ai->ai_addr, ai->ai_addrlen, shown, sizeof shown, 0, 0, NI_NUMERICHOST);
  freeaddrinfo(ai);
  printf("connected to %s:%s at %s\n", host, port, shown);

  rx_len = rx_committed = 0;
  if (!tls_start(t, &hooks, host, out, out_cap)) { fprintf(stderr, "tls_start failed\n"); close(fd); return -1; }
  if (!send_all(fd, t)) { perror("write"); close(fd); return -1; }
  while (tls_state(t) == TLS_WAIT_SH || tls_state(t) == TLS_WAIT_HS) {
    ssize_t k = read(fd, in, sizeof in);
    if (k <= 0) { fprintf(stderr, "the server hung up during the handshake\n"); close(fd); return -1; }
    tls_in(t, in, (uint16_t)k);
    if (!send_all(fd, t)) { perror("write"); close(fd); return -1; }   /* a second ClientHello goes out here after a retry request */
  }
  if (tls_state(t) == TLS_VERIFYING) { if (!send_all(fd, t)) { perror("write"); close(fd); return -1; } tls_verify(t); }
  if (tls_state(t) != TLS_CONNECTED) {
    fprintf(stderr, "handshake: %s (alert %u, tls.c:%u, state %u)\n", tls_error_text(t), t->alert, t->error_line, t->state);
    close(fd); return -1;
  }
  printf("handshake: complete%s; the server's signature over it is %s (scheme %04x)\n",
#ifdef TLS_P256
         t->hrr_done ? " after a retry request, on P-256" : "",
#else
         "",
#endif
         t->sig_state == TLS_SIG_VERIFIED ? "verified" : t->sig_state == TLS_SIG_UNCHECKED ? "UNCHECKED, a scheme this client cannot verify" : "WRONG",
         t->sig_scheme);

  {
    uint16_t off[6], len[6], total = tk_cert_len();
    slot s[6];
    chain_ref ch[6];
    uint8_t n = chain_split(store_read, &(slot){0}, total, off, len, 6), j;
    printf("certificates: %u in %u bytes\n", n, total);
    for (j = 0; j < n; j++) { s[j].base = off[j]; ch[j].read = store_read; ch[j].ctx = &s[j]; ch[j].len = len[j]; }
    *why = chain_policy(ch, n, host, now);         /* the client's half first, as on the machine: the name and the dates cost nothing */
    if (*why == CHAIN_OK) *why = chain_verify(ch, n);   /* then the bank's: the signatures and the anchor */
    printf("chain: %s\n", chain_word(*why));
    if (*why == CHAIN_OK) {
      uint8_t hash[32]; x509_key k;
      x509_key_of(ch[n - 1].read, ch[n - 1].ctx, ch[n - 1].len, &k);
      x509_spki_hash(ch[n - 1].read, ch[n - 1].ctx, &k, hash);
      printf("anchor: %s\n", roots_name[roots_index(hash)]);
      return fd;
    }
  }
  tls_close(t); send_all(fd, t); close(fd);
  return -2;
}

int main(int argc, char **argv)
{
  static tls t;
  static uint8_t out[1600], in[2048];
  char host[128], port[8] = "6697", now[13];
  const char *ip = 0;
  int fd = -1, i, rc, attempt;
  uint8_t why = 0;
  time_t deadline;

  if (argc < 2) { fprintf(stderr, "usage: tls_irc_host HOST[:PORT] [--ip A.B.C.D] [--nick NAME] [--quiet]\n"); return 1; }
  { const char *c = strchr(argv[1], ':');
    if (c) { size_t n = (size_t)(c - argv[1]); if (n > sizeof host - 1) n = sizeof host - 1; memcpy(host, argv[1], n); host[n] = 0; snprintf(port, sizeof port, "%s", c + 1); }
    else snprintf(host, sizeof host, "%s", argv[1]); }
  for (i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--quiet")) quiet = 1;
    else if (!strcmp(argv[i], "--ip") && i + 1 < argc) ip = argv[++i];
    else if (!strcmp(argv[i], "--nick") && i + 1 < argc) snprintf(nick, sizeof nick, "%s", argv[++i]);
  }

  /* today, from the host, in the shape chain_valid_at compares */
  { time_t tt = time(0); struct tm *g = gmtime(&tt); clock_date d;
    d.yy = (uint8_t)(g->tm_year % 100); d.mm = (uint8_t)(g->tm_mon + 1); d.dd = (uint8_t)g->tm_mday;
    d.hh = (uint8_t)g->tm_hour; d.mi = (uint8_t)g->tm_min; d.ss = (uint8_t)g->tm_sec;
    clock_format(&d, now); }

  /* The policy for a server whose chain this client cannot verify: try
   * the name again, since a round-robin will usually hand out another
   * server. Three tries, then give up honestly. */
  for (attempt = 1; attempt <= 3; attempt++) {
    fd = connect_and_verify(host, port, ip, now, &t, out, sizeof out, &why);
    if (fd >= 0) break;
    if (fd == -1) return 1;
    if (why == CHAIN_UNSUPPORTED && !ip && attempt < 3) { printf("reconnecting: the round-robin may serve another server (try %d of 3)\n", attempt + 1); continue; }
    printf("not registering: the chain did not verify\n");
    return 2;
  }
  if (fd < 0) return 2;

  /* ---- IRC over it ---- */
  { char b[80];
    snprintf(b, sizeof b, "NICK %s", nick); send_line(fd, &t, b);
    snprintf(b, sizeof b, "USER %s 0 * :MEGA65 IRC over TLS, the host proof", nick); send_line(fd, &t, b); }
  deadline = time(0) + 60;
  while (tls_state(&t) == TLS_CONNECTED && time(0) < deadline) {
    fd_set r; struct timeval tv = { 1, 0 };
    FD_ZERO(&r); FD_SET(fd, &r);
    if (select(fd + 1, &r, 0, 0, &tv) > 0) {
      ssize_t k = read(fd, in, sizeof in);
      if (k <= 0) break;
      tls_in(&t, in, (uint16_t)k);
      handle_lines(fd, &t);
    }
    if (got_001 && !quit_sent) {
      send_line(fd, &t, "QUIT :the host proof is done");
      quit_sent = 1;
      deadline = time(0) + 3;                        /* a moment for the server's goodbye */
    }
  }
  if (tls_state(&t) == TLS_FAILED) fprintf(stderr, "transfer: %s (tls.c:%u)\n", tls_error_text(&t), t.error_line);
  tls_close(&t); send_all(fd, &t); close(fd);

  printf("registration: %s; %u lines received; server PINGs answered: %s\n",
         got_001 ? "001 welcome received" : "NO 001 within the deadline", lines_in, got_ping ? "yes" : "none arrived");
  if (got_001) printf("  nick %s\n", nick);
  rc = got_001 ? 0 : 3;
  printf("%s\n", rc == 0 ? "OK: IRC over TLS, the chain verified, registered and left cleanly" : "FAILED");
  return rc;
}
