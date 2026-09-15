#ifndef BOX_H
#define BOX_H

#include <Arduino.h>

void box_init();
void check_box();
void lock_box(int box_number);
void unlock_box(int user = 0);
bool check_package(int box_number);
void sendLine(const char* targetUid, String messageText);

// ควบคุม Backlight และวัดระยะ
void backlight_sensor_init();
void check_proximity();
void set_backlight(uint8_t brightness);
void set_backlight_percent(uint8_t percent);

extern bool is_full_brightness;

#endif