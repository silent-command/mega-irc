/* The half of the chain check that is arithmetic: each certificate's
 * signature under the next one's key, and the top against the anchors
 * this client carries. On the MEGA65 this is the TLS bank's, in its top
 * window, beside the keys (REQUIREMENTS.md 5.15); the name and the
 * dates are policy.c's, in the client. */
#include "chain.h"
#include "der.h"
#include "roots.h"
#include "../crypto/crypto.h"
#include "../crypto/pkcs1.h"

/* The issuer's modulus and exponent and the child's signature have to be
 * contiguous for the arithmetic, so they are gathered here; the issuer's
 * key is found into `key`. On the host these are plain statics: a
 * 512-byte modulus is not going on the stack. In the bank they are names
 * for buffers it already has and that are idle while a chain is checked
 * (src/bank/chain_bank.h). */
#ifdef CHAIN_BANK
#include "chain_bank.h"
#else
static uint8_t issuer_n[512], issuer_e[8], child_sig[512];
static x509_key key;
#define key_of(r, k) x509_key_of((r)->read, (r)->ctx, (r)->len, k)
#define spki_hash(r, k, out) x509_spki_hash((r)->read, (r)->ctx, k, out)
#endif
#define N_CAP 512
#define E_CAP 8
#define SIG_CAP 512

/* 1.2.840.113549.1.1.11, sha256WithRSAEncryption: the algorithm every
 * certificate in both chains is signed with (5.5). */
static const uint8_t oid_sha256_rsa[9] = {
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b
};

/* 1.2.840.10045.4.3, the ecdsa-with-SHA2 family: .2 is SHA-256, .3 is
 * SHA-384. Recognised so the client can say what it met, not verified:
 * the chains this client meets that use it are P-384 throughout (5.10). */
static const uint8_t oid_ecdsa_sha2[7] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03 };

uint8_t chain_parts(chain_cert *out)
{
  der c, t, a, o, s;
  uint8_t unused;
  uint16_t end;

  if (!der_tbs(&c, &t)) return 0;
  end = der_after(&c);
  /* What is signed is the TBSCertificate entire, its own tag and length
   * included, so the range starts where its tag sits. */
  out->tbs_off = c.off;
  out->tbs_len = (uint16_t)(der_after(&t) - c.off);

  /* AlgorithmIdentifier, and the OID inside it */
  if (!der_elem(der_after(&t), end, &a) || a.tag != 0x30) return 0;
  if (!der_elem(a.off, end, &o) || o.tag != 0x06) return 0;
  out->alg = CHAIN_ALG_UNKNOWN;
  if (der_oid_is(&o, oid_sha256_rsa, sizeof oid_sha256_rsa, sizeof oid_sha256_rsa)) out->alg = CHAIN_ALG_RSA_SHA256;
  else if (der_oid_is(&o, oid_ecdsa_sha2, sizeof oid_ecdsa_sha2, sizeof oid_ecdsa_sha2 + 1)) out->alg = CHAIN_ALG_ECDSA;   /* the family prefix and one more arc */

  /* signatureValue: a BIT STRING whose first content byte counts unused bits */
  if (!der_elem(der_after(&a), end, &s) || s.tag != 0x03 || s.len < 2) return 0;
  der_read(s.off, &unused, 1);
  if (unused) return 0;
  out->sig_off = (uint16_t)(s.off + 1);
  out->sig_len = (uint16_t)(s.len - 1);
  return 1;
}

void chain_tbs_hash(const chain_cert *c, uint8_t out[32])
{
  sha256_ctx h;
  uint8_t buf[64];
  uint16_t off = c->tbs_off, left = c->tbs_len, take;
  sha256_init(&h);
  while (left) {
    take = left < sizeof buf ? left : (uint16_t)sizeof buf;
    der_read(off, buf, take);
    sha256_update(&h, buf, take);
    off = (uint16_t)(off + take);
    left = (uint16_t)(left - take);
  }
  sha256_final(&h, out);
}

static void select_ref(const chain_ref *r) { chain_select(r->read, r->ctx, r->len); }

uint8_t chain_verify_link(const chain_ref *child, const chain_ref *issuer, const x509_key *ikey)
{
  chain_cert cc;
  uint8_t hash[32];

  if (ikey->kind != X509_KEY_RSA) return 0;        /* both chains are RSA throughout (5.5) */
  if (ikey->n_len == 0 || ikey->n_len > N_CAP || ikey->e_len == 0 || ikey->e_len > E_CAP) return 0;
  select_ref(child);
  if (!chain_parts(&cc) || cc.alg != CHAIN_ALG_RSA_SHA256) return 0;   /* the only scheme verified here */
  if (cc.sig_len == 0 || cc.sig_len > SIG_CAP) return 0;
  chain_tbs_hash(&cc, hash);
  der_read(cc.sig_off, child_sig, cc.sig_len);
  select_ref(issuer);
  der_read(ikey->n_off, issuer_n, ikey->n_len);
  der_read(ikey->e_off, issuer_e, ikey->e_len);
  return rsa_pkcs1_sha256_verify(issuer_n, ikey->n_len, issuer_e, ikey->e_len, child_sig, cc.sig_len, hash);
}

uint8_t roots_index(const uint8_t hash[32])
{
  uint8_t i, j;
  for (i = 0; i < ROOTS_COUNT; i++) {
    for (j = 0; j < 32; j++) if (roots_spki[i][j] != hash[j]) break;
    if (j == 32) return i;
  }
  return ROOTS_COUNT;
}

uint8_t chain_verify(const chain_ref *certs, uint8_t n)
{
  chain_cert cc;
  uint8_t i, hash[32];

  if (n == 0) return CHAIN_MALFORMED;
  for (i = 0; (uint8_t)(i + 1) < n; i++) {
    /* Say what a chain is signed with before trying to verify it, so a
     * scheme this client cannot check is reported as that and not as a
     * malformed certificate. The header walk is cheap; the arithmetic
     * that follows is not, and is skipped for what cannot pass. */
    select_ref(&certs[i]);
    if (!chain_parts(&cc)) return CHAIN_MALFORMED;
    if (cc.alg == CHAIN_ALG_ECDSA) return CHAIN_UNSUPPORTED;
    if (cc.alg != CHAIN_ALG_RSA_SHA256) return CHAIN_MALFORMED;
    if (!key_of(&certs[i + 1], &key)) return CHAIN_MALFORMED;
    if (!chain_verify_link(&certs[i], &certs[i + 1], &key)) return CHAIN_BAD_SIG;
  }

  /* The top must be an anchor we carry. Nothing above it is fetched: a
   * chain that ends anywhere else is sound but untrusted, and the caller
   * decides whether to fall back to trust on first use. */
  if (!key_of(&certs[n - 1], &key)) return CHAIN_MALFORMED;
  spki_hash(&certs[n - 1], &key, hash);
  if (roots_index(hash) == ROOTS_COUNT) return CHAIN_NO_ANCHOR;
  return CHAIN_OK;
}
