#include "mega65/memory.h"
#include "m65_screen.h"
#include "ui.h"
#include "lowram.h"
#include "log.h"
#include "view.h"

#define SCREEN 0x10000UL
#define COLOUR 0xff80000UL

typedef struct {
  char name[VIEW_NAME_CAP];
  uint16_t back;               /* rows scrolled back from the newest */
  uint8_t used, unread;
} view;

#define views ((view *)LOW_VIEWS)      /* the ROM's old screen page (lowram.h); set up by view_init */
uint8_t view_active;

/* The keys are positions, the views are slots, and the two are tied by
 * this order: position p (F1 F3 F5 F7 F2 F4 F6 F8) shows views[order[p]].
 * A view's log ring is addressed by its slot, so a slot never moves;
 * closing a view takes its position out of the order and the ones after
 * it step down, so the keys never skip. Without this, parting the F3
 * channel left F5 and F7 in place with a hole at F3 (5.29). */
static uint8_t order[VIEW_MAX];
static uint8_t positions;                         /* in use, the status view included */

uint8_t view_rows(void) { return (uint8_t)(m65_screen_rows() - 5 - VIEW_ROW_FIRST + 1); }

uint8_t same_ci(const char *a, const char *b)
{
  if (!a || !b) return 0;
  uint8_t x, y;
  for (;; a++, b++) {
    x = (uint8_t)*a; y = (uint8_t)*b;
    if (x >= 'A' && x <= 'Z') x = (uint8_t)(x + 32);
    if (y >= 'A' && y <= 'Z') y = (uint8_t)(y + 32);
    if (x != y) return 0;
    if (!x) return 1;
  }
}

void view_init(void)
{
  uint8_t v;
  for (v = 0; v < VIEW_MAX; v++) { views[v].used = 0; log_clear(v); }
  order[0] = 0; positions = 1;
  views[0].used = 1; views[0].name[0] = 's'; views[0].name[1] = 't'; views[0].name[2] = 'a'; views[0].name[3] = 't'; views[0].name[4] = 'u'; views[0].name[5] = 's'; views[0].name[6] = 0;
  view_show(0);
}

uint8_t view_open(const char *name)
{
  uint8_t v, i;
  for (v = 1; v < VIEW_MAX; v++)
    if (!views[v].used) {
      views[v].used = 1; views[v].unread = 0; views[v].back = 0;
      for (i = 0; name[i] && i < VIEW_NAME_CAP - 1; i++) views[v].name[i] = name[i];
      views[v].name[i] = 0;
      log_clear(v);
      order[positions++] = v;
      view_bar();
      return v;
    }
  return VIEW_NONE;
}

void view_close(uint8_t v)
{
  uint8_t p;
  if (!v || v >= VIEW_MAX || !views[v].used) return;
  views[v].used = 0;
  for (p = 1; p < positions && order[p] != v; p++) ;
  for (; p + 1 < positions; p++) order[p] = order[p + 1];   /* the ones after it step down */
  positions--;
  if (view_active == v) view_show(0); else view_bar();
}

uint8_t view_find(const char *name)
{
  uint8_t v;
  for (v = 1; v < VIEW_MAX; v++) if (views[v].used && same_ci(views[v].name, name)) return v;
  return VIEW_NONE;
}

const char *view_name(uint8_t v) { return views[v].name; }
uint8_t view_at(uint8_t pos) { return pos < positions ? order[pos] : VIEW_NONE; }
uint8_t view_is_channel(uint8_t v) { return (uint8_t)(v && v < VIEW_MAX && views[v].used); }

/* The chat area up one row, colour and all, and the row just logged at
 * its foot: drawn from the log rather than from LOW_ROW, so the nick's
 * colour is painted by the one routine that knows it (log_draw). */
static void live_row(void)
{
  uint8_t rows = view_rows(), last = (uint8_t)(VIEW_ROW_FIRST + rows - 1);
  uint16_t n = (uint16_t)(rows - 1) * LOG_ROW;
  lcopy(SCREEN + (VIEW_ROW_FIRST + 1) * LOG_ROW, SCREEN + VIEW_ROW_FIRST * LOG_ROW, n);
  lcopy(COLOUR + (VIEW_ROW_FIRST + 1) * LOG_ROW, COLOUR + VIEW_ROW_FIRST * LOG_ROW, n);
  lfill(COLOUR + (uint32_t)last * LOG_ROW, m65_screen_text_colour(), LOG_ROW);
  log_draw(view_active, (uint16_t)(log_count(view_active) - 1), last, 1);
}

void view_line(uint8_t v, const char *s)
{
  uint8_t n, first = 1;
  if (v >= VIEW_MAX || !views[v].used) v = 0;
  while (*s || first) {
    n = 0;
    if (!first) { LOW_ROW[n++] = 0x20; LOW_ROW[n++] = 0x20; }
    while (*s && n < 79) LOW_ROW[n++] = (unsigned char)m65_ascii_to_screencode(*s++);
    while (n < LOG_ROW) LOW_ROW[n++] = 0x20;
    log_row(v, LOW_ROW);
    if (v == view_active && !views[v].back) live_row();
    first = 0;
  }
  if (v != view_active && !views[v].unread) { views[v].unread = 1; view_bar(); }
}

static void redraw(void)
{
  uint8_t rows = view_rows(), shown;
  uint16_t count = log_count(view_active), avail, top;
  uint16_t back = views[view_active].back;
  avail = count > back ? (uint16_t)(count - back) : 0;
  if (avail > rows) { shown = rows; top = (uint16_t)(avail - rows); } else { shown = (uint8_t)avail; top = 0; }
  lfill(COLOUR + VIEW_ROW_FIRST * LOG_ROW, m65_screen_text_colour(), (uint16_t)rows * LOG_ROW);   /* the whole area's colour at once */
  if (shown < rows)                               /* a short log sits at the foot, where the live rows go */
    lfill(SCREEN + VIEW_ROW_FIRST * LOG_ROW, 0x20, (uint16_t)(rows - shown) * LOG_ROW);
  if (shown) log_draw(view_active, top, (uint8_t)(VIEW_ROW_FIRST + rows - shown), shown);
}

void view_show(uint8_t v)
{
  if (v >= VIEW_MAX || !views[v].used) return;
  view_active = v;
  views[v].unread = 0;
  redraw();
  view_bar();
}

void view_scroll(int16_t rows)
{
  uint16_t count = log_count(view_active), most = view_rows();
  int16_t back;
  most = count > most ? (uint16_t)(count - most) : 0;   /* the oldest row at the top of the area, no further */
  back = rows ? (int16_t)(views[view_active].back + rows) : 0;   /* back and rows are both under 8192: no overflow */
  if (back < 0) back = 0;
  if ((uint16_t)back > most) back = (int16_t)most;
  views[view_active].back = (uint16_t)back;
  redraw();
}

uint16_t view_back(void) { return views[view_active].back; }

void view_bar(void)
{
  char *bar = LOW_SCRATCH;
  static const char keys[VIEW_MAX] = { '1', '3', '5', '7', '2', '4', '6', '8' };
  char *p = bar, *e = bar + 79;
  const char *n;
  uint8_t v, i, pos;
  for (pos = 0; pos < positions; pos++) {
    v = order[pos];
    if (p > bar && p < e) *p++ = ' ';
    if (p < e) *p++ = (char)(v == view_active ? '[' : ' ');
    if (p < e) *p++ = 'F'; if (p < e) *p++ = keys[pos]; if (p < e) *p++ = ' ';
    for (n = views[v].name, i = 0; *n && i < 14 && p < e; n++, i++) *p++ = *n;
    if (views[v].unread && p < e) *p++ = '*';
    if (p < e) *p++ = (char)(v == view_active ? ']' : ' ');
  }
  *p = 0;
  ui_line(0, bar, 0);
}
