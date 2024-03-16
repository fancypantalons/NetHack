#include "nds_tilemap.h"

int nds_tilemap_init(nds_tilemap_t *map, tilemap_write_row_t writer) 
{
  int ret = map->init(map);

  if (ret >= 0) {
    // This is more space than we need for 16 bpp images, but meh, it's a buffer...
    map->tile_buffer = (u16 *)malloc(map->tile_width * map->tile_width / 2);
    map->writer = writer;
  }

  map->tile_width_in_tiles = map->tile_width / 8;
  map->tile_height_in_tiles = map->tile_height / 8;

  return ret;
}

void nds_tilemap_load_tile(nds_tilemap_t *map, int tile_start_idx, int glyph, coord_t coords) 
{
  // Load the tile image into the map buffer
  map->loader(map, glyph, coords);

  // Now write each tile row to graphics memory
  //
  // Note, at this point, the graphics are organized as 8x8 tile blocks
  // from left to right and top to bottom.
  int i, j;
  int bpp = map->bpp;

  for (j = 0; j < map->tile_height_in_tiles; j++) {
    for (i = 0; i < map->tile_width_in_tiles; i++) {
      int y;
      int subtile_start_idx = tile_start_idx + j * map->tile_width_in_tiles + i; 

      for (y = 0; y < 8; y++) {
        int tile_row_start = (j * map->tile_width_in_tiles + i) * bpp * 8 / 2 + y * bpp / 2;

        map->writer(subtile_start_idx, y, map->tile_buffer + tile_row_start);
      }
    }
  }
}
