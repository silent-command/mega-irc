/* The client: the MEGA65 target's link.ld (llvm-mos mos-platform/mega65)
 * with a second region, $E000-$FEFF in bank 0. That is RAM once
 * m65_own_vectors has mapped the KERNAL out, which the whole run is,
 * and nothing in the family's map claims it below mega-net's vector
 * stub at $FF00 (PLATFORM.md). A PRG cannot load there, the KERNAL
 * being in the way at LOAD time, so the objects named below make a
 * third disk image, HIGH, which bank.c puts in place by DMA as it does
 * CRYPTO and CHAIN (REQUIREMENTS.md 5.17). They are compiled without
 * LTO so they can be named here; a pattern that matches nothing is
 * silent, so check the map after touching this (gemini 5.8). */

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

MEMORY {
    ram (rw) : ORIGIN = 0x2001, LENGTH = 0xafff
    hi (rwx) : ORIGIN = 0xE000, LENGTH = 0x1F00
}

/* the rest is commodore.ld, with the window's sections placed first */
__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld
__basic_zp_size = __basic_zp_end - __basic_zp_start;
MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

INPUT(basic-header.o)
INPUT(unmap-basic.o)

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    /* BASIC program that jumps to _start. */
    .basic_header : { *(.basic_header) }

    /* the self-contained modules: the chain's client half and the row
     * log (5,967 bytes; view.c's 3.7 KB calls the screen and the UI at
     * every turn and stays under LTO in ram, 5.17) */
    .text.hi : {
        *der.o(.text .text.* .rodata .rodata.*)
        *policy.o(.text .text.* .rodata .rodata.*)
        *log.o(.text .text.* .rodata .rodata.*)
        *marks.o(.text .text.* .rodata .rodata.*)
    } > hi
    .bss.hi (NOLOAD) : {
        __hi_bss_start = .;
        *der.o(.bss .bss.*) *policy.o(.bss .bss.*) *log.o(.bss .bss.*) *marks.o(.bss .bss.*)
        __hi_bss_end = .;
    } > hi

    INCLUDE c.ld
}

/* Set initial soft stack address to end of BASIC area. (It grows down.) */
__stack = 0xd000;

/* the whole first region, then the second trimmed: build.py splits the
 * file after 2 + 0xAFFF bytes into irc.prg and HIGH */
OUTPUT_FORMAT {
    SHORT(0x2001)
    FULL(ram)
    TRIM(hi)
}
