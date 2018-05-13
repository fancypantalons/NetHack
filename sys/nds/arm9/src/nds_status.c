#include <nds.h>
#include <stdlib.h>
#include <string.h>

#include "hack.h"

#include "nds_debug.h"
#include "nds_win.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "nds_map.h"
#include "ppm-lite.h"

#define CUTCOUNT 2

struct ppm *status_img = NULL;

nds_charbuf_t *status_lines = NULL;
nds_charbuf_t *last_status_lines = NULL;

int name_printed = 0;

int status_bottom = 0;

void nds_status_putstr(char *str)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  int text_h = system_font->height;

  rectangle_t map_rect = nds_minimap_dims();
  int status_x = 2;
  int status_y = RECT_END_Y(map_rect) + 2;

  if (status_img == NULL) {
    status_img = alloc_ppm(256, system_font->height);
    status_lines = nds_charbuf_create(1);
  }

  if (status_lines->count == 0) {
    char *name;
    int i;

    for (i = 0; i < strlen(str); i++) {
      if ((str[i] == ':') && (strncmp(str + i - 2, "St", 2) == 0)) {
        name = str;
        str[i - 3] = '\0';
        str = str + i - 2;
      }
    }

    if (! name_printed) {
      nds_draw_text(system_font, name,
                    3, 0, vram);

      name_printed = 1;
    }

    nds_charbuf_append(status_lines, str, 0);
  } else {
    nds_charbuf_t *wrapped;
    int i;

    nds_charbuf_append(status_lines, str, 0);

    wrapped = nds_charbuf_wrap(status_lines, 256 - status_x);

    for (i = 0; i < wrapped->count; i++) {
      if ((last_status_lines != NULL) &&
          (i < last_status_lines->count) && 
          (strcmp(wrapped->lines[i].text, last_status_lines->lines[i].text) == 0)) {

        continue;
      }

      clear_ppm(status_img, MAP_COLOUR(CLR_BLACK));

      draw_string(system_font, wrapped->lines[i].text, status_img, 
                  0, 0, 
                  -1, -1);

      draw_ppm(status_img, vram, status_x, status_y + text_h * i, 256);
    }

    status_bottom = status_y + wrapped->count * text_h + 2;

    nds_draw_hline(0, status_bottom - 2, 256, 0, vram);
    nds_draw_hline(0, status_bottom - 1, 256, 0, vram);

    if (last_status_lines) {
      nds_charbuf_destroy(last_status_lines);
    }

    nds_charbuf_destroy(status_lines);

    status_lines = nds_charbuf_create(1);
    last_status_lines = wrapped;
  }
}

int nds_status_get_bottom()
{
  return status_bottom;
}

/*
 * The following data structures come from the genl_ routines in
 * src/windows.c and as such are considered to be on the window-port
 * "side" of things, rather than the NetHack-core "side" of things.
 */

extern const char *status_fieldfmt[MAXBLSTATS];
extern char *status_vals[MAXBLSTATS];
extern boolean status_activefields[MAXBLSTATS];
extern winid WIN_STATUS;

static long nds_condition_bits;
static int nds_status_colors[MAXBLSTATS];
int hpbar_percent, hpbar_color;

static int FDECL(condcolor, (long, unsigned long *));
static int FDECL(condattr, (long, unsigned long *));

char NEARDATA *hilites[CLR_MAX]; /* terminal escapes for the various colors */

static void init_hilite()
{
  int c;
  int backg = NO_COLOR;
  int foreg = NO_COLOR;

  int hi_backg = NO_COLOR;
  int hi_foreg = NO_COLOR | BRIGHT;

  for (c = 0; c < SIZE(hilites); c++) {
    hilites[c] = "\033[1m";
  }

  hilites[CLR_GRAY] = hilites[NO_COLOR] = (char *) 0;

  for (c = 0; c < SIZE(hilites); c++) {
    if ((backg & ~BRIGHT) != c) {
      if (c == foreg) {
        hilites[c] = (char *) 0;
      } else if (c != hi_foreg || backg != hi_backg) {
        hilites[c] = (char *) alloc(sizeof("\033[%d;3%d;4%dm"));

        Sprintf(hilites[c], "\033[%d", !!(c & BRIGHT));

        if ((c | BRIGHT) != (foreg | BRIGHT))
            Sprintf(eos(hilites[c]), ";3%d", c & ~BRIGHT);

        if (backg != CLR_BLACK)
            Sprintf(eos(hilites[c]), ";4%d", backg & ~BRIGHT);

        Strcat(hilites[c], "m");
      }
    }
  }
}

void nds_status_init()
{
  int i;

  for (i = 0; i < MAXBLSTATS; ++i) {
    nds_status_colors[i] = NO_COLOR; /* no color */
  }

  nds_condition_bits = 0L;
  hpbar_percent = 0, hpbar_color = NO_COLOR;

  init_hilite();

  /* let genl_status_init do most of the initialization */
  genl_status_init();
}

void _start_add_to_buffer(char *buffer, int attridx, int coloridx) 
{
  if ((attridx) & HL_BOLD) {
    strcat(buffer, "\033[1m");
  }

  if ((attridx) & HL_INVERSE) {
    strcat(buffer, "\033[7m");
  }

  if ((attridx) & HL_ULINE) {
    strcat(buffer, "\033[4m");
  }

  if ((coloridx != NO_COLOR) && (coloridx != CLR_MAX)) {
    strcat(buffer, hilites[coloridx]);
  }
}

void _end_add_to_buffer(char *buffer, int attridx, int coloridx) 
{
  if ((coloridx != NO_COLOR) && (coloridx != CLR_MAX)) {
    strcat(buffer, "\033[39;49m");
  }

  if ((attridx) & HL_ULINE) {
    strcat(buffer, "\033[24m");
  }

  if ((attridx) & HL_INVERSE) {
    strcat(buffer, "\033[27m");
  }

  if ((attridx) & HL_BOLD) {
    strcat(buffer, "\033[2m");
  }
}

void _status_add_to_buffer(char *buffer, int attridx, int coloridx, char *str) 
{
  _start_add_to_buffer(buffer, attridx, coloridx);
  strcat(buffer, str);
  _end_add_to_buffer(buffer, attridx, coloridx);
}

#define MaybeDisplayCond(bm,txt) \
            if (nds_condition_bits & (bm)) {                                  \
                strcat(buffer, " ");                                          \
                if (iflags.hilite_delta) {                                    \
                    attrmask = condattr(bm, colormasks);                      \
                    coloridx = condcolor(bm, colormasks);                     \
                    _status_add_to_buffer(buffer, attrmask, coloridx, txt);                  \
                } else {                                                      \
                  strcat(buffer, txt);                                        \
                }                                                             \
            }

void nds_status_update(int fldidx, genericptr_t ptr, int chg, int percent, int color, unsigned long *colormasks)
{
  char buffer[256] = "";

  long *condptr = (long *) ptr;
  int i, attrmask = 0;
  int coloridx = NO_COLOR;
  char *text = (char *) ptr;
  static boolean oncearound = FALSE; /* prevent premature partial display */

  enum statusfields fieldorder[2][15] = {
    { BL_TITLE, BL_STR, BL_DX, BL_CO, BL_IN, BL_WI, BL_CH, BL_ALIGN,
      BL_SCORE, BL_FLUSH, BL_FLUSH, BL_FLUSH, BL_FLUSH, BL_FLUSH,
      BL_FLUSH },
    { BL_LEVELDESC, BL_GOLD, BL_HP, BL_HPMAX, BL_ENE, BL_ENEMAX,
      BL_AC, BL_XP, BL_EXP, BL_HD, BL_TIME, BL_HUNGER,
      BL_CAP, BL_CONDITION, BL_FLUSH }
  };

  int attridx = 0;

  if (fldidx != BL_FLUSH) {
    if (!status_activefields[fldidx]) {
      return;
    }

    if (fldidx == BL_CONDITION) {
      nds_condition_bits = *condptr;
      oncearound = TRUE;
    } else {
      Sprintf(status_vals[fldidx],
              (fldidx == BL_TITLE && iflags.wc2_hitpointbar) ? "%-30s" :
              status_fieldfmt[fldidx] ? status_fieldfmt[fldidx] : "%s",
              text);

      nds_status_colors[fldidx] = color;

      if (iflags.wc2_hitpointbar && fldidx == BL_HP) {
        hpbar_percent = percent;
        hpbar_color = color;
      }
    }
  }

  if (!oncearound) {
    return;
  }

  for (i = 0; fieldorder[0][i] != BL_FLUSH; ++i) {
    int fldidx1 = fieldorder[0][i];

    if (status_activefields[fldidx1]) {
      if (fldidx1 != BL_TITLE || !iflags.wc2_hitpointbar) {
        coloridx = nds_status_colors[fldidx1] & 0x00FF;
        attridx = (nds_status_colors[fldidx1] & 0xFF00) >> 8;
        text = status_vals[fldidx1];

        if (iflags.hilite_delta) {
          if (*text == ' ') {
            strcat(buffer, " ");
            text++;
          }

          _status_add_to_buffer(buffer, attridx, coloridx, text);
        } else {
          strcat(buffer, text);
        }
      } else {
        /* hitpointbar using hp percent calculation */
        int bar_pos, bar_len;
        char *bar2 = (char *)0;
        char bar[MAXCO], savedch;
        boolean twoparts = FALSE;

        text = status_vals[fldidx1];
        bar_len = strlen(text);

        if (bar_len < MAXCO-1) {
          Strcpy(bar, text);
          bar_pos = (bar_len * hpbar_percent) / 100;

          if (bar_pos < 1 && hpbar_percent > 0) {
              bar_pos = 1;
          }

          if (bar_pos >= bar_len && hpbar_percent < 100) {
              bar_pos = bar_len - 1;
          }

          if (bar_pos > 0 && bar_pos < bar_len) {
              twoparts = TRUE;
              bar2 = &bar[bar_pos];
              savedch = *bar2;
              *bar2 = '\0';
          }
        }

        if (iflags.hilite_delta && iflags.wc2_hitpointbar) {
          strcat(buffer, "[");
          coloridx = hpbar_color & 0x00FF;

          _status_add_to_buffer(buffer, ATR_INVERSE, coloridx, bar);

          if (twoparts) {
            *bar2 = savedch;
            strcat(buffer, bar2);
          }

          strcat(buffer, "]");
        } else {
          strcat(buffer, text);
        }
      }
    }
  }

  strcat(buffer, "\033[m");
  putstr(WIN_STATUS, 0, buffer);

  buffer[0] = '\0';

  for (i = 0; fieldorder[1][i] != BL_FLUSH; ++i) {
    int fldidx2 = fieldorder[1][i];

    if (status_activefields[fldidx2]) {
      if (fldidx2 != BL_CONDITION) {
        coloridx = nds_status_colors[fldidx2] & 0x00FF;
        attridx = (nds_status_colors[fldidx2] & 0xFF00) >> 8;
        text = status_vals[fldidx2];

        if (iflags.hilite_delta) {
          if (*text == ' ') {
           strcat(buffer, " ");
            text++;
          }

          if (fldidx2 == BL_GOLD) {
            _status_add_to_buffer(buffer, attridx, coloridx, text);
            strcat(buffer, " ");
          } else {
            _status_add_to_buffer(buffer, attridx, coloridx, text);
          }
        } else {
          strcat(buffer, text);
        }
      } else {
        MaybeDisplayCond(BL_MASK_STONE, "Stone");
        MaybeDisplayCond(BL_MASK_SLIME, "Slime");
        MaybeDisplayCond(BL_MASK_STRNGL, "Strngl");
        MaybeDisplayCond(BL_MASK_FOODPOIS, "FoodPois");
        MaybeDisplayCond(BL_MASK_TERMILL, "TermIll");
        MaybeDisplayCond(BL_MASK_BLIND, "Blind");
        MaybeDisplayCond(BL_MASK_DEAF, "Deaf");
        MaybeDisplayCond(BL_MASK_STUN, "Stun");
        MaybeDisplayCond(BL_MASK_CONF, "Conf");
        MaybeDisplayCond(BL_MASK_HALLU, "Hallu");
        MaybeDisplayCond(BL_MASK_LEV, "Lev");
        MaybeDisplayCond(BL_MASK_FLY, "Fly");
        MaybeDisplayCond(BL_MASK_RIDE, "Ride");
      }
    }
  }

  strcat(buffer, "\033[m");

  /* putmixed() due to GOLD glyph */
  putmixed(WIN_STATUS, 0, buffer);

  return;
}

/*
 * Return what color this condition should
 * be displayed in based on user settings.
 */
int condcolor(long bm, unsigned long *bmarray)
{
  int i;

  if (bm && bmarray) {
    for (i = 0; i < CLR_MAX; ++i) {
      if (bmarray[i] && (bm & bmarray[i])) {
        return i;
      }
    }
  }

  return NO_COLOR;
}

int condattr(long bm, unsigned long *bmarray)
{
  int attr = 0;
  int i;

  if (bm && bmarray) {
    for (i = HL_ATTCLR_DIM; i < BL_ATTCLR_MAX; ++i) {
      if (bmarray[i] && (bm & bmarray[i])) {
        switch(i) {
          case HL_ATTCLR_DIM:
            attr |= HL_DIM;
            break;

          case HL_ATTCLR_BLINK:
            attr |= HL_BLINK;
            break;

          case HL_ATTCLR_ULINE:
            attr |= HL_ULINE;
            break;

          case HL_ATTCLR_INVERSE:
            attr |= HL_INVERSE;
            break;

          case HL_ATTCLR_BOLD:
            attr |= HL_BOLD;
            break;
        }
      }
    }
  }

  return attr;
}
