/* RSASSA-PKCS1-v1_5 verification with SHA-256 (RFC 8017 8.2.2).
 *
 * The family already has RSA-PSS, because that is the only RSA scheme
 * TLS 1.3 uses for CertificateVerify. Certificate signatures are a
 * different matter: every certificate Libera and OFTC serve is signed
 * sha256WithRSAEncryption, which is v1.5 (REQUIREMENTS.md 5.5). So
 * verifying a chain needs this, and nothing else in the family has it.
 *
 * New here, so it is a new file: the crypto copied from ../mega-gemini
 * stays identical to its origin.
 *
 * One modular exponentiation over mp.h, exactly as rsa.c's PSS does,
 * then a padding check instead of PSS's. Moduli up to 4096 bits,
 * verification only. Needs about 3 KB of stack, as rsa_pss_verify does.
 */
#ifndef PKCS1_H
#define PKCS1_H

#include <stdint.h>

/* n and e big-endian without leading zeros; sig of n's length; hash the
 * SHA-256 of the signed bytes. 1 if valid, 0 otherwise. */
uint8_t rsa_pkcs1_sha256_verify(const uint8_t *n, uint16_t nlen, const uint8_t *e, uint16_t elen,
                                const uint8_t *sig, uint16_t siglen, const uint8_t hash[32]);

#endif
