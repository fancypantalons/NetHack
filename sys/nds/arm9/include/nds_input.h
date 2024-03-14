#ifndef _NDS_INPUT_H_
#define _NDS_INPUT_H_

#define CLICK_2_FRAMES 30

typedef enum KEYPAD_EXTRA_BITS {
  KEY_UP_LEFT = BIT(14),
  KEY_UP_RIGHT = BIT(15),
  KEY_DOWN_LEFT = BIT(16),
  KEY_DOWN_RIGHT = BIT(17)
} KEYPAD_EXTRA_BITS;

typedef struct {
  int pressed;
  int released;
  int held;
  int touching;
  int tapped;

  int press_and_hold;
  int dragging;
  int drag_started;
  int drag_stopped;

  coord_t touch_coords;
  coord_t tap_coords;

  coord_t initial_touch_coords;
  coord_t drag_distance;

  int key_held_frames;
  int touch_held_frames;
} nds_input_state_t;

nds_input_state_t nds_poll_input(nds_input_state_t prev_state);

#endif
