
//โค้ด main.cpp

#include "ui.h"
#include "screens.h"
#include "structs.h"
#include <Arduino_GFX_Library.h>
#include "event.h"
#include "box.h"
#include <WiFi.h>

const char* ssid     = "Nnn";
const char* password = "12102548";

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else
Arduino_DataBus *bus = new Arduino_ESP32SPI(2, 15, 18, 23);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, 4, 3);
#define CANVAS
#endif

#include "touch.hpp"

static uint32_t screenWidth;
static uint32_t screenHeight;
static uint32_t bufSize;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

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
#endif
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    if (touch_has_signal()) {
        if (touch_touched()) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        } else if (touch_released()) {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("\n--- Starting Smart Box System ---");

    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(BLACK);

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());
    lv_init();

    screenWidth = gfx->width();
    screenHeight = gfx->height();
    bufSize = screenWidth * 40;

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * bufSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * bufSize, MALLOC_CAP_8BIT);
    }

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();

    lv_obj_add_event_cb(objects.user1, user_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(objects.user2, user_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(objects.back, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(objects.keyboard, password_check_event_handler, LV_EVENT_READY, NULL);

    // 1. เริ่มต้นระบบกล่องพัสดุและเซอร์โว
    box_init();

    // 2. เชื่อมต่อ Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 15) {
        delay(400);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected successfully!");
        Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Connection timeout. Continuing in background.");
    }

    // 3. เริ่มต้นม่านดำและเซนเซอร์วัดระยะ (เรียกท้ายสุดหลัง UI โหลดเสร็จ)
    backlight_sensor_init();
}

extern bool is_full_brightness; // ดึงสถานะจอมาจาก box.cpp

void loop()
{
    lv_timer_handler();

    check_box();
    check_proximity();

#ifdef CANVAS
    gfx->flush();
#endif

    delay(5);
}