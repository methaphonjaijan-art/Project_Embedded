//ไฟล์ box.cpp


#include "box.h"
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// --- กำหนดขาพินอุปกรณ์กล่อง ---
#define IR1_PIN     33
#define IR2_PIN     34
#define SERVO1_PIN  13
#define SERVO2_PIN  14

// --- กำหนดขาเซนเซอร์วัดระยะ & ไฟหน้าจอ ---
#define TRIG_PIN        26
#define ECHO_PIN        35
#define BL_PIN          32
#define DETECT_DIST_CM  50

// --- ข้อมูล LINE Messaging API ---
const char* channelToken = "nZYIOssTZpFn+/fZ38wnzEeJuEfPqBKukrG/CTLW7UocQQP5yNLY+JE1PTogHqPpBcC1OnWKMFHXlIe0msUn7h+pzNjxNe6IvePpWHG6aoztrNhYi3Mgs1JUYR/0AYBL64mrijAlMaGMLal8FI1hLAdB04t89/1O/w1cDnyilFU=";
const char* user1_id = "U661b5c343d7ab14120820bf92e9867d3";
const char* user2_id = "Uc97ac7d0267eb92af50daffac3402b83";

// โครงสร้างคิวส่ง LINE
struct LineMsg {
    char to[40];
    char text[128];
};
static QueueHandle_t lineQueue = NULL;

Servo servo1;
Servo servo2;

bool box1_locked = false;
bool box2_locked = false;

static unsigned long ir1_start = 0;
static bool ir1_waiting = false;
static unsigned long ir1_unseen_start = 0;

static unsigned long ir2_start = 0;
static bool ir2_waiting = false;
static unsigned long ir2_unseen_start = 0;

static unsigned long last_detected_time = 0;
static bool is_full_brightness = false;
static unsigned long last_ping_time = 0;

// อ้างอิง gfx จาก main.cpp
extern Arduino_GFX *gfx;

static lv_obj_t *main_screen = NULL;  // ตัวแปรจำหน้าจอหลักของ UI
static lv_obj_t *blank_screen = NULL; // หน้าจอสีดำสำหรับโหมด Sleep

void set_screen_sleep(bool sleep)
{
    // จำหน้าจอ UI หลักไว้ตอนที่ถูกเรียกครั้งแรก
    if (main_screen == NULL) {
        main_screen = lv_scr_act();
    }

    // สร้างหน้าจอดำสนิทเตรียมไว้ 1 หน้าจอ
    if (blank_screen == NULL) {
        blank_screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(blank_screen, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(blank_screen, LV_OPA_COVER, 0);
    }

    if (sleep) {
        // สลับไปแสดงหน้าจอดำสนิททันที (UI หลักยังคงอยู่ใน RAM ไม่หาย)
        lv_scr_load(blank_screen);
        Serial.println("[DISPLAY] Screen Sleeping (Switched to Blank Screen)");
    } else {
        // ปลุกคอนโทรลเลอร์จอ และสลับกลับมาหน้าจอ UI หลัก
        gfx->displayOn();
        if (main_screen != NULL) {
            lv_scr_load(main_screen);
        }
        Serial.println("[DISPLAY] Screen Woken Up (UI Restored)");
    }
}

// Background Task สำหรับส่ง LINE ป้องกันลูปสะดุดและ Handshake ล่ม
void lineTask(void *pvParameters)
{
    LineMsg msg;
    while (1)
    {
        if (xQueueReceive(lineQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[LINE] WiFi not connected. Trying to reconnect...");
                WiFi.reconnect();
                int retry = 0;
                while (WiFi.status() != WL_CONNECTED && retry < 10) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    retry++;
                }
            }

            if (WiFi.status() == WL_CONNECTED) {
                WiFiClientSecure client;
                client.setInsecure();
                client.setTimeout(6000);

                HTTPClient http;
                if (http.begin(client, "https://api.line.me/v2/bot/message/push"))
                {
                    http.addHeader("Content-Type", "application/json; charset=utf-8");
                    http.addHeader("Authorization", String("Bearer ") + channelToken);

                    String payload = "{\"to\":\"" + String(msg.to) + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + String(msg.text) + "\"}]}";
                    int code = http.POST(payload);

                    if (code == 200) {
                        Serial.printf("[LINE OK] Sent to %s\n", msg.to);
                    } else {
                        Serial.printf("[LINE FAIL] HTTP Code: %d\n", code);
                        Serial.printf("[LINE DETAIL] %s\n", http.getString().c_str());
                    }
                    http.end();
                }
            } else {
                Serial.println("[LINE FAIL] WiFi reconnect timeout!");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void sendLine(const char* targetUid, String messageText)
{
    if (lineQueue == NULL) return;
    LineMsg msg;
    strncpy(msg.to, targetUid, sizeof(msg.to) - 1);
    msg.to[sizeof(msg.to) - 1] = '\0';
    strncpy(msg.text, messageText.c_str(), sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    xQueueSend(lineQueue, &msg, pdMS_TO_TICKS(50));
}

bool check_package(int box_number)
{
    if (box_number == 1) return (digitalRead(IR1_PIN) == LOW);
    if (box_number == 2) return (digitalRead(IR2_PIN) == LOW);
    return false;
}

void lock_box(int box_number)
{
    if (box_number == 1)
    {
        servo1.write(0);
        box1_locked = true;
        ir1_waiting = false;
        Serial.println("\n>>> [BOX 1] LOCKED (Servo 0°) <<<");
        sendLine(user1_id, "📦 EM01 : มีพัสดุมาส่งที่กล่องของคุณ กล่องปิดล็อกเรียบร้อย!");
    }
    else if (box_number == 2)
    {
        servo2.write(0);
        box2_locked = true;
        ir2_waiting = false;
        Serial.println("\n>>> [BOX 2] LOCKED (Servo 0°) <<<");
        sendLine(user2_id, "📦 EM02 : มีพัสดุมาส่งที่กล่องของคุณ กล่องปิดล็อกเรียบร้อย!");
    }
}

void unlock_box(int user)
{
    if (user == 1)
    {
        servo1.write(180);
        box1_locked = false;
        ir1_waiting = false;
        Serial.println("[BOX 1] Unlocked by User 1");
    }
    else if (user == 2)
    {
        servo2.write(180);
        box2_locked = false;
        ir2_waiting = false;
        Serial.println("[BOX 2] Unlocked by User 2");
    }
}

void box_init()
{
    pinMode(IR1_PIN, INPUT_PULLUP);
    pinMode(IR2_PIN, INPUT);

    lineQueue = xQueueCreate(5, sizeof(LineMsg));
    xTaskCreatePinnedToCore(lineTask, "LineTask", 8192, NULL, 1, NULL, 0);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);

    servo1.setPeriodHertz(50);
    servo2.setPeriodHertz(50);

    servo1.attach(SERVO1_PIN, 500, 2500);
    servo2.attach(SERVO2_PIN, 500, 2500);

    servo1.write(180);
    servo2.write(180);
    box1_locked = false;
    box2_locked = false;

    Serial.println("[BOX] Dual Box Ready (Both Open)");
}

void check_box()
{
    // จัดการกล่อง 1
    if (!box1_locked)
    {
        if (check_package(1))
        {
            ir1_unseen_start = 0;
            if (!ir1_waiting)
            {
                ir1_waiting = true;
                ir1_start = millis();
                Serial.println("[BOX 1] Parcel detected! Closing in 10s...");
            }
            else if (millis() - ir1_start >= 10000)
            {
                lock_box(1);
            }
        }
        else if (ir1_waiting)
        {
            if (ir1_unseen_start == 0) ir1_unseen_start = millis();
            if (millis() - ir1_unseen_start >= 1000)
            {
                ir1_waiting = false;
                ir1_unseen_start = 0;
                Serial.println("[BOX 1] Parcel removed -> Reset Timer");
            }
        }
    }

    // จัดการกล่อง 2
    if (!box2_locked)
    {
        if (check_package(2))
        {
            ir2_unseen_start = 0;
            if (!ir2_waiting)
            {
                ir2_waiting = true;
                ir2_start = millis();
                Serial.println("[BOX 2] Parcel detected! Closing in 10s...");
            }
            else if (millis() - ir2_start >= 10000)
            {
                lock_box(2);
            }
        }
        else if (ir2_waiting)
        {
            if (ir2_unseen_start == 0) ir2_unseen_start = millis();
            if (millis() - ir2_unseen_start >= 1000)
            {
                ir2_waiting = false;
                ir2_unseen_start = 0;
                Serial.println("[BOX 2] Parcel removed -> Reset Timer");
            }
        }
    }
}

// --- ควบคุมการเริ่มต้นหน้าจอและ Ultrasonic ---
void backlight_sensor_init()
{
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    pinMode(ECHO_PIN, INPUT);

    // ประมวลผลรอบแรกของ LVGL ให้เฟรมแรกเสร็จสมบูรณ์
    lv_timer_handler();

    // ดับสนิทตั้งแต่เปิดเครื่องด้วยม่านดำ
    set_screen_sleep(true);
    is_full_brightness = false;
    last_detected_time = 0;

    Serial.println("[BL] Initialized: Black Standby Screen (Covered)");
}

static long read_ultrasonic_distance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(4);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 25000);
    if (duration == 0) return 999;

    return (long)(duration * 0.034 / 2);
}

void check_proximity()
{   
    // เงื่อนไข 1: กล่องยังเปิดอยู่ทั้งคู่ บังคับม่านดำปิดเสมอ และไม่รัน Ultrasonic
    if (!box1_locked && !box2_locked)
    {
        if (is_full_brightness)
        {
            set_screen_sleep(true);
            is_full_brightness = false;
            Serial.println("[DISPLAY] Both boxes open -> Force Screen Sleep");
        }
        return;
    }

    // เงื่อนไข 2: มีกล่องล็อกแล้วอย่างน้อย 1 กล่อง เริ่มตรวจจับคนเดินเข้าใกล้
    if (millis() - last_ping_time >= 200)
    {
        last_ping_time = millis();
        long distance = read_ultrasonic_distance();

        Serial.printf("[SONAR] Measured: %ld cm\n", distance);

        // อยู่ในระยะ 5 ถึง 50 ซม. ปลดม่านดำแสดงผล UI
        if (distance >= 5 && distance <= DETECT_DIST_CM)
        {
            last_detected_time = millis();
            if (!is_full_brightness)
            {
                set_screen_sleep(false);
                is_full_brightness = true;
                Serial.printf(">>> [PROXIMITY] Person detected at %ld cm -> Screen ON <<<\n", distance);
            }
        }
        else
        {
            // เดินออกห่างเกิน 5 วินาที ดับหน้าจอกลับเป็นสีดำ
            if (is_full_brightness && (millis() - last_detected_time >= 5000))
            {
                set_screen_sleep(true);
                is_full_brightness = false;
                Serial.println("[PROXIMITY] Person left -> Screen OFF (Blackout)");
            }
        }
    }
}