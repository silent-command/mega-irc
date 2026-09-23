/* The client's buffers in bank 0's low RAM, below the program: the
 * family's map (mega-net PLATFORM.md) leaves $1400-$15FF and
 * $1A00-$1FAF to clients, as gemini's lowram.h uses them, and the
 * program's own headroom is what the MVP's features are paid from
 * (REQUIREMENTS.md 5.16). What is here is reached by absolute address;
 * nothing is an array, so sizeof is 2 and the length is written next
 * to each.
 *
 *   $1100  512   the disk layer's sector buffer (F011_BUF_AT)   m65_f011.c
 *   $1300  256   its BAM copy (BAM2_AT)                        m65_cbmdos.c
 *                -- both there because this client leaves through the
 *                ROM's reset (m65_exit.c), which rebuilds whatever
 *                BASIC 65 keeps in $1000-$13FF; a client that returns
 *                to BASIC any other way must not (step 3)
 *   $1400  256   the receive buffer                       conn.c
 *   $1500  256   a line folded for the screen             ircc.c
 *   $1A00  640   the TLS send buffer                      conn.c
 *   $1C80  513   the line being assembled from the stream ircc.c
 *   $1E81  201   the line being typed                     ircc.c
 *   $1F4A   13   the clock as twelve digits               ircc.c
 *   $1F58   83   a screen row being logged, and its nick's span (step 3)  view.c, log.c
 *   $1FB0        the family's exit stub: nothing past $1FAF */
#ifndef LOWRAM_H
#define LOWRAM_H

#define LOW_IN ((unsigned char *)0x1400)
#define LOW_IN_CAP 256
#define LOW_SHOWN ((char *)0x1500)
#define LOW_SHOWN_CAP 256
#define LOW_OUT ((unsigned char *)0x1A00)
#define LOW_OUT_CAP 640
#define LOW_LINE ((char *)0x1C80)
#define LOW_LINE_CAP 513
#define LOW_INPUT ((char *)0x1E81)
#define LOW_INPUT_CAP 201
#define LOW_NOW ((char *)0x1F4A)
#define LOW_ROW ((unsigned char *)0x1F58)

/* And the ROM's old screen page, $0800-$0FFF, which this client vacated
 * when its screen went to $10000 (5.7) and which only the bank's RTI at
 * $0F0F uses (5.15); nothing prints through the KERNAL. Data only, and
 * not across $0F0F (5.18):
 *   $0800  809   the TLS engine's state                    conn.c
 *   $0B30  513   a line being composed to send             ircc.c
 *   $0D40  296   the view table                            view.c
 *   $0E68   80   the TLS failure text                      conn.c
 *   $0EB8   80   a row being built: the input, the bar     ircc.c, view.c
 *   $0F10  112   the counts row                            ircc.c
 *   $0F80   64   the channels to join, comma-separated     ircc.c */
#define LOW_TLS ((void *)0x0800)
#define LOW_TMP ((char *)0x0B30)
#define LOW_VIEWS ((void *)0x0D40)
#define LOW_FAIL ((char *)0x0E68)
#define LOW_FAIL_CAP 80
#define LOW_SCRATCH ((char *)0x0EB8)
#define LOW_COUNTS ((char *)0x0F10)
#define LOW_COUNTS_CAP 112
#define LOW_CHANNEL ((char *)0x0F80)
#define LOW_CHANNEL_CAP 64
/* $0FC0  33  the NickServ password, typed and never written to disk
 * (section 2); the last of the page, short of $1000 */
#define LOW_NSPASS ((char *)0x0FC0)
#define LOW_NSPASS_CAP 33

#endif
