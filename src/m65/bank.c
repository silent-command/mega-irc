/* The TLS bank from the boot disk: CRYPTO to $12000, CHAIN (its top
 * window) to $1E000 and the trampoline to $1700, as the Gemini client's
 * boot.c does (ssh ck_boot); then the bank's INIT. Three tries each: the
 * F011 read path is not perfect. */
#include "mega65/memory.h"
#include "m65_boot.h"
#include "m65_cbmdos.h"
#include "ck_payload.h"
#include "m65_tlskit.h"
#include "bank.h"

#define CK_BASE 0x12000UL
#define CK_CHAIN 0x1E000UL
#define CK_TR 0x1700UL
#define HIGH_AT 0xE000UL          /* bank 0 under the KERNAL, which m65_own_vectors has mapped out: the client's own window (irc.ld, 5.17) */

extern char __hi_bss_start[], __hi_bss_end[];

unsigned char bank_boot(const char **err)
{
  unsigned char attempt;
  /* the client's own window first: the objects irc.ld names, loaded
   * where the PRG could not reach, and their .bss zeroed, which crt0
   * did only for the region it knows */
  for (attempt = 0; attempt < 3; attempt++)
    if (cbmdos_load("HIGH", boot_drive, HIGH_AT, CK_HIGH_SIZE) == CK_HIGH_SIZE) break;
  if (attempt == 3) { *err = "HIGH not found on the boot disk (or wrong size)"; return 0; }
  lfill((unsigned long)(unsigned int)__hi_bss_start, 0, (unsigned int)(__hi_bss_end - __hi_bss_start));
  lcopy((long)(unsigned int)ck_tramp_bin, (long)CK_TR, CK_TRAMP_SIZE);
  /* the map the trampoline restores after each call: this program's, with
   * the KERNAL out of $E000 (m65_own_vectors), not the trampoline's
   * built-in default, which maps it back and hands the next ethernet
   * interrupt to the KERNAL's handler (ssh ckit.c; mega-net 5.18) */
  lpoke(CK_TR + 0x0A, 0x00); lpoke(CK_TR + 0x0B, 0xE0); lpoke(CK_TR + 0x0C, 0x00); lpoke(CK_TR + 0x0D, 0x00);
  for (attempt = 0; attempt < 3; attempt++)
    if (cbmdos_load("CRYPTO", boot_drive, CK_BASE, CK_BIN_SIZE) == CK_BIN_SIZE) break;
  if (attempt == 3) { *err = "CRYPTO not found on the boot disk (or wrong size)"; return 0; }
  if (lpeek(CK_BASE) != 0x4c) { *err = "the TLS bank did not land at $12000"; return 0; }
  for (attempt = 0; attempt < 3; attempt++)
    if (cbmdos_load("CHAIN", boot_drive, CK_CHAIN, CK_CHAIN_SIZE) == CK_CHAIN_SIZE) break;
  if (attempt == 3) { *err = "CHAIN not found on the boot disk (or wrong size)"; return 0; }
  if (lpeek(CK_TR + 0x0E) != 0x08) { *err = "the bank's trampoline did not land"; return 0; }
  if (!m65_tlskit_init()) { *err = "the TLS bank's INIT failed"; return 0; }
  *err = 0;
  return 1;
}
