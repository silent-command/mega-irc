/* The host suite for the certificate chain: PKCS#1 v1.5, the walking
 * that x509.c leaves out, and the whole verification against the anchors
 * this client carries. Run against what Libera and OFTC actually served
 * on 2026-09-22 (the DER files under tests/certs, REQUIREMENTS.md 5.5).
 *
 * The family's rule is that the host proves the arithmetic and the
 * machine measures it, so nothing here goes near the MEGA65 until every
 * check below passes. The fixtures are real certificates rather than
 * generated ones on purpose: a chain the networks do not actually serve
 * proves nothing about joining them. */
#include <stdio.h>
#include <string.h>
#include "tls/chain.h"
#include "tls/roots.h"
#include "tls/x509.h"

#ifndef CERT_DIR
#define CERT_DIR "tests/certs"
#endif

static int checks, failed;
#define CHECK(cond, what) do { checks++; if (!(cond)) { failed++; printf("FAIL line %d: %s\n", __LINE__, what); } } while (0)

/* A certificate in memory, behind the same read hook the machine uses
 * for one sitting in far memory. 8192 because the same type holds a
 * whole concatenated chain below, and a Libera chain is 4543 bytes: a
 * first draft had 4096 here, the chain ran 447 bytes past the array
 * into the length field, and the splitter walked a corrupted store.
 * Undefined behaviour let seven checks pass and one fail. */
typedef struct { unsigned char b[8192]; unsigned short len; } cert;

static void rd(void *ctx, uint16_t off, uint8_t *dst, uint16_t n)
{
  const cert *c = (const cert *)ctx;
  uint16_t i;
  for (i = 0; i < n; i++) dst[i] = (uint16_t)(off + i) < c->len ? c->b[off + i] : 0;
}

static int load(cert *c, const char *name)
{
  char path[256];
  FILE *f;
  size_t n;
  snprintf(path, sizeof path, "%s/%s.der", CERT_DIR, name);
  f = fopen(path, "rb");
  if (!f) { printf("FAIL: cannot open %s\n", path); failed++; checks++; return 0; }
  n = fread(c->b, 1, sizeof c->b, f);
  fclose(f);
  c->len = (unsigned short)n;
  return n > 0;
}

/* A certificate that sits at an offset inside a larger store: the shape
 * the TLS kit's store has, and the bank's on the machine. The hook adds
 * the slot's base to every read. */
typedef struct { const cert *store; uint16_t base; } slot;
static void slot_rd(void *ctx, uint16_t o, uint8_t *dst, uint16_t k)
{
  const slot *s = (const slot *)ctx;
  rd((void *)s->store, (uint16_t)(s->base + o), dst, k);
}

/* The walkers read one selected certificate (chain.h): each helper
 * selects and asks, which is what every caller did in one call before. */
static uint8_t parts_of(cert *c, uint16_t len, chain_cert *p) { chain_select(rd, c, len); return chain_parts(p); }
static uint8_t names(cert *c, const char *host) { chain_select(rd, c, c->len); return chain_host_matches(host); }
static uint8_t valid(cert *c, const char *now) { chain_select(rd, c, c->len); return chain_valid_at(now); }
static uint8_t link(cert *child, cert *issuer, const x509_key *k)
{
  chain_ref a, b;
  a.read = rd; a.ctx = child; a.len = child->len;
  b.read = rd; b.ctx = issuer; b.len = issuer->len;
  return chain_verify_link(&a, &b, k);
}
/* Both halves, as the host client runs them and the machine runs them in
 * two images: the policy first, so no arithmetic is spent on a chain the
 * name or the date refuses; then the signatures. */
static uint8_t verify(const chain_ref *ch, uint8_t n, const char *host, const char *now)
{
  uint8_t r = chain_policy(ch, n, host, now);
  return r ? r : chain_verify(ch, n);
}

/* 2026-09-22, inside both leaves' validity: Libera's runs 260727 to
 * 261025 and OFTC's 260830 to 261128. */
static const char *const NOW = "260922000000";

static void chain_case(const char *net, const char *host, const char *cn_only,
                       uint16_t want_tbs, uint16_t want_sig)
{
  cert leaf, mid, root;
  chain_cert parts;
  x509_key kmid, kroot, kleaf;
  chain_ref ch[3];
  char name[64], msg[160];
  unsigned char save;

  snprintf(name, sizeof name, "%s_0", net); if (!load(&leaf, name)) return;
  snprintf(name, sizeof name, "%s_1", net); if (!load(&mid, name)) return;
  snprintf(name, sizeof name, "%s_2", net); if (!load(&root, name)) return;

  ch[0].read = rd; ch[0].ctx = &leaf; ch[0].len = leaf.len;
  ch[1].read = rd; ch[1].ctx = &mid;  ch[1].len = mid.len;
  ch[2].read = rd; ch[2].ctx = &root; ch[2].len = root.len;

  /* ---- the parts, against the offsets openssl reports for these files */
  snprintf(msg, sizeof msg, "%s: the leaf can be walked", net);
  CHECK(parts_of(&leaf, leaf.len, &parts), msg);
  snprintf(msg, sizeof msg, "%s: the signed range is %u bytes", net, want_tbs);
  CHECK(parts.tbs_len == want_tbs, msg);
  snprintf(msg, sizeof msg, "%s: the TBS starts at 4, after the outer header", net);
  CHECK(parts.tbs_off == 4, msg);
  snprintf(msg, sizeof msg, "%s: the signature is %u bytes", net, want_sig);
  CHECK(parts.sig_len == want_sig, msg);

  /* ---- the keys */
  snprintf(msg, sizeof msg, "%s: the intermediate's key is RSA", net);
  CHECK(x509_key_of(rd, &mid, mid.len, &kmid) && kmid.kind == X509_KEY_RSA, msg);
  snprintf(msg, sizeof msg, "%s: the anchor's key is RSA", net);
  CHECK(x509_key_of(rd, &root, root.len, &kroot) && kroot.kind == X509_KEY_RSA, msg);
  snprintf(msg, sizeof msg, "%s: the leaf's own key is read", net);
  CHECK(x509_key_of(rd, &leaf, leaf.len, &kleaf), msg);

  /* ---- one link at a time */
  snprintf(msg, sizeof msg, "%s: the leaf verifies under the intermediate", net);
  CHECK(link(&leaf, &mid, &kmid), msg);
  snprintf(msg, sizeof msg, "%s: the intermediate verifies under the anchor", net);
  CHECK(link(&mid, &root, &kroot), msg);
  snprintf(msg, sizeof msg, "%s: the leaf does not verify under the anchor", net);
  CHECK(!link(&leaf, &root, &kroot), msg);
  snprintf(msg, sizeof msg, "%s: the leaf does not verify under its own key", net);
  CHECK(!link(&leaf, &leaf, &kleaf), msg);

  /* ---- the name, which is the thing a CN check would get wrong */
  snprintf(msg, sizeof msg, "%s: the SAN covers %s", net, host);
  CHECK(names(&leaf, host), msg);
  snprintf(msg, sizeof msg, "%s: the SAN covers %s uppercased", net, host);
  { char up[64]; size_t i; for (i = 0; host[i] && i < sizeof up - 1; i++)
      up[i] = (char)(host[i] >= 'a' && host[i] <= 'z' ? host[i] - 32 : host[i]);
    up[i] = 0;
    CHECK(names(&leaf, up), msg); }
  snprintf(msg, sizeof msg, "%s: the SAN refuses a host it does not name", net);
  CHECK(!names(&leaf, "irc.example.com"), msg);
  snprintf(msg, sizeof msg, "%s: the SAN refuses a prefix of a name it has", net);
  CHECK(!names(&leaf, "irc.libera.cha"), msg);
  /* Whether the common name also appears among the alternative names is
   * the issuing authority's choice, not a property to assert: OFTC's
   * does. It is reported rather than checked, because a check that
   * cannot fail proves nothing and inflates the count. */
  printf("  note: %s CN %s %s in the SAN\n", net, cn_only,
         names(&leaf, cn_only) ? "is also" : "is NOT");

  /* ---- the dates */
  snprintf(msg, sizeof msg, "%s: the leaf is valid now", net);
  CHECK(valid(&leaf, NOW), msg);
  snprintf(msg, sizeof msg, "%s: the leaf is not valid in 2030", net);
  CHECK(!valid(&leaf, "300101000000"), msg);
  snprintf(msg, sizeof msg, "%s: the leaf was not valid in 2020", net);
  CHECK(!valid(&leaf, "200101000000"), msg);
  snprintf(msg, sizeof msg, "%s: the anchor is valid now", net);
  CHECK(valid(&root, NOW), msg);

  /* ---- the whole chain */
  snprintf(msg, sizeof msg, "%s: the full chain verifies to a carried anchor", net);
  CHECK(verify(ch, 3, host, NOW) == CHAIN_OK, msg);
  snprintf(msg, sizeof msg, "%s: without the anchor it is sound but untrusted", net);
  CHECK(verify(ch, 2, host, NOW) == CHAIN_NO_ANCHOR, msg);
  snprintf(msg, sizeof msg, "%s: the wrong host is refused before any arithmetic", net);
  CHECK(verify(ch, 3, "irc.example.com", NOW) == CHAIN_BAD_NAME, msg);
  snprintf(msg, sizeof msg, "%s: an expired chain is refused", net);
  CHECK(verify(ch, 3, host, "300101000000") == CHAIN_EXPIRED, msg);
  snprintf(msg, sizeof msg, "%s: the leaf alone is not an anchor", net);
  CHECK(verify(ch, 1, host, NOW) == CHAIN_NO_ANCHOR, msg);

  /* ---- tampering */
  save = leaf.b[parts.sig_off + 40]; leaf.b[parts.sig_off + 40] ^= 1;
  snprintf(msg, sizeof msg, "%s: one flipped bit in the signature fails the link", net);
  CHECK(!link(&leaf, &mid, &kmid), msg);
  snprintf(msg, sizeof msg, "%s: and fails the whole chain", net);
  CHECK(verify(ch, 3, host, NOW) == CHAIN_BAD_SIG, msg);
  leaf.b[parts.sig_off + 40] = save;

  save = leaf.b[parts.tbs_off + 60]; leaf.b[parts.tbs_off + 60] ^= 1;
  snprintf(msg, sizeof msg, "%s: one flipped bit in the signed body fails", net);
  CHECK(!link(&leaf, &mid, &kmid), msg);
  leaf.b[parts.tbs_off + 60] = save;

  snprintf(msg, sizeof msg, "%s: a truncated certificate is refused, not walked", net);
  CHECK(!parts_of(&leaf, (uint16_t)(leaf.len / 2), &parts), msg);

  snprintf(msg, sizeof msg, "%s: the untouched chain still verifies afterwards", net);
  CHECK(verify(ch, 3, host, NOW) == CHAIN_OK, msg);
}

int main(void)
{
  printf("certificate chains, as Libera and OFTC served them on 2026-09-22:\n");
  printf("  carrying %d anchors:", ROOTS_COUNT);
  { int i; for (i = 0; i < ROOTS_COUNT; i++) printf(" %s%s", roots_name[i], i + 1 < ROOTS_COUNT ? "," : ""); }
  printf("\n");

  chain_case("libera", "irc.libera.chat", "molybdenum.libera.chat", 1488, 256);
  chain_case("oftc", "irc.oftc.net", "weber.oftc.net", 1039, 256);

  /* A second host from Libera's SAN, to show the list is walked and not
   * just its first entry. */
  {
    cert leaf;
    if (load(&leaf, "libera_0")) {
      CHECK(names(&leaf, "irc.eu.libera.chat"),
            "libera: a later name in the SAN list is found too");
      CHECK(names(&leaf, "irc.gnome.org"),
            "libera: an unrelated domain in the same SAN is found");
    }
  }

  /* The deeper anchor: Root YR is itself signed by Root X1, which is the
   * fallback for a server that chains straight there. */
  {
    cert root_yr, x1;
    x509_key kx1;
    if (load(&root_yr, "libera_2") && load(&x1, "isrg_root_x1")) {
      CHECK(x509_key_of(rd, &x1, x1.len, &kx1) && kx1.kind == X509_KEY_RSA,
            "ISRG Root X1's key is RSA");
      CHECK(link(&root_yr, &x1, &kx1),
            "Root YR verifies under Root X1, the fallback anchor");
      { uint8_t h[32]; x509_key k;
        x509_key_of(rd, &x1, x1.len, &k);
        x509_spki_hash(rd, &x1, &k, h);
        CHECK(roots_index(h) < ROOTS_COUNT, "Root X1 is one of the carried anchors");
        x509_key_of(rd, &root_yr, root_yr.len, &k);
        x509_spki_hash(rd, &root_yr, &k, h);
        CHECK(roots_index(h) < ROOTS_COUNT, "Root YR is one of the carried anchors"); }
    }
  }

  /* A key we do not carry must not pass as an anchor. */
  {
    cert mid;
    if (load(&mid, "libera_1")) {
      uint8_t h[32]; x509_key k;
      x509_key_of(rd, &mid, mid.len, &k);
      x509_spki_hash(rd, &mid, &k, h);
      CHECK(roots_index(h) == ROOTS_COUNT, "the intermediate is NOT an anchor");
    }
  }

  /* The TLS kit's store: every certificate end to end, the leaf first.
   * The chain must be found in it from the DER headers alone, and verify
   * exactly as it did from three separate buffers. */
  {
    static cert store;
    static cert parts[3];
    slot s[3];
    chain_ref ch[3];
    uint16_t off[4], len[4];
    uint8_t n, i;
    int ok = 1;
    store.len = 0;
    for (i = 0; i < 3; i++) {
      char name[16]; snprintf(name, sizeof name, "libera_%u", i);
      if (!load(&parts[i], name)) { ok = 0; break; }
      if ((size_t)store.len + parts[i].len > sizeof store.b) { printf("FAIL: the store is too small for the chain\n"); failed++; checks++; ok = 0; break; }
      memcpy(store.b + store.len, parts[i].b, parts[i].len);
      store.len = (unsigned short)(store.len + parts[i].len);
    }
    if (ok) {
      n = chain_split(rd, &store, store.len, off, len, 4);
      CHECK(n == 3, "the store splits into the three certificates it holds");
      CHECK(off[0] == 0 && len[0] == parts[0].len, "the leaf is first and whole");
      CHECK(off[1] == parts[0].len && len[1] == parts[1].len, "the intermediate follows it exactly");
      CHECK(off[2] == (uint16_t)(parts[0].len + parts[1].len) && len[2] == parts[2].len, "and the anchor follows that");
      CHECK(chain_split(rd, &store, (uint16_t)(store.len - 100), off, len, 4) == 2,
            "a store cut short yields only the certificates that are whole");
      /* verify through the store, each certificate read at its own base:
       * what the machine does over the bank's store */
      for (i = 0; i < 3; i++) {
        s[i].store = &store; s[i].base = off[i];
        ch[i].read = slot_rd; ch[i].ctx = &s[i]; ch[i].len = len[i];
      }
      CHECK(verify(ch, 3, "irc.libera.chat", NOW) == CHAIN_OK,
            "the chain verifies straight out of the concatenated store");
      CHECK(verify(ch, 3, "irc.example.com", NOW) == CHAIN_BAD_NAME,
            "and still refuses the wrong host from there");
    }
  }

  /* Libera serves ECDSA P-384 from some of its servers (5.10). That chain
   * must be recognised for what it is and refused as unsupported, never
   * as malformed and never let through. */
  {
    static cert ec[4];
    chain_ref ch[4];
    chain_cert parts;
    uint8_t i, ok = 1;
    for (i = 0; i < 4; i++) {
      char name[24]; snprintf(name, sizeof name, "libera_ec_%u", i);
      if (!load(&ec[i], name)) { ok = 0; break; }
      ch[i].read = rd; ch[i].ctx = &ec[i]; ch[i].len = ec[i].len;
    }
    if (ok) {
      CHECK(parts_of(&ec[0], ec[0].len, &parts), "the EC leaf walks: it is a well-formed certificate");
      CHECK(parts.alg == CHAIN_ALG_ECDSA, "and is recognised as ECDSA-signed");
      CHECK(names(&ec[0], "irc.libera.chat"), "its SAN covers irc.libera.chat like the RSA ones");
      CHECK(verify(ch, 4, "irc.libera.chat", NOW) == CHAIN_UNSUPPORTED,
            "the EC chain is refused as UNSUPPORTED, not malformed, and not let through");
    }
  }

  printf("%d checks, %d failed\n", checks, failed);
  return failed != 0;
}
