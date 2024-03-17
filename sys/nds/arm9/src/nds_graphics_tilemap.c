#include "nds_graphics_tilemap.h"
#include "nds_debug.h"

extern short glyph2tile[];

void _nds_tilemap_load_graphics_tile(nds_tilemap_t *tilemap, int glyph, coord_t coords) 
{
  nds_graphics_tilemap_t *gmap = (nds_graphics_tilemap_t *)tilemap;

  int tileidx = glyph2tile[glyph];
  int row_bytes;
  int bpp;
  int bmp_tile_x, bmp_tile_y;

  u8 *bmp_row_start;
  u16 *tile_row_start;

  int block_row_start = 0;

  static int last_row_start = 0;
  static u8 *buffer = NULL;

  /*
   * Compute the number of bytes that makes up a row of an individual NetHack
   * tile.
   */

  bpp = tilemap->bpp;
  row_bytes = tilemap->tile_width / 8 * bpp;

  /* Now calculate the pointer which points to the start of the BMP row */

  bmp_tile_y = tileidx / gmap->width_in_tiles;
  bmp_tile_x = tileidx % gmap->width_in_tiles;

  block_row_start = bmp_tile_y * tilemap->tile_height;

  /* 
   * Wee optimization here.  The game tiles tend to be grouped so holding onto
   * the last row we loaded can improve performance.
   */
  if ((buffer == NULL) || (block_row_start != last_row_start)) {
    bmp_read_rows(&(gmap->tiles), 
                  block_row_start,
                  tilemap->tile_height,
                  &buffer);

    last_row_start = block_row_start;
  }

  bmp_row_start = buffer + bmp_tile_x * row_bytes;

  int i, j, x;

  for (j = 0; j < tilemap->tile_height_in_tiles; j++) {
    int y;

    for (y = 0; y < 8; y++) {
      for (i = 0; i < tilemap->tile_width_in_tiles; i++) {
        /* Again, this works because 8 * (bpp / 8) == bpp */

        tile_row_start = tilemap->tile_buffer + (j * tilemap->tile_width_in_tiles + i) * bpp * 8 / 2 + y * bpp / 2; 

        for (x = 0; x < bpp; x += 2, bmp_row_start += 2) {
          if (bpp == 4) {
            u16 a, b, c, d;

            a = (bmp_row_start[0] & 0xF0) >> 4;
            b = (bmp_row_start[0] & 0x0F);
            c = (bmp_row_start[1] & 0xF0) >> 4;
            d = (bmp_row_start[1] & 0x0F);

            tile_row_start[x / 2] = (d << 12) | (c << 8) | (b << 4) | (a << 0);
          } else {
            tile_row_start[x / 2] = (bmp_row_start[1] << 8) |
                                     bmp_row_start[0];
          }
        }
      }

      bmp_row_start += row_bytes * gmap->width_in_tiles - row_bytes;
    }
  }
}

int _nds_tilemap_graphics_init(nds_tilemap_t *tilemap) 
{
  nds_graphics_tilemap_t *gmap = (nds_graphics_tilemap_t *)tilemap;

  /* Now load the tiles into memory */

  if (bmp_open(gmap->fname, &(gmap->tiles)) < 0) {
    return -1;
  }

  /* Compute the width and height of our image, in tiles */

  gmap->width_in_tiles = bmp_width(&(gmap->tiles)) / tilemap->tile_width;
  gmap->height_in_tiles = bmp_height(&(gmap->tiles)) / tilemap->tile_width;

  /* Alright, file loaded, let's copy over the palette */

  tilemap->palette_length = gmap->tiles.palette_length;
  tilemap->bpp = bmp_bpp(&(gmap->tiles));

  int i;

  for (i = 0; i < gmap->tiles.palette_length; i++) {
    u16 val = RGB15((gmap->tiles.palette[i].r >> 3),
                    (gmap->tiles.palette[i].g >> 3),
                    (gmap->tiles.palette[i].b >> 3));

    tilemap->palette[i] = val;
  }

  tilemap->loader = &_nds_tilemap_load_graphics_tile;

  return tilemap->bpp;
}

nds_tilemap_t *nds_graphics_tilemap_new(char *fname, int tile_width, int tile_height) 
{
  nds_graphics_tilemap_t *gmap = (nds_graphics_tilemap_t *)malloc(sizeof(nds_graphics_tilemap_t));

  gmap->fname = fname;

  gmap->base.tile_width = tile_width;
  gmap->base.tile_height = tile_height;

  gmap->base.init = &_nds_tilemap_graphics_init;

  return (nds_tilemap_t *)gmap;
}
