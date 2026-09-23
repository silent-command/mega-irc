#include <string.h>
#include "mega65/memory.h"
#include "m65_cbmdos.h"
#include "lowram.h"
#include "marks.h"

#define MARKS_FILE "IRC.CFG"
#define MARKS_MAGIC "IRC1"
#define MARKS_FAR 0x11800UL               /* bank 1, above the font copy at $11000; mega-ftp's slot, one program at a time */
#define MARKS_CAP 2048
#define FIELDS 5

unsigned char marks_count;
static unsigned int text_len;
static unsigned char marks_drive;
#define line LOW_LINE                     /* one field of the file, in the stream's line buffer, idle whenever this runs (before any connection) */
#define LINE_CAP 96

/* Copies the line at `at` into `out` (up to cap - 1 characters) and
 * returns the offset just past its newline, or 0xffff at the end. */
static unsigned int read_line(unsigned int at, char *out, unsigned char cap)
{
  unsigned int chunk = text_len - at, i;
  unsigned char n = 0;
  if (at >= text_len) return 0xffff;
  if (chunk > LINE_CAP) chunk = LINE_CAP;
  lcopy(MARKS_FAR + at, (unsigned long)(unsigned int)line, chunk);
  for (i = 0; i < chunk && line[i] != '\n'; i++)
    if (n < cap - 1) out[n++] = line[i];
  out[n] = 0;
  return (unsigned int)(at + i + 1);                /* past the newline, or past the end */
}

/* The offset of entry i's first line, or 0xffff. */
static unsigned int entry_at(unsigned char i)
{
  unsigned int at;
  unsigned char k;
  char t[2];
  at = read_line(0, t, sizeof t);                   /* the magic line */
  for (k = 0; k < i * FIELDS && at != 0xffff; k++) at = read_line(at, t, sizeof t);
  return at;
}

static void count_entries(void)
{
  unsigned int at = 0;
  unsigned char lines = 0;
  char t[2];
  marks_count = 0;
  while ((at = read_line(at, t, sizeof t)) != 0xffff) lines++;
  if (lines) marks_count = (unsigned char)((lines - 1) / FIELDS);
  if (marks_count > MARKS_MAX) marks_count = MARKS_MAX;
}

void marks_load(unsigned char drive)
{
  marks_drive = drive;
  text_len = (unsigned int)cbmdos_load(MARKS_FILE, drive, MARKS_FAR, MARKS_CAP);
  marks_count = 0;
  if (!text_len) return;
  read_line(0, line, LINE_CAP);
  if (strcmp(line, MARKS_MAGIC)) { text_len = 0; return; }   /* not ours, or newer: ignored, not guessed */
  count_entries();
}

unsigned char marks_get(unsigned char i, char *host, char *port, char *tls, char *nick, char *chans)
{
  unsigned int at;
  if (i >= marks_count) return 0;
  at = entry_at(i);
  if (at == 0xffff) return 0;
  at = read_line(at, host, MARKS_HOST);
  at = read_line(at, port, MARKS_PORT);
  at = read_line(at, tls, MARKS_TLS);
  at = read_line(at, nick, MARKS_NICK);
  read_line(at, chans, MARKS_CHANS);
  return 1;
}

static unsigned char write_file(void)
{
  unsigned int i;
  cbmdos_delete(MARKS_FILE, marks_drive);           /* absent the first time; fine */
  if (cbmdos_create(MARKS_FILE, marks_drive) != CBMDOS_OK) return 0;
  for (i = 0; i < text_len; i++)
    if (cbmdos_put(lpeek(MARKS_FAR + i)) != CBMDOS_OK) { cbmdos_close(); return 0; }
  return cbmdos_close() == CBMDOS_OK;
}

/* Appends one field as a line of the far text. */
static unsigned char put_field(const char *s)
{
  unsigned int n = (unsigned int)strlen(s);
  if (text_len + n + 1 > MARKS_CAP) return 0;
  if (n) lcopy((unsigned long)(unsigned int)s, MARKS_FAR + text_len, n);
  lpoke(MARKS_FAR + text_len + n, '\n');
  text_len = (unsigned int)(text_len + n + 1);
  return 1;
}

unsigned char marks_add(const char *host, const char *port, const char *tls, const char *nick, const char *chans)
{
  unsigned int was = text_len;
  if (marks_count >= MARKS_MAX) return 0;
  if (!text_len) { strcpy(line, MARKS_MAGIC); if (!put_field(line)) return 0; }
  if (!put_field(host) || !put_field(port) || !put_field(tls) || !put_field(nick) || !put_field(chans)) { text_len = was; return 0; }
  marks_count++;
  if (write_file()) return 1;
  text_len = was; count_entries();
  return 0;
}

unsigned char marks_remove(unsigned char i)
{
  unsigned int from, to, tail;
  if (i >= marks_count) return 0;
  from = entry_at(i);
  to = entry_at((unsigned char)(i + 1));
  if (from == 0xffff) return 0;
  if (to == 0xffff) to = text_len;
  tail = text_len - to;
  if (tail) lcopy(MARKS_FAR + to, MARKS_FAR + from, tail);   /* forwards: the destination is below the source */
  text_len = (unsigned int)(text_len - (to - from));
  marks_count--;
  return write_file();
}
