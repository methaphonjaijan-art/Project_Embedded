#include "ui.h"
#include <Arduino_GFX_Library.h>
#include "event.h"

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

#define GFX_BL 32
Arduino_DataBus *bus = new Arduino_ESP32SPI(2, 15, 18, 23);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 4, 3);
#define CANVAS

#endif /* !defined(DISPLAY_DEV_KIT) */
#include "touch.hpp"

static uint32_t screenWidth;
static uint32_t screenHeight;
static uint32_t bufSize;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// ขา I/O สำหรับโจทย์ข้อ 5
int LED1 = 26;
int *pLED1 = &LED1;
int LED2 = 22;

int SW1 = 14;
int *pSW1 = &SW1;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
#ifndef DIRECT_MODE
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
#endif // #ifndef DIRECT_MODE

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    if (touch_has_signal())
    {
        if (touch_touched())
        {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        }
        else if (touch_released())
        {
            data->state = LV_INDEV_STATE_REL;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup()
{
    Serial.begin(115200);

    // กำหนดโหมดขาต่างๆ
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(SW1, INPUT_PULLUP);
    pinMode(33, INPUT);
    pinMode(34, INPUT);
    analogWriteResolution(8);

    Serial.println("Arduino_GFX LVGL Widgets example");

#ifdef GFX_EXTRA_PRE_INIT
    GFX_EXTRA_PRE_INIT();
#endif

    if (!gfx->begin())
    {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());

    lv_init();

    screenWidth = gfx->width();
    screenHeight = gfx->height();

#ifdef DIRECT_MODE
    bufSize = screenWidth * screenHeight;
#else
    bufSize = screenWidth * 40;
#endif

#ifdef ESP32
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * bufSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf)
    {
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * bufSize, MALLOC_CAP_8BIT);
    }
#else
    disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * bufSize);
#endif
    if (!disp_draw_buf)
    {
        Serial.println("LVGL disp_draw_buf allocate failed!");
    }
    else
    {
        lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);

        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = screenWidth;
        disp_drv.ver_res = screenHeight;
        disp_drv.flush_cb = my_disp_flush;
        disp_drv.draw_buf = &draw_buf;
#ifdef DIRECT_MODE
        disp_drv.direct_mode = true;
#endif
        lv_disp_drv_register(&disp_drv);

        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = my_touchpad_read;
        lv_indev_drv_register(&indev_drv);

        ui_init();

        // ข้อ a: ควบคุม valve_bt
        lv_obj_add_event_cb(objects.valve_bt, event_handler, LV_EVENT_ALL, pLED1);

        // ข้อ b, c: สวิตช์ปั๊ม และ Roller
        lv_obj_add_event_cb(objects.sw_pump, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(objects.pwmpercent, event_handler, LV_EVENT_VALUE_CHANGED, NULL);

        // ข้อ d: Timer เช็คสวิตช์ปุ่มกด Pin 14 ทุก 300 ms (0.3 วินาที)
        lv_timer_create(get_sw_state, 300, pSW1);

        // ข้อ e: Timer อ่านค่า ADC Pin 33 ทุก 100 ms (0.1 วินาที)
        lv_timer_create(get_adc33_state, 100, NULL);

        // ข้อ f: Timer อ่านค่า ADC Pin 34 ทุก 200 ms (0.2 วินาที)
        lv_timer_create(get_adc34_state, 200, NULL);
    }
}

void loop()
{
    lv_timer_handler();

#ifdef DIRECT_MODE
#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#else
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#endif
#endif

#ifdef CANVAS
    gfx->flush();
#endif

    delay(5);
}