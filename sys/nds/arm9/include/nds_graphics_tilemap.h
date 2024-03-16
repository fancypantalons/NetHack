#ifndef _NDS_GRAPHICS_TILEMAP_H_
#define _NDS_GRAPHICS_TILEMAP_H_

#include "nds_tilemap.h"
#include "bmp.h"

typedef struct {
  struct nds_tilemap_s base;

  char *fname;

  bmp_t tiles;

  int width_in_tiles;
  int height_in_tiles;
} nds_graphics_tilemap_t;

nds_tilemap_t *nds_graphics_tilemap_new(char *fname, int tile_width, int tile_height);

#endif
