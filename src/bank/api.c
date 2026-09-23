/* The TLS bank's entries: the C behind each jump in jumptable.S, which
 * is the kit (src/tls/tlskit.h) as the bank sees it. The Gemini client's
 * api.c with this client's differences (REQUIREMENTS.md 5.15): no
 * renderer and no self-test; CertificateVerify checked for RSA-PSS only,
 * since an EC chain is refused before it could matter (5.10, 5.14); the
 * P-256 pair for a server that will not take X25519 (5.14); the store
 * read out to the client, whose half of the chain check needs it; and
 * the chain entry itself, in the top window (chain_api.c).
 *
 * Every entry reads a parameter block from the caller's memory by DMA
 * (A/X/Y hold its 28-bit address), moves data through a staging buffer,
 * and never touches the caller's memory with the CPU, which is hidden
 * while the bank is mapped. The keys, the transcript, the record cipher
 * and the server's certificates all live here; the client sees none of
 * them. INIT is this image's crt0 (ssh src/bank/api.c; mega-net 5.7,
 * 5.11). The long operations yield to mega-net's poll between rounds. */
#include <stdint.h>
#include "crypto.h"
#include "aead.h"
#include "hkdf.h"
#include "keys.h"
#include "x509.h"
#include "p256.h"
#include "dma.h"
#include "mp.h"

#define IN_BSS(n) __attribute__((section(".bss." n)))
#define POKE(a, v) (*(volatile uint8_t *)(a) = (uint8_t)(v))
#define PEEK(a) (*(volatile uint8_t *)(a))
#define PHYS(p) ((uint32_t)(uintptr_t)(p) + CK_PHYS_BASE)

#define CERT_FAR 0x5E900UL       /* bank 5, above mega-net's socket pool: the certificate chain, the leaf first */
#define CERT_CAP 0x1700U         /* 5888 bytes; Libera's chain is 4543, OFTC's 4094 (5.5); more is cut */

extern volatile uint8_t ck_api_a, ck_api_x, ck_api_y, ck_api_z;
extern volatile uint8_t ck_api_ra, ck_api_rx, ck_api_ry, ck_api_rz;
extern char __bss_start[], __bss_size[];
extern char __zp_bss_start[], __zp_bss_size[];
extern char __zp_data_start[], __zp_data_load_start[], __zp_data_size[];

/* the stage and the small buffer are shared by name with the chain check
 * in the top window (chain_bank.h), which is why they are not static */
uint8_t ck_stage[256] IN_BSS("stage");
uint8_t ck_small[64] IN_BSS("small");
#define stage ck_stage
#define small ck_small
static uint8_t blk[16] IN_BSS("blk");
static sha256_ctx transcript IN_BSS("transcript");
static uint8_t pool[32] IN_BSS("pool");          /* the random pool: a hash, stirred by the client's samples */
static uint32_t pool_ctr IN_BSS("pool_ctr");
static uint8_t priv[32] IN_BSS("priv");          /* the X25519 or the P-256 private key: one handshake uses one */
static tls_schedule sched IN_BSS("sched");
static tls_traffic rd IN_BSS("rd"), wr IN_BSS("wr");
static uint32_t rd_seq IN_BSS("rd_seq"), wr_seq IN_BSS("wr_seq");
static aead_ctx cipher IN_BSS("cipher");         /* reading: a record may still be open while... */
static aead_ctx sealer IN_BSS("sealer");         /* ...the client's Finished is sealed (gemini 5.5) */
uint16_t ck_cert_length IN_BSS("cert_len");      /* shared with chain_api.c */
#define cert_len ck_cert_length
#define sig ((uint8_t *)(mp_scratch + 256))      /* an RSA-4096 signature, 512 bytes: the upper half of the scratch, which RSA fills only after it has read the signature; not the multiplier's t, which mp_init now uses (gemini 5.12) */
static uint8_t digest[32] IN_BSS("digest");
/* verify's content and key overlay the stage (off the 512-byte soft
 * stack, and the bank is full): the content is hashed into digest
 * before the certificate is read through rdbuf, which is the stage's
 * first 64 bytes; the key sits above them (gemini 5.6). */
#define content stage
#define vkey (*(x509_key *)(stage + 130))

static uint32_t arg_ptr(void)
{
  return (uint32_t)ck_api_a | ((uint32_t)ck_api_x << 8) | ((uint32_t)ck_api_y << 16);
}

static uint32_t p28(const uint8_t *b)
{
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t u16(const uint8_t *b) { return (uint16_t)(b[0] | ((uint16_t)b[1] << 8)); }

static void get_block(void) { ck_dma_copy(arg_ptr(), PHYS(blk), sizeof blk); }

/* ---- the yield: mega-net's poll, with the map put back to ours ------ */

#define NET_TR 0x1600
#define NET_ENTRY_POLL 0x200F

static uint16_t yield_frames IN_BSS("yield_frames");   /* frames of $D7FA seen at the yields: how long an entry ran (gemini 5.12) */
static uint8_t yield_last IN_BSS("yield_last");
static void count_frames(void) { uint8_t f = PEEK(0xd7fa); yield_frames = (uint16_t)(yield_frames + (uint8_t)(f - yield_last)); yield_last = f; }
void ck_frames_start(void) { yield_frames = 0; yield_last = PEEK(0xd7fa); }
void ck_frames_done(void) { count_frames(); blk[14] = (uint8_t)yield_frames; blk[15] = (uint8_t)(yield_frames >> 8); ck_dma_copy(PHYS(blk + 14), arg_ptr() + 14, 2); }   /* the frames into the caller's block: not a return register, so Z stays zero (trap 2, gemini 5.12) */

static void net_poll(void)
{
  uint8_t sa, sx, sy, sz;
  count_frames();
  sa = PEEK(NET_TR + 0x0A); sx = PEEK(NET_TR + 0x0B); sy = PEEK(NET_TR + 0x0C); sz = PEEK(NET_TR + 0x0D);
  POKE(NET_TR + 0x0A, 0x00); POKE(NET_TR + 0x0B, 0xE1);       /* our map, as the trampoline sets it */
  POKE(NET_TR + 0x0C, 0x00); POKE(NET_TR + 0x0D, 0xB1);
  POKE(NET_TR + 0x00, NET_ENTRY_POLL & 0xff); POKE(NET_TR + 0x01, NET_ENTRY_POLL >> 8);
  POKE(NET_TR + 0x02, 0); POKE(NET_TR + 0x03, 0); POKE(NET_TR + 0x04, 0); POKE(NET_TR + 0x05, 0);
  ((void (*)(void))(NET_TR + 0x0E))();
  POKE(NET_TR + 0x0A, sa); POKE(NET_TR + 0x0B, sx); POKE(NET_TR + 0x0C, sy); POKE(NET_TR + 0x0D, sz);
}

/* ---- entries ---------------------------------------------------------- */

void ck_api_init(void)
{
  uint16_t n = (uint16_t)(uintptr_t)__bss_size, i;
  for (i = 0; i < n; i++) __bss_start[i] = 0;
  n = (uint16_t)(uintptr_t)__zp_bss_size;
  for (i = 0; i < n; i++) __zp_bss_start[i] = 0;
  n = (uint16_t)(uintptr_t)__zp_data_size;
  for (i = 0; i < n; i++) __zp_data_start[i] = __zp_data_load_start[i];
  /* Where the vectors point while the bank is mapped: an RTI at $0F0F
   * in bank 0, the ROM's old screen page, which nothing writes once the
   * screen is at $10000. The vectors themselves are written by the
   * prologue on every entry (jumptable.S, 5.15). */
  POKE(0x0F0F, 0x40);
  crypto_yield = net_poll;
  ck_api_ra = 0;
}

void ck_api_version(void) { ck_api_ra = 0; ck_api_rx = 1; }

/* Runs f over the caller's bytes in stage-sized pieces. */
static void each_piece(uint32_t src, uint16_t left, void (*f)(uint8_t *, uint16_t), uint8_t back)
{
  uint16_t take;
  while (left) {
    take = left < sizeof stage ? left : (uint16_t)sizeof stage;
    ck_dma_copy(src, PHYS(stage), take);
    f(stage, take);
    if (back) ck_dma_copy(PHYS(stage), src, take);
    src += take; left = (uint16_t)(left - take);
  }
}

/* ---- random: pool = SHA-256(pool, sample); out = SHA-256(pool, counter) */

static void stir(uint8_t *p, uint16_t n)
{
  sha256_ctx c;
  sha256_init(&c); sha256_update(&c, pool, 32); sha256_update(&c, p, n); sha256_final(&c, pool);
}

/* {ptr28, len16} */
void ck_api_random_seed(void) { get_block(); each_piece(p28(blk), u16(blk + 4), stir, 0); }

static void random_fill(uint8_t *out, uint8_t n)
{
  sha256_ctx c;
  uint8_t i, take;
  while (n) {
    sha256_init(&c); sha256_update(&c, pool, 32); sha256_update(&c, (const uint8_t *)&pool_ctr, 4); sha256_final(&c, small);
    pool_ctr++;
    take = n < 32 ? n : 32;
    for (i = 0; i < take; i++) out[i] = small[i];
    out += take; n = (uint8_t)(n - take);
  }
}

/* {out28, n u8} */
void ck_api_random(void)
{
  get_block();
  random_fill(small, blk[4]);
  ck_dma_copy(PHYS(small), p28(blk), blk[4]);
}

/* ---- the transcript ---------------------------------------------------- */

void ck_api_transcript_init(void) { sha256_init(&transcript); }

static void tr_update(uint8_t *p, uint16_t n) { sha256_update(&transcript, p, n); }

/* {ptr28, len16} */
void ck_api_transcript_update(void) { get_block(); each_piece(p28(blk), u16(blk + 4), tr_update, 0); }

/* {out28} */
void ck_api_transcript_hash(void)
{
  sha256_ctx c = transcript;
  get_block();
  sha256_final(&c, small);
  ck_dma_copy(PHYS(small), p28(blk), 32);
}

/* ---- keys -------------------------------------------------------------- */

/* the handshake secrets from the shared secret and hash(CH, SH); the
 * handshake traffic keys installed for reading and writing, sequence 0 */
static void keys_from(const uint8_t *ecdhe, const uint8_t *hash)
{
  tls_schedule_start(&sched);
  tls_schedule_handshake(&sched, ecdhe, hash);
  tls_traffic_keys(sched.s_hs, &rd); rd_seq = 0;
  tls_traffic_keys(sched.c_hs, &wr); wr_seq = 0;
}

/* {out28} */
void ck_api_keyshare(void)
{
  get_block();
  random_fill(priv, 32);
  x25519_base(small, priv);
  ck_dma_copy(PHYS(small), p28(blk), 32);
}

/* {peer28, hash28} */
void ck_api_keys_handshake(void)
{
  uint8_t ecdhe[32];
  get_block();
  ck_dma_copy(p28(blk), PHYS(small), 32);        /* the peer's share */
  ck_dma_copy(p28(blk + 4), PHYS(small + 32), 32);   /* hash(CH, SH) */
  ck_frames_start();
  x25519(ecdhe, priv, small);
  keys_from(ecdhe, small + 32);
  ck_frames_done();
}

/* {out28}: 04, X, Y. A scalar the curve refuses is drawn again, as the
 * host kit does; the frames are counted, since this is a scalar
 * multiplication too (5.14). */
void ck_api_keyshare_p256(void)
{
  get_block();
  ck_frames_start();
  do random_fill(priv, 32); while (!p256_keygen(stage + 1, priv));
  stage[0] = 4;
  ck_dma_copy(PHYS(stage), p28(blk), 65);
  ck_frames_done();
}

/* {peer28, hash28}; A = 1, or 0 if the peer's 65 bytes are not an
 * uncompressed point on the curve */
void ck_api_keys_handshake_p256(void)
{
  uint8_t ecdhe[32];
  get_block();
  ck_dma_copy(p28(blk), PHYS(stage), 65);
  ck_dma_copy(p28(blk + 4), PHYS(small + 32), 32);
  ck_api_ra = 0;
  ck_frames_start();
  if (stage[0] == 4 && p256_ecdh(ecdhe, priv, stage + 1)) {
    keys_from(ecdhe, small + 32);
    ck_api_ra = 1;
  }
  ck_frames_done();
}

/* {hash28} */
void ck_api_keys_master(void)
{
  get_block();
  ck_dma_copy(p28(blk), PHYS(small), 32);
  tls_schedule_master(&sched, small);
}

void ck_api_write_switch(void) { tls_traffic_keys(sched.c_ap, &wr); wr_seq = 0; }
void ck_api_read_switch(void) { tls_traffic_keys(sched.s_ap, &rd); rd_seq = 0; }

/* {server u8, hash28, out28} */
void ck_api_finished(void)
{
  get_block();
  ck_dma_copy(p28(blk + 1), PHYS(small), 32);
  tls_finished(blk[0] ? sched.s_hs : sched.c_hs, small, small + 32);
  ck_dma_copy(PHYS(small + 32), p28(blk + 5), 32);
}

/* ---- records ----------------------------------------------------------- */

static void enc_piece(uint8_t *p, uint16_t n) { aead_encrypt(&sealer, p, n); }
static void dec_piece(uint8_t *p, uint16_t n) { aead_decrypt(&cipher, p, n); }

/* {rec28, len16}: the header's length filled in, the plaintext
 * encrypted in place, the tag appended */
void ck_api_seal(void)
{
  uint8_t nonce[12];
  uint32_t rec;
  uint16_t len;
  get_block();
  rec = p28(blk); len = u16(blk + 4);
  ck_dma_copy(rec, PHYS(small), 5);
  small[3] = (uint8_t)((len + 16) >> 8); small[4] = (uint8_t)(len + 16);
  ck_dma_copy(PHYS(small), rec, 5);
  tls_nonce(&wr, 0, wr_seq++, nonce);
  aead_start(&sealer, wr.key, nonce, small, 5);
  each_piece(rec + 5, len, enc_piece, 1);
  aead_tag(&sealer, small);
  ck_dma_copy(PHYS(small), rec + 5 + len, 16);
  /* Poly1305's multiplier table on this machine is one global, valid for
   * whichever context built it last (poly1305.c, rr_ready): the record
   * being read, if one is open, must rebuild its own (gemini 5.5) */
  cipher.poly.rr_ready = 0;
}

/* {hdr28} */
void ck_api_open_start(void)
{
  uint8_t nonce[12];
  get_block();
  ck_dma_copy(p28(blk), PHYS(small), 5);
  tls_nonce(&rd, 0, rd_seq, nonce);
  aead_start(&cipher, rd.key, nonce, small, 5);
}

/* {ptr28, len16}: in place */
void ck_api_open_data(void) { get_block(); each_piece(p28(blk), u16(blk + 4), dec_piece, 1); }

/* {tag28}; A = 1 if it matched */
void ck_api_open_check(void)
{
  get_block();
  ck_dma_copy(p28(blk), PHYS(small), 16);
  rd_seq++;
  ck_api_ra = (uint8_t)aead_check(&cipher, small);
}

/* ---- the certificates ---------------------------------------------------- */

void ck_api_cert_reset(void) { cert_len = 0; }

/* {ptr28, len16}: appended in far memory, up to CERT_CAP */
void ck_api_cert_append(void)
{
  uint16_t n;
  get_block();
  n = u16(blk + 4);
  if ((uint32_t)cert_len + n > CERT_CAP) n = (uint16_t)(CERT_CAP - cert_len);
  if (n) { ck_dma_copy(p28(blk), CERT_FAR + cert_len, n); cert_len = (uint16_t)(cert_len + n); }
}

void ck_api_cert_len(void) { ck_api_ra = (uint8_t)cert_len; ck_api_rx = (uint8_t)(cert_len >> 8); }

/* {off16, dst28, len16}: bytes of the store into the caller's memory, for
 * the client's half of the chain check (5.15). Clamped to the store:
 * the kit's promise of zeros past the end is kept by the client's kit,
 * which has the room this region has not. */
void ck_api_cert_read(void)
{
  uint16_t off, n;
  get_block();
  off = u16(blk); n = u16(blk + 6);
  if (off >= cert_len) return;
  if (n > (uint16_t)(cert_len - off)) n = (uint16_t)(cert_len - off);
  ck_dma_copy(CERT_FAR + off, p28(blk + 2), n);
}

/* The store's bytes into `dst`, which is whatever the walker hands
 * over: a local, often in zero page, which a DMA cannot reach (zero page
 * is bank 0's, not this image's, so PHYS() of it is wrong: gemini 5.5).
 * So the DMA lands in a pinned buffer and the CPU copies from there.
 * `ctx` is a base offset, so one certificate of the chain can be read
 * as if it began at zero (chain_api.c). */
#define rdbuf stage                              /* the stage's first RDBUF_CAP bytes, idle during a verify */
#define RDBUF_CAP 64                             /* not sizeof stage: the key sits at stage + 130, and an RSA modulus is 256 bytes (gemini 5.10) */
void ck_cert_read(void *ctx, uint16_t off, uint8_t *dst, uint16_t n)
{
  uint8_t *end = dst + n, *q, *qend;
  uint16_t take;
  off = (uint16_t)(off + (uint16_t)(uintptr_t)ctx);
  while (dst < end) {                              /* walks, not counts: the checker's rule (ssh 5.6) */
    if (off >= cert_len) { while (dst < end) *dst++ = 0; return; }
    take = (uint16_t)(end - dst);
    if (take > RDBUF_CAP) take = RDBUF_CAP;
    if ((uint32_t)off + take > cert_len) take = (uint16_t)(cert_len - off);
    ck_dma_copy(CERT_FAR + off, PHYS(rdbuf), take);
    for (q = rdbuf, qend = rdbuf + take; q < qend; q++) *dst++ = *q;
    off = (uint16_t)(off + take);
  }
}

/* The key and the pin of one certificate of the store, for the chain
 * check in the top window (chain_bank.h): through the one reader, as a
 * constant, so the link keeps a single x509_key_of (5.15). */
uint8_t ck_key_of(uint16_t base, uint16_t len, x509_key *k) { return x509_key_of(ck_cert_read, (void *)(uintptr_t)base, len, k); }
void ck_spki_hash(uint16_t base, const x509_key *k, uint8_t out[32]) { x509_spki_hash(ck_cert_read, (void *)(uintptr_t)base, k, out); }

/* The content signed in CertificateVerify: 64 spaces, the context
 * string, a zero, the transcript hash (RFC 8446 4.4.3). */
static void cv_content(const uint8_t hash[32], uint8_t out[130])
{
  static const char ctx[] = "TLS 1.3, server CertificateVerify";
  uint8_t i;
  for (i = 0; i < 64; i++) out[i] = ' ';
  for (i = 0; ctx[i]; i++) out[64 + i] = (uint8_t)ctx[i];
  out[97] = 0;
  for (i = 0; i < 32; i++) out[98 + i] = hash[i];
}

/* {scheme16, hash28, sig28, siglen16}; A = 0 wrong, 1 verified, 2
 * unchecked; X = why, when wrong: 1 the signature is too long, 2 the
 * certificate could not be read, 3 the key is of another kind, 4 the
 * arithmetic (or the DER) refused it. Only rsa_pss_rsae_sha256 is
 * checked here: the leaf's key decides the scheme, and a leaf with an
 * EC key sits in an EC chain, which the chain check refuses (5.10), so
 * the ECDSA verifier would only ever have run for a connection about to
 * be dropped. Leaving it out is what made the P-256 agreement fit (5.15). */
void ck_api_verify(void)
{
  uint16_t scheme, siglen;
  get_block();
  scheme = (uint16_t)(((uint16_t)blk[0] << 8) | blk[1]); siglen = u16(blk + 10);
  if (scheme != 0x0804) { ck_api_ra = 2; return; }
  ck_api_ra = 0; ck_api_rx = 1;
  ck_frames_start();
  if (siglen > 512) return;                        /* sig is 512 bytes; RSA-4096 is 512 */
  ck_dma_copy(p28(blk + 2), PHYS(small), 32);
  ck_dma_copy(p28(blk + 6), PHYS(sig), siglen);
  cv_content(small, content);
  sha256(content, 130, digest);                    /* before the certificate is read: content overlays the stage */
  ck_api_rx = 2;
  if (!x509_key_of(ck_cert_read, 0, cert_len, &vkey)) return;
  ck_api_rx = 3;
  if (vkey.kind != X509_KEY_RSA) return;
  ck_api_rx = 4;
  ck_api_ra = x509_rsa_pss_verify(ck_cert_read, 0, &vkey, digest, sig, siglen);
  ck_frames_done();
}

/* {out28}; A = 0: the pin is not built here. It is the trust-on-first-use
 * fallback's primitive (REQUIREMENTS.md 2) and that fallback is not in
 * the MVP; gemini's api.c has the working entry, for when it is, and the
 * bytes it takes were wanted first for the chain (5.15). */
void ck_api_pin(void) { ck_api_ra = 0; }

void ck_api_selftest(void) { ck_api_ra = 0; ck_api_rx = 9; }   /* not built here: gemini's, with CK_SELFTEST */
