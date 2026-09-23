#include "sha384.h"

/* FIPS 180-4 5.3.4: the fractional parts of the square roots of the
 * 9th to 16th primes, where SHA-512 takes the 1st to 8th. */
void sha384_init(sha512_ctx *c)
{
  static const uint64_t iv[8] = {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL };
  uint8_t i;
  for (i = 0; i < 8; i++) c->h[i] = iv[i];
  c->len = 0; c->total = 0;
}

/* The padding, the length and the rounds are SHA-512's to the last bit;
 * SHA-384 is that result cut to 48 bytes. */
void sha384_final(sha512_ctx *c, uint8_t out[48])
{
  uint8_t full[64], i;
  sha512_final(c, full);
  for (i = 0; i < 48; i++) out[i] = full[i];
}

void sha384(const uint8_t *p, uint16_t n, uint8_t out[48])
{
  sha512_ctx c;
  sha384_init(&c);
  sha384_update(&c, p, n);
  sha384_final(&c, out);
}
