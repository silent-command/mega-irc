/* chain.c's buffers as the bank has them, in place of its statics
 * (chain.c, CHAIN_BANK; REQUIREMENTS.md 5.15). Each is a buffer the bank
 * already owns and that is idle while a chain is checked, and each is a
 * named .bss buffer the DMA may land in, never a local (gemini 5.5):
 *
 *   the issuer's modulus in the lower half of the multiplier's scratch
 *   and the child's signature in the upper, the way api.c keeps `sig`
 *   there: pkcs1.c copies the modulus out with mp_init before it reads
 *   the signature into the lower half, and writes the upper half only
 *   after that (gemini 5.12);
 *   the exponent in `small`, the whole of which is free here;
 *   the issuer's key at stage + 130, where verify keeps its own. */
#include "mp.h"
extern uint8_t ck_stage[256], ck_small[64];
#define issuer_n ((uint8_t *)mp_scratch)
#define child_sig ((uint8_t *)(mp_scratch + 256))
#define issuer_e ck_small
#define key (*(x509_key *)(ck_stage + 130))

/* The key and the pin of a certificate of the store, asked of api.c
 * rather than of x509.c directly: the store has one reader, and when
 * every call to x509_key_of passes it as a constant the link keeps one
 * copy specialised for it. A call from this window through the chain_ref's
 * pointer cost a second, generic copy of 1.2 KB (5.15). */
uint8_t ck_key_of(uint16_t base, uint16_t len, x509_key *k);
void ck_spki_hash(uint16_t base, const x509_key *k, uint8_t out[32]);
#define key_of(r, k) ck_key_of((uint16_t)(uintptr_t)(r)->ctx, (r)->len, k)
#define spki_hash(r, k, out) ck_spki_hash((uint16_t)(uintptr_t)(r)->ctx, k, out)
