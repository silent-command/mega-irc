/* SHA-384 (FIPS 180-4): SHA-512's compression with another initial
 * value and the first 48 bytes of the result. It is here because Let's
 * Encrypt's elliptic-curve hierarchy signs `ecdsa-with-SHA384` from the
 * root down (REQUIREMENTS.md 5.22), and in a file of its own because
 * `sha512.c` is the Gemini client's, byte-identical, and stays so. The
 * context is SHA-512's: every block, every round and every buffer is
 * shared, so this costs the initial value and a truncation. */
#ifndef SHA384_H
#define SHA384_H

#include <stdint.h>
#include "crypto.h"

void sha384_init(sha512_ctx *c);
/* the same bytes-in as SHA-512 */
#define sha384_update sha512_update
void sha384_final(sha512_ctx *c, uint8_t out[48]);
void sha384(const uint8_t *p, uint16_t n, uint8_t out[48]);

#endif
