extern __INT16_TYPE__ touch_raw_x;
extern __INT16_TYPE__ touch_raw_y;
extern __INT16_TYPE__ touch_last_x;
extern __INT16_TYPE__ touch_last_y;
extern bool touch_swap_xy;
extern __INT16_TYPE__ touch_map_x1;
extern __INT16_TYPE__ touch_map_x2;
extern __INT16_TYPE__ touch_map_y1;
extern __INT16_TYPE__ touch_map_y2;
extern __INT16_TYPE__ touch_max_x;
extern __INT16_TYPE__ touch_max_y;
void touch_init(
__INT16_TYPE__ w,
__INT16_TYPE__ h,
__UINT8_TYPE__ r);
bool touch_has_signal();
void translate_touch_raw();
bool touch_touched();
bool touch_released();