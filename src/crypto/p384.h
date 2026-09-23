/* ECDSA over NIST P-384 with a SHA-384 digest (`ecdsa-with-SHA384`):
 * what Let's Encrypt's elliptic-curve hierarchy signs with, from ISRG
 * Root X2 down to the leaf, and therefore what verifying an EC chain
 * from Libera needs (REQUIREMENTS.md 5.10, 5.22).
 *
 * Verification only: this client proves other people's signatures and
 * makes none of its own, so there is no key generation and no agreement
 * here. The arithmetic is `p256.c`'s, on 24-limb numbers instead of 16:
 * the same Jacobian doubling and addition, the same Shamir loop, the
 * same Montgomery context. Constant time is not a goal (a signature and
 * a public key are public).
 *
 * The scratch is this file's own and 1,488 bytes wide, because
 * `mp_scratch` holds 512 limbs and P-384 wants 744: see 5.22 before
 * putting this in a bank. */
#ifndef P384_H
#define P384_H

#include <stdint.h>

/* `pub` is the uncompressed point's X then Y, 48 bytes each; `hash` is
 * a SHA-384 digest; `r` and `s` are the signature's halves, 48 bytes
 * each, big-endian and zero-padded. 1 if the signature is good. */
uint8_t p384_verify(const uint8_t pub[96], const uint8_t hash[48], const uint8_t r[48], const uint8_t s[48]);

#endif
