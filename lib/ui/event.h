#ifndef EVENT_H
#define EVENT_H

#pragma once
#include <lvgl.h>
#include "ui.h"

#if defined(EEZ_FOR_LVGL)
#include <eez/flow/lvgl_api.h>
#endif

#if !defined(EEZ_FOR_LVGL)
#include "screens.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern int selected_user;
extern volatile int pending_line_user;
extern volatile bool pending_line_is_pass;

void user_btn_event_handler(lv_event_t *e);
void back_btn_event_handler(lv_event_t *e);
void password_check_event_handler(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_H */




