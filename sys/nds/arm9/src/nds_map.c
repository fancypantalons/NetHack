/*
 * Tile management functions.
 */

#include <nds.h>
#include "hack.h"

#include "bmp.h"
#include "ppm-lite.h"
#include "font-bdf.h"

#include "nds_debug.h"
#include "nds_win.h"
#include "nds_util.h"
#include "nds_map.h"
#include "nds_gfx.h"

#include "nds_graphics_tilemap.h"
#include "nds_text_tilemap.h"

#define FONT_FILE_NAME          "map.bdf"
#define FONT_PALETTE_NAME       "map.pal"

#define MAP_PALETTE_BASE        32

#define TILE_WIDTH		iflags.wc_tile_width
#define TILE_HEIGHT		iflags.wc_tile_height
#define TILE_FILE		iflags.wc_tile_file

#define MAP_BASE BG_MAP_BASE(8)
#define TILE_BASE BG_TILE_BASE(6)

#define MAX_TILE_SLOTS          512

#define MINIMAP_X               4

#define c2(a,i)		(RGB15((a[i+2]>>3),(a[i+1]>>3),(a[i]>>3)))

#define CACHE_SLOT_TO_TILE_INDEX(c) ((c + 1) * tilemap->tile_width_in_tiles * tilemap->tile_height_in_tiles)

/* The map */

nds_map_t *map;
nds_tilemap_t *tilemap;

/* General rendering variables */

u16 *tile_ram = (u16 *)BG_TILE_RAM(6);
u16 *map_ram = (u16 *)BG_MAP_RAM(8);

rectangle_t map_dimensions = {
  .start = { .x = 0, .y = 0 },
  .dims = { .width = COLNO, .height = ROWNO }
};

int pet_count;

/*
 * Initialize our tile cache.
 */
void nds_init_tile_cache()
{
  int i;

  map->num_cache_entries = (MAX_TILE_SLOTS - 1) / (tilemap->tile_width_in_tiles * tilemap->tile_height_in_tiles);

  map->tile_cache = (tile_cache_entry_t *)malloc(sizeof(tile_cache_entry_t) * map->num_cache_entries);

  for (i = 0; i < map->num_cache_entries; i++) {
    map->tile_cache[i].glyph = -1;
  }
}

/*
 * Returns the cache slot number for the given glyph, if one exists.
 */
int nds_find_cache_slot(coord_t coords)
{
  int glyph = map->glyphs[coords.y][coords.x];
  int i;
  int ch, colour;
  unsigned int special;
  int tile = glyph2tile[glyph];

  mapglyph(glyph, &ch, &colour, &special, coords.x, coords.y, 0);

  for (i = 0; i < map->num_cache_entries; i++) {
    if ((map->tile_cache[i].glyph == glyph) && (map->tile_cache[i].colour == colour) && 
        (map->tile_cache[i].special == special) && (map->tile_cache[i].ch == ch) &&
        (map->tile_cache[i].tile == tile)) {
      return i;
    }
  }

  return -1;
}

/*
 * Allocate a cache slot for the given glyph.  Note, this may evict an entry
 * if the cache is full.
 */
int nds_allocate_cache_slot(coord_t coords)
{ 
  int glyph = map->glyphs[coords.y][coords.x];
  int cache_slot = -1;
  int i;

  int oldest_diff = 0;
  int oldest;

  int ch, colour;
  unsigned int special;

  for (i = 0; i < map->num_cache_entries; i++) {
    int diff;

    if (map->tile_cache[i].glyph < 0) {
      cache_slot = i;

      break;
    } else if (map->tile_cache[i].last_used < 0) {
      continue;
    }

    diff = moves - map->tile_cache[i].last_used;

    if (diff > oldest_diff) {
      oldest = i;
      oldest_diff = diff;
    }
  }

  if (cache_slot < 0) {
    cache_slot = oldest;
  }

  mapglyph(glyph, &ch, &colour, &special, coords.x, coords.y, 0);

  map->tile_cache[cache_slot].glyph = glyph;
  map->tile_cache[cache_slot].colour = colour;
  map->tile_cache[cache_slot].special = special;
  map->tile_cache[cache_slot].ch = ch;
  map->tile_cache[cache_slot].tile = glyph2tile[glyph];

  return cache_slot;
}

void nds_write_tile_row(int subtile_idx, int y, u16 *row)
{
  int bpp = tilemap->bpp;
  u16 *tile_row_start = tile_ram + subtile_idx * bpp * 8 / 2 + y * bpp / 2; 

  /* Again, this works because 8 * (bpp / 8) == bpp */

  memcpy(tile_row_start, row, tilemap->tile_width_in_tiles * tilemap->tile_height_in_tiles * bpp);
}

int nds_load_tile(coord_t coords)
{
  int cache_slot;
  int tile_idx;

  if ((cache_slot = nds_find_cache_slot(coords)) < 0) {
    cache_slot = nds_allocate_cache_slot(coords);
    tile_idx = CACHE_SLOT_TO_TILE_INDEX(cache_slot);

    nds_tilemap_load_tile(tilemap, tile_idx, map->glyphs[coords.y][coords.x], coords);
  } else {
    tile_idx = CACHE_SLOT_TO_TILE_INDEX(cache_slot);
  }

  map->tile_cache[cache_slot].last_used = moves;

  return tile_idx;
}

/*
 * Plot the specified tile to the map.  Note, this represents the tile index
 * as present in the BMP file, *not* the tile RAM index.
 */
void nds_draw_tile(coord_t coords)
{
  int glyph = map->glyphs[coords.y][coords.x];
  int midx, tidx;
  int i, j;
  int palette;

  if (glyph < 0) {
    tidx = 0x0000;
  } else {
    tidx = nds_load_tile(coords);
  }

  /*
   * midx is the starting map index.
   * tidx is the starting tile index.
   *
   * We do a bit of tricky looping here to keep things fast.  midx points to
   * the starting map index.  Each time we complete a row iteration, we move
   * midx ahead by map_width, so it points to the start of the next row.  We
   * then use that to loop through the row copying the tile indices over.
   *
   * Meanwhile, tidx starts off as the initial tile index in RAM.  Because
   * the tiles are ordered left-to-right, top-to-bottom, we can just
   * increment the counter as we populate the map.
   */

  if (TILE_FILE == NULL) {
    switch (iflags.cursor) {
      case 0:
        palette = COORDS_ARE_EQUAL(coords, map->cursor) ? 3 : 2;
        break;

      case 1:
        palette = (((coords.x != u.ux) || (coords.y != u.uy)) && COORDS_ARE_EQUAL(coords, map->cursor)) ? 3 : 2;
        break;

      case 2:
        palette = 2;
        break;

      default:
        break;
    }
  } else {
    palette = 2;
  }

  coord_t screen_tile_coords = coord_subtract(coords, map->viewport.start);

  midx = screen_tile_coords.y * tilemap->tile_height_in_tiles * 32 + screen_tile_coords.x * tilemap->tile_width_in_tiles;

  for (j = 0; j < tilemap->tile_height_in_tiles; j++, midx += 32) {
    for (i = 0; i < tilemap->tile_width_in_tiles; i++) {
      map_ram[midx + i] = tidx | (palette << 12); 

      if (tidx > 0) {
        tidx++;
      }
    }
  }

  /* 
   * Yeah, this is a weird place for this, BUT... here we put in a sprite
   * cursor if this is a pet and we're in tile mode.
   */
  if ((TILE_FILE != NULL) && iflags.hilite_pet) {
    int ch, color;
    unsigned int special;

    mapglyph(glyph, &ch, &color, &special, coords.x, coords.y, 0);
  }
}

/*
 * Get the user sprite set up and drawn.
 */
void nds_clear_sprites()
{
  int i;

  for (i = 0; i < map->sprite_count; i++) {
    map->sprites[i].hidden = true;
  }
}

void nds_draw_sprites()
{
  int i;

  for (i = 0; i < map->sprite_count; i++) {
    sprite_t *sprite = &(map->sprites[i]);

    coord_t screen_tile_coords = coord_subtract(sprite->coords, map->viewport.start);

    oamSet(
      &oamMain,                // Video subsystem to use
      sprite->index,           // The index of the allocate sprite
      screen_tile_coords.x * tilemap->tile_width,        // Our coordinates
      screen_tile_coords.y * tilemap->tile_height,
      2,                       // Sprite priority
      1,                       // Alpha value for sprite
      SpriteSize_64x64,        // Obviously the size
      SpriteColorFormat_Bmp,   // and format
      sprite->gfx,             // Pointer to our graphics memory
      0,                       // Rotation index
      false,                   // Don't double size of rotated sprites
      sprite->hidden,          // Whether or not this sprite is hidden
      false, false,            // Horizontal and vertical flip
      false                    // Mosaic
    );
  }

  oamUpdate(&oamMain);
}

void nds_set_graphics_cursor(coord_t coords)
{
  if (POINT_IN_RECT(coords, map->viewport)) {
    map->sprites[0].coords = coords;
    map->sprites[0].hidden = false;
  } else {
    map->sprites[0].hidden = true;
  }
}

void nds_highlight_tile(coord_t coords)
{
  if (! POINT_IN_RECT(coords, map->viewport)) {
    return;
  } 

  map->sprites[1].coords = coords;
  map->sprites[1].hidden = false;
}

void nds_clear_highlight(int x, int y)
{
  map->sprites[1].hidden = true;
}

void nds_render_cursor(int sprite_num, int r, int g, int b)
{
  map->sprites[sprite_num].gfx = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_Bmp);
  map->sprites[sprite_num].index = map->sprite_count++;

  int x, y;

  for (y = 0; y < tilemap->tile_height; y++) {
    for (x = 0; x < tilemap->tile_width; x++) {
      if ((x != 0) && (x != (tilemap->tile_width - 1)) && 
          (y != 0) && (y != (tilemap->tile_height - 1))) {
        continue;
      }

      if ( ((x == 0) || (x == (tilemap->tile_width - 1))) &&
           ((y > 2) && (y < (tilemap->tile_height - 3))) ) {
        continue;
      }

      if ( ((y == 0) || (y == (tilemap->tile_width - 1))) &&
           ((x > 2) && (x < (tilemap->tile_height - 3))) ) {
        continue;
      }

      map->sprites[sprite_num].gfx[(y * 256) + x] = ARGB16(1, r, g, b);
    }
  }
}

void nds_render_highlighter(int sprite_num, int r, int g, int b)
{
  map->sprites[sprite_num].gfx = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_Bmp);
  map->sprites[sprite_num].index = map->sprite_count++;

  int x, y;

  for (y = 0; y < tilemap->tile_height; y++) {
    for (x = 0; x < tilemap->tile_width; x++) {
      map->sprites[sprite_num].gfx[(y * 256) + x] = ARGB16(1, r, g, b);
    }
  }
}

void nds_init_sprite(int bpp)
{
  /* First, initialize the sprite subsystem */

  oamInit(&oamMain, SpriteMapping_Bmp_2D_256, false);
  
  /* Let's draw our highlight thinger */

  nds_render_cursor(0, 63, 63, 63);
  //nds_render_highlighter(1, 0, 0, 63);
}

/*
 * Here we do two things.  First, we load the BMP into memory.  We throw an
 * error if it isn't an indexed file of some kind (bpp <= 8).  After the file 
 * is loaded, we populate the palette provided.  This involves converting the 
 * 24-bit RGB tuplets to 15-bit NDS palette entries.
 */
int nds_init_map()
{
  u16 *palette;
  int i;

  int bpp;

  map = (nds_map_t *)malloc(sizeof(nds_map_t));
  memset(map, 0, sizeof(nds_map_t));

  /* 
   * Alright, load the tile file or font data, depending on the rendering
   * mode.
   */

  if (TILE_FILE == NULL) {
    tilemap = nds_text_tilemap_new(FONT_FILE_NAME, FONT_PALETTE_NAME);
  } else {
    tilemap = nds_graphics_tilemap_new(TILE_FILE, TILE_WIDTH, TILE_HEIGHT);
  }

  if ((bpp = nds_tilemap_init(tilemap, &nds_write_tile_row)) < 0) {
    return -1;
  }

  /* Initialize the data we need to manage the map and tiles */

  nds_clear_map();
  nds_init_tile_cache();

  map->viewport.dims.width = 32 / tilemap->tile_width_in_tiles; 
  map->viewport.dims.height = 24 / tilemap->tile_height_in_tiles;

  /* Now initialize our graphics layer */

  /*
   * Set up the tile RAM starting at 16k.  At 65k, the bitmap data starts
   * for the menu layer, so we stick the map RAM 2k before that, 
   * which maximizes the amount of tile RAM we have.
   */

  switch (bpp) {
    case 4:
      REG_BG1CNT = BG_32x32 | MAP_BASE | TILE_BASE | BG_COLOR_16 | BG_PRIORITY_3; 
      REG_DISPCNT |= DISPLAY_BG1_ACTIVE;

      palette = (u16 *)BG_PALETTE + MAP_PALETTE_BASE;
      break;

    case 8:
      REG_BG3CNT = BG_RS_32x32 | MAP_BASE | TILE_BASE | BG_PRIORITY_3; 
      REG_DISPCNT |= DISPLAY_BG3_ACTIVE;

      REG_BG3PA = 1 << 8;
      REG_BG3PB = 0;
      REG_BG3PC = 0;
      REG_BG3PD = 1 << 8;

      vramSetBankE(VRAM_E_LCD);
      palette = VRAM_E_EXT_PALETTE[3][2];

      break;

    default:
      DEBUG_PRINT("Sorry, %d bpp tile files aren't supported.\n", bpp);

      return -1;
  }

  /* Alright, time to copy over the palette data. */

  for (i = 0; i < tilemap->palette_length; i++) {
    palette[i] = tilemap->palette[i];
  }

  /* If we're using extended palettes, get the VRAM set up. */

  if (bpp == 8) {
    vramSetBankE(VRAM_E_BG_EXT_PALETTE);
  }

  nds_init_sprite(bpp);

  return 0;
}

nds_map_t *nds_get_map()
{
  return map;
}

void nds_clear_map()
{
  memset(map_ram, 0, 32 * 24 * 2);

  if (map != NULL) {
    int x, y;

    for (y = 0; y < ROWNO; y++) {
      for (x = 0; x < COLNO; x++) {
        map->glyphs[y][x] = -1;
      }
    }
  }
}

rectangle_t nds_minimap_dims()
{
  int minimap_x = MINIMAP_X;
  int minimap_y = system_font->height + 2;
  rectangle_t res;

  res.start.x = minimap_x - 1;
  res.start.y = minimap_y - 1;
  res.dims.width = COLNO * 2 + 1;
  res.dims.height = ROWNO * 2 + 1;

  return res;
}

void nds_draw_minimap()
{
  u16 *sub_vram = (u16 *)BG_BMP_RAM_SUB(4);
  int x, y;
  rectangle_t dims = nds_minimap_dims();

  int rx1, ry1;

  rx1 = dims.start.x;
  ry1 = dims.start.y;

  for (y = 0; y < ROWNO; y++) {
    for (x = 0; x < COLNO; x++) {
      int glyph = map->glyphs[y][x];
      int typ = levl[x][y].typ;
      u16 colour;

      if (glyph < 0) {
        colour = 0x00;
      } else if ((x == u.ux) && (y == u.uy)) {
        colour = C_YOU;
      } else if (glyph_is_normal_monster(glyph)) {
        colour = C_MON; 
      } else if (glyph_is_pet(glyph)) {
        colour = C_PET;
      } else if (IS_WALL(typ)) {
        colour = C_WALL;
      } else if (IS_STWALL(typ)) {
        colour = 0x00;
      } else if (IS_DOOR(typ)) {
        colour = C_DOOR;
      } else if (typ == STAIRS) {
        colour = C_STAIRS;
      } else if (typ == ALTAR) {
        colour = C_ALTAR;
      } else if (typ == CORR) {
        colour = C_CORR;
      } else if (IS_FURNITURE(typ)) {
        colour = C_FURNITURE;
      } else if (IS_ROOM(typ)) {
        colour = C_ROOM;
      } else {
        colour = C_WALL;
      }

      sub_vram[(y + (ry1 + 1) / 2) * 256 + x + (rx1 + 1) / 2] = (colour << 8) | colour;
      sub_vram[(y + (ry1 + 1) / 2) * 256 + x + (rx1 + 1) / 2 + 128] = (colour << 8) | colour;
    }
  }

  nds_draw_rect_outline(dims, C_MAPBORDER, sub_vram);

  int width = map->viewport.dims.width;
  int height = map->viewport.dims.height;

  rectangle_t visible_rect = {
    .start = { .x = map->center.x * 2 - width + rx1, .y = map->center.y * 2 - height + ry1 },
    .dims = { .width = width * 2 + 1, .height = height * 2 + 1 }
  };

  nds_draw_rect_outline(visible_rect, C_VISBORDER, sub_vram);
}

void nds_clear_minimap()
{
  u16 *sub_vram = (u16 *)BG_BMP_RAM_SUB(4);
  int x, y;
  rectangle_t map_rect = nds_minimap_dims();

  for (y = 0; y < ROWNO; y++) {
    for (x = 0; x < COLNO; x++) {
      sub_vram[(y + map_rect.start.y + 1) * 256 + x + map_rect.start.x + 1] = 0x0000;
      sub_vram[(y + map_rect.start.y + 1) * 256 + x + map_rect.start.x + 1 + 128] = 0x0000;
    }
  }

  nds_draw_hline(map_rect.start.x - 1, map_rect.start.y - 1, COLNO * 2 + 3, C_MAPBORDER, sub_vram);
  nds_draw_hline(map_rect.start.x - 1, RECT_END_Y(map_rect) + 1, COLNO * 2 + 3, C_MAPBORDER, sub_vram);

  nds_draw_vline(map_rect.start.x - 1, map_rect.start.y - 1, ROWNO * 2 + 3, C_MAPBORDER, sub_vram);
  nds_draw_vline(RECT_END_X(map_rect) + 2, map_rect.start.y - 1, ROWNO * 2 + 3, C_MAPBORDER, sub_vram);
}

void nds_draw_map(coord_t *center)
{
  swiWaitForVBlank();

  if (center != NULL) {
    nds_map_set_center(*center);
  }

  pet_count = 0;

  nds_clear_sprites();

  if (map != NULL) {
    int x, y;

    for (y = map->viewport.start.y; y < RECT_END_Y(map->viewport); y++) {
      for (x = map->viewport.start.x; x < RECT_END_X(map->viewport); x++) {
        coord_t cur = { .x = x, .y = y };

        nds_draw_tile(cur);
      }
    }

    if (TILE_FILE != NULL) {
      nds_set_graphics_cursor(map->cursor);
    }

    nds_draw_minimap(map);
  } else {
    nds_clear_map();
    nds_clear_minimap();
  }

  nds_draw_sprites();
}

coord_t nds_map_translate_coords(coord_t coords)
{
  coord_t res = {
    .x = map->viewport.start.x + coords.x,
    .y = map->viewport.start.y + coords.y
  };

  return res;
}

int nds_map_tile_width()
{
  return tilemap->tile_width;
}

int nds_map_tile_height()
{
  return tilemap->tile_height;
}

coord_t nds_map_get_center()
{
  return map->center;
}

void nds_map_set_center(coord_t center)
{
  int width = map->viewport.dims.width;
  int height = map->viewport.dims.height;

  if ((center.x + width / 2) > COLNO) {
    map->center.x = COLNO - width / 2;
  } else if ((center.x - width / 2) < 0) {
    map->center.x = width / 2;
  } else {
    map->center.x = center.x;
  }

  if ((center.y + height / 2) > ROWNO) {
    map->center.y = ROWNO - height / 2;
  } else if ((center.y - height / 2) < 0) {
    map->center.y = height / 2;
  } else {
    map->center.y = center.y;
  }

  map->viewport.start.x = map->center.x - width / 2;
  map->viewport.start.y = map->center.y - height / 2;

  map->dirty = 1;
}

void nds_map_set_cursor(coord_t cursor)
{
  map->cursor = cursor;
  map->dirty = 1;
}

coord_t nds_map_relativize(coord_t coords)
{
  int u_center_px = (u.ux - map->viewport.start.x) * tilemap->tile_width + tilemap->tile_width / 2;
  int u_center_py = (u.uy - map->viewport.start.y) * tilemap->tile_height + tilemap->tile_height / 2;

  coord_t res = {
    .x = coords.x - u_center_px,
    .y = coords.y - u_center_py
  };

  return res;
}
