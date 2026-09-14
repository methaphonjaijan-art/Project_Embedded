#include "box.h"
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
#define BL_PWM_CHANNEL  4
#define BL_FREQ         5000
#define BL_RESOLUTION   8
#define DETECT_DIST_CM  50

// --- LINE Messaging API ---
const char* channelToken = "nZYIOssTZpFn+/fZ38wnzEeJuEfPqBKukrG/CTLW7UocQQP5yNLY+JE1PTogHqPpBcC1OnWKMFHXlIe0msUn7h+pzNjxNe6IvePpWHG6aoztrNhYi3Mgs1JUYR/0AYBL64mrijAlMaGMLal8FI1hLAdB04t89/1O/w1cDnyilFU=";
const char* user1_id = "U661b5c343d7ab14120820bf92e9867d3";
const char* user2_id = "Uc97ac7d0267eb92af50daffac3402b83";

// --- โครงสร้างและ Queue สำหรับส่ง LINE เบื้องหลัง ---
struct LineMessage {
    char targetUid[40];
    char messageText[128];
};
static QueueHandle_t lineQueue = NULL;

Servo servo1;
Servo servo2;

bool box1_locked = false;
bool box2_locked = false;

static unsigned long ir1_start = 0;
static bool ir1_waiting = false;

static unsigned long ir2_start = 0;
static bool ir2_waiting = false;

static unsigned long last_detected_time = 0;
static bool is_dimmed = true; // เริ่มต้นโหมดประหยัดพลังงาน
static unsigned long last_ping_time = 0;

// Task ส่ง LINE เบื้องหลังบน Core 0 ไม่รบกวนหน้าจอ/เซอร์โว
void lineTask(void *pvParameters)
{
    LineMessage msg;
    while (1)
    {
        if (xQueueReceive(lineQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[LINE] Wi-Fi not connected");
                continue;
            }

            WiFiClientSecure client;
            client.setInsecure();
            client.setTimeout(5000);

            HTTPClient http;
            if (http.begin(client, "https://api.line.me/v2/bot/message/push"))
            {
                http.addHeader("Content-Type", "application/json; charset=utf-8");
                http.addHeader("Authorization", String("Bearer ") + channelToken);

                String payload = "{\"to\":\"" + String(msg.targetUid) + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + String(msg.messageText) + "\"}]}";

                int responseCode = http.POST(payload);
                if (responseCode == 200) {
                    Serial.printf("[LINE SUCCESS] Sent to %s\n", msg.targetUid);
                } else {
                    Serial.printf("[LINE ERROR] Code: %d\n", responseCode);
                }
                http.end();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void sendLine(const char* targetUid, String messageText)
{
    if (lineQueue == NULL) return;

    LineMessage msg;
    strncpy(msg.targetUid, targetUid, sizeof(msg.targetUid) - 1);
    msg.targetUid[sizeof(msg.targetUid) - 1] = '\0';

    strncpy(msg.messageText, messageText.c_str(), sizeof(msg.messageText) - 1);
    msg.messageText[sizeof(msg.messageText) - 1] = '\0';

    if (xQueueSend(lineQueue, &msg, pdMS_TO_TICKS(10)) != pdTRUE) {
        Serial.println("[LINE] Queue is full! Dropping message.");
    }
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
        Serial.println("[BOX 1] Locked (Servo 1 -> 90°)");
        sendLine(user1_id, "📦EM01 : มีพัสดุมาส่งที่กล่องของคุณ กล่องปิดล็อกเรียบร้อย!");
    }
    else if (box_number == 2)
    {
        servo2.write(0);
        box2_locked = true;
        ir2_waiting = false;
        Serial.println("[BOX 2] Locked (Servo 2 -> 90°)");
        sendLine(user2_id, "📦EM02 : มีพัสดุมาส่งที่กล่องของคุณ กล่องปิดล็อกเรียบร้อย!");
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

    // สร้าง Queue และ Background Task สำหรับส่ง LINE
    lineQueue = xQueueCreate(5, sizeof(LineMessage));
    xTaskCreatePinnedToCore(lineTask, "LineTask", 6144, NULL, 1, NULL, 0);

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

    Serial.println("[BOX] Dual Box System Initialized");
}

void check_box()
{
    // เช็คกล่องที่ 1 (รอ 10 วินาทีตามโค้ดใหม่ของคุณ)
    if (!box1_locked)
    {
        if (check_package(1))
        {
            if (!ir1_waiting)
            {
                ir1_waiting = true;
                ir1_start = millis();
                Serial.println("[BOX 1] Parcel detected! Waiting 10s...");
            }
            else if (millis() - ir1_start >= 10000)
            {
                lock_box(1);
            }
        }
        else
        {
            if (ir1_waiting) ir1_waiting = false;
        }
    }

    // เช็คกล่องที่ 2 (รอ 10 วินาที)
    if (!box2_locked)
    {
        if (check_package(2))
        {
            if (!ir2_waiting)
            {
                ir2_waiting = true;
                ir2_start = millis();
                Serial.println("[BOX 2] Parcel detected! Waiting 10s...");
            }
            else if (millis() - ir2_start >= 10000)
            {
                lock_box(2);
            }
        }
        else
        {
            if (ir2_waiting) ir2_waiting = false;
        }
    }
}

// --- ฟังก์ชัน Backlight และเซนเซอร์วัดระยะ ---
void set_backlight_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (255 * percent) / 100;
    ledcWrite(BL_PWM_CHANNEL, duty);
}

void backlight_sensor_init()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    ledcSetup(BL_PWM_CHANNEL, BL_FREQ, BL_RESOLUTION);
    ledcAttachPin(BL_PIN, BL_PWM_CHANNEL);
    
    set_backlight_percent(20); // เปิดเครื่องมาให้หรี่ไฟเหลือ 20% ทันที
    is_dimmed = true;
    last_detected_time = millis();
}

static long read_ultrasonic_distance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 10000);
    if (duration == 0) return 999;

    return duration * 0.034 / 2;
}

void check_proximity()
{   
    // จุดที่แก้ไข: เติมปีกกาปิดสมบูรณ์
    if (!box1_locked && !box2_locked)
    {
        if (!is_dimmed)
        {
            set_backlight_percent(20);
            is_dimmed = true;
            Serial.println("[BL] Both boxes unlocked -> Screen standby (20%)");
        }
        return; // ข้ามการวัด Ultrasonic ถ้ายังไม่มีกล่องไหนล็อก
    }

    // มีกล่องล็อกแล้วอย่างน้อย 1 กล่อง -> เริ่มวัดระยะคนเข้าใกล้
    if (millis() - last_ping_time >= 200)
    {
        last_ping_time = millis();
        long distance = read_ultrasonic_distance();

        // เมื่อคนอยู่ในระยะ 50 ซม. -> สว่าง 100%
        if (distance > 0 && distance <= DETECT_DIST_CM)
        {
            last_detected_time = millis();
            if (is_dimmed)
            {
                set_backlight_percent(100);
                is_dimmed = false;
                Serial.println("[BL] Person approached -> Screen 100%");
            }
        }
        else
        {
            // ออกนอกระยะเกิน 5 วินาที -> หรี่เหลือ 20%
            if (!is_dimmed && (millis() - last_detected_time >= 5000))
            {
                set_backlight_percent(20);
                is_dimmed = true;
                Serial.println("[BL] Nobody detected -> Screen Dimmed (20%)");
            }
        }
    }
}