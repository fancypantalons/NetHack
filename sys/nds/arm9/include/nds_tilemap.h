#ifndef _NDS_TILEMAP_H_
#define _NDS_TILEMAP_H_

#include <hack.h>
#include <nds.h>
#include "nds_util.h"

struct nds_tilemap_s;

typedef int (*tilemap_init_t)(struct nds_tilemap_s *map);
typedef void (*tilemap_load_tile_t)(struct nds_tilemap_s *map, int glyph, coord_t coords);
typedef void (*tilemap_write_row_t)(int tile_start_idx, int y, u16 *row);

typedef struct nds_tilemap_s {
  int tile_width;
  int tile_height;

  int tile_width_in_tiles;
  int tile_height_in_tiles;

  u16 *tile_buffer;

  int bpp;
  int palette_length;
  u16 palette[512];

  tilemap_init_t init;
  tilemap_load_tile_t loader;
  tilemap_write_row_t writer;
} nds_tilemap_t;

int nds_tilemap_init(nds_tilemap_t *map, tilemap_write_row_t writer); 

void nds_tilemap_load_tile(nds_tilemap_t *map, int tile_start_idx, int glyph, coord_t coords);

#endif
