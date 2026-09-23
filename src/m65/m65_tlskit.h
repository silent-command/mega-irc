/* The MEGA65 side of the kit beyond tlskit.h: bringing the bank up,
 * feeding it entropy, and its chain entry. The bank image (CRYPTO on the
 * disk) is loaded to $12000, its top window (CHAIN) to $1E000 and the
 * trampoline to $1700 by bank.c; then INIT. The Gemini client's
 * m65_tlskit.h with this client's entries (REQUIREMENTS.md 5.15). */
#ifndef M65_TLSKIT_H
#define M65_TLSKIT_H

#include <stdint.h>

/* INIT: the bank's crt0. 1 on success. After the images are in place. */
uint8_t m65_tlskit_init(void);

/* Entropy into the bank's pool: hardware samples from the client. */
void m65_tlskit_seed(const uint8_t *p, uint16_t n);

/* After tk_verify returned TK_WRONG: 1 too long, 2 no certificate, 3 key kind, 4 refused. */
extern uint8_t tk_verify_reason;
/* Frames the bank spent on the key agreement (both P-256 steps counted
 * in), on the signature, and on the chain (gemini 5.12). */
extern uint16_t tk_frames_keys, tk_frames_verify, tk_frames_chain;

/* The bank's half of the chain check over the certificates it holds:
 * the signatures and the anchor (chain.h, chain_verify). Returns CHAIN_*;
 * tk_chain_count is how many certificates the store held. Run the
 * client's half first (chain_policy), which costs nothing. */
uint8_t tk_chain(void);
extern uint8_t tk_chain_count;

#endif
