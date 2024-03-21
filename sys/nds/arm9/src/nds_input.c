#include <nds.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_util.h"
#include "nds_map.h"
#include "nds_input.h"
#include "nds_debug.h"

#define PRESSED(v, c) ((v & (c)) == (c))

coord_t _to_map_coords(coord_t coords)
{
  coord_t res = { 
    .x = coords.x / nds_map_tile_width(), 
    .y = coords.y / nds_map_tile_height()
  };

  return res;
}

/*
 * Here we synthetically create diagonal button press events that we can use
 * elsewhere.
 */
int _nds_remap_key(int key) {
  if (PRESSED(key, KEY_UP | KEY_LEFT)) {
    return KEY_UP_LEFT;
  } else if (PRESSED(key, KEY_UP | KEY_RIGHT)) {
    return KEY_UP_RIGHT;
  } else if (PRESSED(key, KEY_DOWN | KEY_LEFT)) {
    return KEY_DOWN_LEFT;
  } else if (PRESSED(key, KEY_DOWN | KEY_RIGHT)) {
    return KEY_DOWN_RIGHT;
  } else {
    return key;
  }
}

nds_input_state_t nds_poll_input(nds_input_state_t prev_state)
{
  nds_input_state_t state = prev_state;

  swiWaitForVBlank();

  scanKeys();
  scan_touch_screen();

  /*
   * First, we'll process the touchscreen tap/drag events.
   */
  state.held = nds_keysHeld();

  if (state.held & KEY_TOUCH) {
    state.touch_coords = _to_map_coords(get_touch_coords());
    state.touching = 1;
  } else {
    state.touching = 0;
  }

  if (state.touching && ! prev_state.touching) {
    state.initial_touch_coords = state.touch_coords;
  }

  /*
   * Alright, knowing the previous touch state and the current one, now
   * check for dragging.
   */
  if (prev_state.touching && state.touching && 
      ! COORDS_ARE_EQUAL(prev_state.initial_touch_coords, state.touch_coords)) {

    state.drag_distance = coord_subtract(prev_state.initial_touch_coords, state.touch_coords);

    if (! state.dragging) {
      state.dragging = 1;
      state.drag_started = 1;
    } else {
      state.drag_started = 0;
    }
  } else if (state.dragging) {
    state.dragging = 0;
    state.drag_stopped = 1;
  } else if (state.drag_stopped) {
    state.drag_stopped = 0;
  }

  state.held = _nds_remap_key(nds_keysHeld());

  /*
   * To make diagonal presses work semi-reliably, we only register a key
   * press after the same buttons have been pressed for at least three
   * frames.
   */
  if (prev_state.held && state.held && (prev_state.held == state.held)) {
    state.key_held_frames = prev_state.key_held_frames + 1;

    if (state.key_held_frames > 3) {
      state.pressed = _nds_remap_key(prev_state.held & state.held);
    }
  } else {
    state.pressed = 0;
    state.key_held_frames = 0;
  }

  state.released = nds_keysUp();

  /*
   * Tap events...
   */
  state.tapped = get_tap_coords(&(state.tap_coords)) && ! prev_state.press_and_hold && ! state.dragging && ! state.drag_stopped;
  state.tap_coords = _to_map_coords(state.tap_coords);

  /*
   * And press-and-hold...
   */
  if (state.touching && ! state.dragging) {
    state.touch_held_frames = prev_state.touch_held_frames + 1;

    if (state.touch_held_frames > CLICK_2_FRAMES) {
      state.press_and_hold = 1;
    }
  } else {
    state.press_and_hold = 0;
    state.touch_held_frames = 0;
  }

  return state;
}
