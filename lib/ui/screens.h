#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_PASSWORD = 2,
    _SCREEN_ID_LAST = 2
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *password;
    lv_obj_t *user1;
    lv_obj_t *user2;
    lv_obj_t *text_p;
    lv_obj_t *keyboard;
    lv_obj_t *back;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_password();
void tick_screen_password();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/