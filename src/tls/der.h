/* Walking DER, for the certificate chain: the element walker, the
 * Certificate's outer shape, and the fields of its TBSCertificate. One
 * certificate is selected at a time and the walkers take offsets alone,
 * reading it through an x509_read hook as x509.c does, so it may sit in
 * far memory on the machine and in a plain buffer on the host.
 *
 * Shared by the two halves of the chain check, which live in different
 * images on the MEGA65: the signatures in the TLS bank (chain.c) and
 * the name and the dates in the client (policy.c), REQUIREMENTS.md 5.15. */
#ifndef DER_H
#define DER_H

#include <stdint.h>
#include "x509.h"

/* The certificate the walkers read, until the next call. */
void chain_select(x509_read read, void *ctx, uint16_t len);
/* Bytes of it. */
void der_read(uint16_t off, uint8_t *dst, uint16_t n);

/* A DER element: its tag, and its content's offset and length. */
typedef struct { uint8_t tag; uint16_t off, len; } der;

/* The element at `at`, which must end by `end`. 0 if it does not fit. */
uint8_t der_elem(uint16_t at, uint16_t end, der *d);
/* Where the element's content ends: the next element's offset. */
uint16_t der_after(const der *d);
/* The Certificate and the TBSCertificate inside it. */
uint8_t der_tbs(der *c, der *t);
/* The `want`th field of the TBSCertificate, past the optional [0] version. */
#define TBS_VALIDITY 3
#define TBS_EXTS 6
uint8_t der_tbs_field(uint8_t want, der *f);
/* Is `o` an OID of exactly `len` bytes (at most 9) whose first `n` are `want`? */
uint8_t der_oid_is(const der *o, const uint8_t *want, uint8_t n, uint8_t len);

#endif
