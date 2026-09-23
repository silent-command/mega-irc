/* Walking a certificate chain: what x509.h deliberately leaves out.
 *
 * The Gemini client trusts a server's key on first use and says so
 * plainly at the top of x509.h -- "No chain, no names, no dates" --
 * because for Gemini's self-signed world that is the right policy. It
 * is the wrong one here: Libera and OFTC use Let's Encrypt, whose keys
 * rotate every 60 to 90 days, so pinning by key would report a changed
 * key at every renewal (REQUIREMENTS.md 2, 5.5).
 *
 * So this client carries a root public key or two and verifies the
 * chain up to one of them. That needs what x509.c does not expose: the
 * TBSCertificate's byte range, the signature, the validity dates and
 * the subject alternative names.
 *
 * The check is in two halves, because on the MEGA65 they live in two
 * images (5.15): the signatures and the anchor are arithmetic and stay
 * in the TLS bank beside the keys (chain.c, chain_verify); the host
 * name and the dates are policy, need no key, and run in the client,
 * where the clock is (policy.c, chain_policy). The host runs both.
 * Every certificate is read through an x509_read hook (der.h), so one
 * may sit in far memory on the machine and in a plain buffer on the
 * host without this code knowing the difference.
 */
#ifndef CHAIN_H
#define CHAIN_H

#include <stdint.h>
#include "x509.h"
#include "der.h"

/* What a certificate is signed with. Only the first is verified here;
 * the second is recognised so the client can say what it met, because
 * Libera serves ECDSA P-384 from some of its servers (5.10) and
 * "malformed" would be the wrong word for a chain that is merely one
 * this client cannot check. */
#define CHAIN_ALG_UNKNOWN 0
#define CHAIN_ALG_RSA_SHA256 1     /* sha256WithRSAEncryption: verified */
#define CHAIN_ALG_ECDSA 2          /* ecdsa-with-SHA256 or -SHA384: recognised, not verified */

typedef struct {
  uint16_t tbs_off, tbs_len;   /* the TBSCertificate, its own tag and length included: what is signed */
  uint16_t sig_off, sig_len;   /* the signature, past the BIT STRING's unused-bits byte */
  uint8_t alg;                 /* CHAIN_ALG_* */
} chain_cert;

/* One certificate of a chain, wherever the caller keeps it. */
typedef struct {
  x509_read read;
  void *ctx;
  uint16_t len;
} chain_ref;

/* What a chain check concluded. */
#define CHAIN_OK 0           /* every signature good, the top is an anchor we carry */
#define CHAIN_MALFORMED 1    /* a certificate could not be walked */
#define CHAIN_BAD_SIG 2      /* a link did not verify under its issuer */
#define CHAIN_NO_ANCHOR 3    /* the chain is sound but ends somewhere we do not trust */
#define CHAIN_BAD_NAME 4     /* the leaf is not for the host we dialled */
#define CHAIN_EXPIRED 5      /* outside a certificate's validity */
#define CHAIN_UNSUPPORTED 6  /* signed with a scheme this client recognises but cannot verify (ECDSA) */

/* ---- the signatures: chain.c, the bank's half ---------------------------- */

/* The parts of the selected certificate (chain_select, der.h) that
 * verifying it needs, and what it is signed with. 0 only if the
 * certificate cannot be walked; a scheme this client cannot verify still
 * parses, with alg saying which, so the caller can tell the user the
 * truth (5.10). */
uint8_t chain_parts(chain_cert *out);

/* SHA-256 over the selected certificate's TBSCertificate, read through
 * the hook in pieces so nothing needs the whole certificate at once. */
void chain_tbs_hash(const chain_cert *c, uint8_t out[32]);

/* Is `child` signed by `ikey`, the key already found in `issuer`? 1 if
 * the signature is good. This checks the signature and nothing else,
 * and leaves `issuer` selected. */
uint8_t chain_verify_link(const chain_ref *child, const chain_ref *issuer, const x509_key *ikey);

/* Every signature, certs[0] the leaf and each signed by the next, and
 * that the top is an anchor from roots.h. Returns CHAIN_OK, MALFORMED,
 * BAD_SIG, NO_ANCHOR or UNSUPPORTED; never BAD_NAME or EXPIRED, which
 * are chain_policy's. */
uint8_t chain_verify(const chain_ref *certs, uint8_t n);

/* The TLS kit keeps the server's certificates end to end in one store,
 * the leaf first, each one's own DER header saying where the next
 * begins, so no table of offsets is needed. This finds them. Returns
 * how many, at most `max`; a store that stops parsing cleanly returns
 * the ones found before it did. In der.c: both halves start with it. */
uint8_t chain_split(x509_read read, void *ctx, uint16_t total, uint16_t *off, uint16_t *len, uint8_t max);

/* ---- the policy: policy.c, the client's half ----------------------------- */

/* Is `now` inside the selected certificate's validity? `now` is twelve
 * digits, YYMMDDHHMMSS, the shape a UTCTime carries, so the comparison
 * is on digits and needs no calendar. */
uint8_t chain_valid_at(const char *now);

/* Does the selected certificate's SubjectAltName cover `host`? A
 * leading "*." matches one label. The common name is NOT consulted:
 * both networks put the name anyone actually dials in the SAN and
 * something else entirely in the CN (5.8), so a CN fallback would only
 * ever accept a certificate the SAN had already refused. */
uint8_t chain_host_matches(const char *host);

/* The leaf's name and every certificate's dates. `host` or `now` may be
 * null to skip that check. Returns CHAIN_OK, BAD_NAME or EXPIRED, and
 * MALFORMED for an empty chain. Cheap: run it before chain_verify, so
 * the arithmetic is never spent on a certificate the policy refuses. */
uint8_t chain_policy(const chain_ref *certs, uint8_t n, const char *host, const char *now);

#endif
