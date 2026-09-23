#include "pkcs1.h"
#include "mp.h"

#define ctx mp_shared

/* The DigestInfo that wraps a SHA-256 hash, RFC 8017 9.2:
 *   SEQUENCE { SEQUENCE { OID 2.16.840.1.101.3.4.2.1, NULL }, OCTET STRING }
 * These nineteen bytes are not recited from memory: they are what the
 * certificates Libera and OFTC actually serve recover to, read back out
 * of a real signature with the issuer's key (5.5). */
static const uint8_t sha256_digestinfo[19] = {
  0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
  0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};

uint8_t rsa_pkcs1_sha256_verify(const uint8_t *n, uint16_t nlen, const uint8_t *e, uint16_t elen,
                                const uint8_t *sig, uint16_t siglen, const uint8_t hash[32])
{
  mp_limb *s = mp_scratch, *m = mp_scratch + MP_MAX_LIMBS;
  uint8_t *em = (uint8_t *)ctx.rr;                 /* R^2 is spent once the exponentiation is done */
  uint16_t i, j;

  /* The same shape rsa.c insists on: whole 32-bit limbs and the top bit
   * set, which is what the machine's multiplier wants (gemini 5.12). */
  if (nlen < 128 || nlen > 2 * MP_MAX_LIMBS || (nlen & 3) || siglen != nlen || !(n[0] & 0x80)) return 0;
  if (elen == 0) return 0;

  mp_init(&ctx, n, nlen);
  mp_from_be(&ctx, s, sig, siglen);
  if (mp_cmp(&ctx, s, ctx.m) >= 0) return 0;       /* a signature at or above the modulus */
  mp_to_mont(&ctx, s, s);
  mp_mont_exp(&ctx, m, s, e, elen);
  mp_from_mont(&ctx, m, m);
  mp_to_be(&ctx, m, em, nlen);

  /* EM = 0x00 0x01 || FF..FF || 0x00 || DigestInfo || H */
  if (em[0] != 0x00 || em[1] != 0x01) return 0;
  for (i = 2; i < nlen && em[i] == 0xff; i++) ;
  if (i < 2 + 8) return 0;                         /* RFC 8017: at least eight bytes of padding */
  if (i >= nlen || em[i] != 0x00) return 0;
  i++;
  /* The hash must end exactly at the end: nothing may trail it, which is
   * the check a lax verifier leaves out and a forger looks for. */
  if ((uint16_t)(nlen - i) != (uint16_t)(sizeof sha256_digestinfo + 32)) return 0;
  for (j = 0; j < sizeof sha256_digestinfo; j++)
    if (em[i + j] != sha256_digestinfo[j]) return 0;
  i = (uint16_t)(i + sizeof sha256_digestinfo);
  for (j = 0; j < 32; j++)
    if (em[i + j] != hash[j]) return 0;
  return 1;
}
