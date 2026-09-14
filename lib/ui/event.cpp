#include "event.h"
#include <Arduino.h>
#include <lvgl.h>
#include "src/misc/lv_ll.h"

#ifdef __cplusplus
extern "C" {
#endif

void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    
}

#ifdef __cplusplus
}
#endif