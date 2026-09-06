#ifndef EVENT_H
#define EVENT_H

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

    void event_handler(lv_event_t *e);
    void get_sw_state(lv_timer_t *timer);
    void get_adc33_state(lv_timer_t *timer);
    void get_adc34_state(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_H */