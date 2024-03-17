#include "nds_text_tilemap.h"
#include "nds_debug.h"
#include "ppm-lite.h"
#include "font-bdf.h"

extern short glyph2tile[];

#define ROUND_UP(val) ( (((val) & 0x07) == 0) ? (val) : (((val / 8) + 1) * 8) )

void _nds_tilemap_load_text_tile(nds_tilemap_t *tilemap, int glyph, coord_t coords) 
{
  nds_text_tilemap_t *tmap = (nds_text_tilemap_t *)tilemap;

  int ch, color;
  unsigned int special;
  char tmp[BUFSZ];
  int tile_x, y, i;
  unsigned char *img_data;
  int black = MAP_COLOUR(CLR_BLACK);

  /* Alright, now convert the glyph to a character */

  mapglyph(glyph, &ch, &color, &special, coords.x, coords.y, 0);

  /* Mmm... hacky... */

  if (color == CLR_BLACK) {
    color = CLR_BLUE;
  }

  if (((special & MG_PET) && iflags.hilite_pet) ||
      ((special & MG_DETECT) && iflags.use_inverse)) {
    sprintf(tmp, "\e[7m%c", ch);
  } else {
    sprintf(tmp, "%c", ch);
  }

  /* Now draw the character to a PPM image... yes, this is inefficient :) */

  clear_ppm(tmap->text_img, black);

  draw_string(tmap->font, tmp, tmap->text_img, 
              tmap->text_img->width / 2 - tmap->char_w / 2, 
              tmap->text_img->height / 2 - tmap->char_h / 2, 
              -1, -1);

  img_data = (unsigned char *)tmap->text_img->bitmap;

  /* Now copy the contents of the PPM to tile RAM */

  for (y = 0; y < tmap->text_img->height; y++) {
    for (tile_x = 0; tile_x < tilemap->tile_width_in_tiles; tile_x++) {
      int tile_y = y / 8;
      int tile_row = y % 8;

      u16 *row_ptr = tilemap->tile_buffer + 
                     ((tile_y * tilemap->tile_width_in_tiles + tile_x) * 64 +
                      tile_row * 8) / 2;

      for (i = 0; i < 4; i++, img_data += 2) {
        u8 c0 = ((img_data[0] != black) ? color : 0) + 1;
        u8 c1 = ((img_data[1] != black) ? color : 0) + 1;

        row_ptr[i] = (c1 << 8) | c0;
      }
    }
  }
}

int _nds_nds_tilemap_text_init(nds_tilemap_t *tilemap) 
{
  nds_text_tilemap_t *tmap = (nds_text_tilemap_t *)tilemap;

  int img_w, img_h;
  int i;
  int palcnt;

  if ((tmap->font = read_bdf(tmap->font_fname)) == NULL) {
    DEBUG_PRINT("Unable to open '%s'\n", tmap->font_fname);

    return -1;
  }

  if ((palcnt = nds_load_palette(tmap->palette_fname, tilemap->palette + 1)) < 0) {
    DEBUG_PRINT("Unable to open '%s'\n", tmap->palette_fname);

    return -1;
  }

  /* Generate our inverse palette */

  for (i = 1; i < palcnt + 1; i++) {
    tilemap->palette[i + 256] = tilemap->palette[i] ^ 0x7FFF;
  }

  tilemap->palette_length = 512;

  /* Now figure out our dimensions */

  text_dims(tmap->font, "#", &(tmap->char_w), &(tmap->char_h));

  img_w = ROUND_UP(tmap->char_w);
  img_h = ROUND_UP(tmap->char_h);

  tmap->text_img = alloc_ppm(img_w, img_h);

  tilemap->tile_width = img_w;
  tilemap->tile_height = img_h;

  tilemap->bpp = 8;

  tilemap->loader = &_nds_tilemap_load_text_tile;

  return tilemap->bpp;
}

nds_tilemap_t *nds_text_tilemap_new(char *font_fname, char *palette_fname) 
{
  nds_text_tilemap_t *tmap = (nds_text_tilemap_t *)malloc(sizeof(nds_text_tilemap_t));

  tmap->font_fname = font_fname;
  tmap->palette_fname = palette_fname;

  tmap->base.init = &_nds_nds_tilemap_text_init;

  return (nds_tilemap_t *)tmap;
}
