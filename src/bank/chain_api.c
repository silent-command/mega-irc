/* The chain entry, in the top window beside chain.c and der.c: the
 * signatures and the anchor over the certificate store (api.c, CERT_FAR),
 * which is the bank's half of the check; the client has already refused
 * a wrong name or date with its own (policy.c; REQUIREMENTS.md 5.15).
 *
 * {block}: A = CHAIN_*, X = how many certificates the store held; the
 * frames the arithmetic took into the caller's block at +14, as the key
 * agreement reports its own (gemini 5.12). */
#include <stdint.h>
#include "chain.h"

extern volatile uint8_t ck_api_ra, ck_api_rx;
extern uint16_t ck_cert_length;
void ck_cert_read(void *ctx, uint16_t off, uint8_t *dst, uint16_t n);
void ck_frames_start(void);
void ck_frames_done(void);

#define CHAIN_MAX 5              /* Libera's EC chain is four (5.10); more is cut, and then the top is no anchor */
static chain_ref refs[CHAIN_MAX];
static uint16_t off[CHAIN_MAX], len[CHAIN_MAX];

void ck_api_chain(void)
{
  uint8_t n, i;
  ck_frames_start();
  n = chain_split(ck_cert_read, 0, ck_cert_length, off, len, CHAIN_MAX);
  for (i = 0; i < n; i++) {
    refs[i].read = ck_cert_read;
    refs[i].ctx = (void *)(uintptr_t)off[i];       /* the read hook adds it: each certificate begins at zero */
    refs[i].len = len[i];
  }
  ck_api_rx = n;
  ck_api_ra = chain_verify(refs, n);
  ck_frames_done();
}
