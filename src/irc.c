#include "irc.h"

uint8_t irc_parse(char *line, irc_msg *m)
{
  char *p = line, *q;
  uint8_t i;

  m->tags = m->prefix = m->host = m->cmd = m->trailing = 0;
  m->numeric = 0;
  m->nparams = 0;
  for (i = 0; i < IRC_PARAMS_MAX; i++) m->params[i] = 0;
  if (!p) return 0;

  /* IRCv3 message tags, "@k=v;k=v " ahead of everything else. This
   * client negotiates no capabilities, so none should arrive; they are
   * stepped over rather than trusted, because a line that began with one
   * would otherwise have its tag read as the command. */
  if (*p == '@') {
    m->tags = ++p;
    while (*p && *p != ' ') p++;
    if (!*p) return 0;                     /* tags and nothing after them */
    *p++ = 0;
    while (*p == ' ') p++;
  }

  if (*p == ':') {
    m->prefix = ++p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;
    for (q = m->prefix; *q; q++)
      if (*q == '!') { *q = 0; m->host = q + 1; break; }
    while (*p == ' ') p++;
  }

  if (!*p) return 0;                       /* a prefix with no command */
  m->cmd = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = 0;

  /* Three digits and nothing else is a numeric reply. */
  if (m->cmd[0] >= '0' && m->cmd[0] <= '9' &&
      m->cmd[1] >= '0' && m->cmd[1] <= '9' &&
      m->cmd[2] >= '0' && m->cmd[2] <= '9' && !m->cmd[3])
    m->numeric = (uint16_t)((m->cmd[0] - '0') * 100 +
                            (m->cmd[1] - '0') * 10 +
                            (m->cmd[2] - '0'));

  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    if (*p == ':') { m->trailing = p + 1; break; }
    /* Past IRC_PARAMS_MAX the parameter is walked but not kept, so the
     * trailing is still found on a line with more of them than this
     * client has any use for. */
    if (m->nparams < IRC_PARAMS_MAX) m->params[m->nparams++] = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;
  }
  return 1;
}

uint8_t irc_is(const irc_msg *m, const char *name)
{
  const char *a = m->cmd;
  if (!a) return 0;
  while (*a && *name && *a == *name) { a++; name++; }
  return (uint8_t)(!*a && !*name);
}

uint8_t irc_ctcp(irc_msg *m, char *tag, uint8_t tagcap)
{
  char *p;
  uint8_t n = 0;

  if (!m->trailing || m->trailing[0] != 1 || tagcap == 0) return 0;
  p = m->trailing + 1;
  while (*p && *p != 1 && *p != ' ') {
    if ((uint8_t)(n + 1) < tagcap) tag[n++] = *p;
    p++;
  }
  tag[n] = 0;
  if (n == 0) return 0;                    /* "\001\001" is not a request */

  if (*p == ' ') p++;
  m->trailing = p;
  while (*p) { if (*p == 1) { *p = 0; break; } p++; }
  return 1;
}
