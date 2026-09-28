/* The chain check, whole: the name and the dates (policy.c) and then
 * the signatures and the anchor (chain.c) over the certificate store
 * (api.c, CERT_FAR). The policy half ran in the client until 2026-09-27,
 * when P-256 left the bank and made the room for it here; the client
 * now sends the host name and the clock in with the call and keeps no
 * DER walker of its own (REQUIREMENTS.md 5.15, 5.31).
 *
 * `host` and `now` are the bank's copies of the caller's strings, or
 * null for a check that skips that half (api.c, ck_api_chain). Returns
 * CHAIN_*; ck_api_rx is how many certificates the store held. */
#include <stdint.h>
#include "chain.h"

extern volatile uint8_t ck_api_rx;
extern uint16_t ck_cert_length;
void ck_cert_read(void *ctx, uint16_t off, uint8_t *dst, uint16_t n);

#define CHAIN_MAX 5              /* Libera's EC chain is four (5.10); more is cut, and then the top is no anchor */
static chain_ref refs[CHAIN_MAX];
static uint16_t off[CHAIN_MAX], len[CHAIN_MAX];

uint8_t ck_chain_run(const char *host, const char *now)
{
  uint8_t n, i, r;
  n = chain_split(ck_cert_read, 0, ck_cert_length, off, len, CHAIN_MAX);
  for (i = 0; i < n; i++) {
    refs[i].read = ck_cert_read;
    refs[i].ctx = (void *)(uintptr_t)off[i];       /* the read hook adds it: each certificate begins at zero */
    refs[i].len = len[i];
  }
  ck_api_rx = n;
  r = chain_policy(refs, n, host, now);            /* the name and the dates: no arithmetic */
  if (r != CHAIN_OK) return r;
  return chain_verify(refs, n);                    /* the signatures and the anchor: the seconds */
}
