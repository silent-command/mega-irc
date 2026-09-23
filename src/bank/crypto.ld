/* The TLS bank: a raw, headerless image in bank 1 at physical $12000,
 * executed with bank 1 mapped over CPU $2000-$BFFF (and its top 8 KB,
 * $1E000-$1FFFF, over $E000-$FFFF for the interrupt vectors in force
 * during a call). The Gemini client's bank (its src/bank/crypto.ld;
 * ssh REQUIREMENTS.md 5.4, 5.7; mega-net 5.7, 5.10, 5.11), with the
 * chain check in the top window where Gemini keeps its renderer
 * (REQUIREMENTS.md 5.13, 5.15).
 *
 * $2000-$BDFF is code and data; $BE00-$BFFF the bank's own soft stack;
 * $E000-$F7FF the chain check (CHAIN, a second file, loaded to $1E000).
 * $F800 up is the colour-RAM mirror, which in 80x50 is the display's:
 * the vectors at $FFFA are written by the prologue on every entry and
 * the colour put back on exit (jumptable.S, 5.15). */
MEMORY {
    ram (rw) : ORIGIN = 0x2000, LENGTH = 0x9E00
    hi (rwx) : ORIGIN = 0xE000, LENGTH = 0x1800
}

/* Zero page $68-$FF, swapped with the caller's on every call, as
 * mega-net does with $90-$FF: each saves what it finds and puts it
 * back, so the bank nests inside the client and mega-net's POLL nests
 * inside the bank. Eight bytes more than the SSH client's $70: the
 * multiplier's 16-bit row once walked three zero-page pointers (gemini
 * 5.6); the 32-bit row that replaced it patches addresses instead (5.12). */
__rc0 = 0x68;
INCLUDE imag-regs.ld
MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = 0x100 - (__rc31 + 1) }

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

__stack = 0xC000;

SECTIONS {
    .jumptable : { KEEP(*(.jumptable)) } > ram
    /* The chain check in the top window ($1E000-$1F7FF), code and data:
     * the window is the second image, CHAIN. The objects are compiled
     * without LTO so they can be named here; a pattern that matches
     * nothing is silent (gemini 5.8), so check the map after touching this. */
    .text.hi : { *der.o(.text .text.* .rodata .rodata.*) *chain.o(.text .text.* .rodata .rodata.*) *roots.o(.text .text.* .rodata .rodata.*) *chain_api.o(.text .text.* .rodata .rodata.*) } > hi
    /* The static stack (the compiler's home for the locals of functions
     * that never recurse, 416 bytes, .noinit..Lstatic_stack) is data
     * reached by absolute address like the rest, and the region below
     * the bank's stack is full: it lives here too (5.15). */
    .bss.hi (NOLOAD) : { *der.o(.bss .bss.*) *chain.o(.bss .bss.*) *chain_api.o(.bss .bss.*) *jumptable*.o(.noinit .noinit.*) *lto.o(.noinit .noinit.*) } > hi
    INCLUDE c.ld
}

/* The whole first region, then the second trimmed: build.py splits the
 * file at 0x9E00 into CRYPTO and CHAIN. */
OUTPUT_FORMAT { FULL(ram) TRIM(hi) }
