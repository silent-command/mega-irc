/* Bookmarks: IRC.CFG on the disk the client booted from, as mega-ftp
 * keeps FTPC.CFG, so the file travels with the program. A line "IRC1",
 * then five lines per entry: host, port, TLS (y or n), nick, the
 * channels to join (comma-separated). No passwords: the disk is also
 * the distribution medium (REQUIREMENTS.md section 2). The text stays
 * in bank 1 above the font copy, which nothing else of this client's
 * uses, and is read field by field into the caller's buffers. In the
 * HIGH window: nothing here runs before bank_boot (5.17). */
#ifndef MARKS_H
#define MARKS_H

#define MARKS_MAX 8
#define MARKS_NONE 0xff
#define MARKS_HOST 64
#define MARKS_PORT 6
#define MARKS_TLS 2
#define MARKS_NICK 17
#define MARKS_CHANS 64

extern unsigned char marks_count;

void marks_load(unsigned char drive);
/* Entry i into the five buffers (their sizes above); 1 if it exists. */
unsigned char marks_get(unsigned char i, char *host, char *port, char *tls, char *nick, char *chans);
/* Appends and writes the file; 0 when the list is full or the write failed. */
unsigned char marks_add(const char *host, const char *port, const char *tls, const char *nick, const char *chans);
/* Removes entry i and writes the file; 0 when the write failed. */
unsigned char marks_remove(unsigned char i);

#endif
