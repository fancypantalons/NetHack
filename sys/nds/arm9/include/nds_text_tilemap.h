#ifndef _NDS_text_TILEMAP_H_
#define _NDS_text_TILEMAP_H_

#include "nds_tilemap.h"

typedef struct {
  struct nds_tilemap_s base;

  char *font_fname;
  char *palette_fname;

  struct font *font;
  struct ppm *text_img;

  int char_w;
  int char_h;
} nds_text_tilemap_t;

nds_tilemap_t *nds_text_tilemap_new(char *font_fname, char *palette_fname);

#endif
