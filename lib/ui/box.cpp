#include "box.h"
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define INVERT_BACKLIGHT 0

// กำหนดระดับความสว่าง
#define BRIGHTNESS_DIM_PERCENT   40   // ระดับหรี่แสง (สว่างพอเห็น ไม่ดับสนิท)
#define BRIGHTNESS_FULL_PERCENT  100  // ระดับสว่างเต็มที่

// --- กำหนดขาพินอุปกรณ์กล่อง ---
#define IR1_PIN     33
#define IR2_PIN     34
#define SERVO1_PIN  13
#define SERVO2_PIN  14

// --- กำหนดขาเซนเซอร์วัดระยะ & ไฟหน้าจอ ---
#define TRIG_PIN        26
#define ECHO_PIN        35
#define BL_PIN          32
#define BL_PWM_CHANNEL  6
#define BL_FREQ         5000
#define BL_RESOLUTION   8
#define DETECT_DIST_CM  50


// --- LINE Messaging API ---
const char* channelToken = "nZYIOssTZpFn+/fZ38wnzEeJuEfPqBKukrG/CTLW7UocQQP5yNLY+JE1PTogHqPpBcC1OnWKMFHXlIe0msUn7h+pzNjxNe6IvePpWHG6aoztrNhYi3Mgs1JUYR/0AYBL64mrijAlMaGMLal8FI1hLAdB04t89/1O/w1cDnyilFU=";
const char* user1_id = "U661b5c343d7ab14120820bf92e9867d3";
const char* user2_id = "Uc97ac7d0267eb92af50daffac3402b83";

Servo servo1;
Servo servo2;

bool box1_locked = false;
bool box2_locked = false;

static unsigned long ir1_start = 0;
static bool ir1_waiting = false;

static unsigned long ir2_start = 0;
static bool ir2_waiting = false;

static unsigned long last_detected_time = 0;
static bool is_dimmed = false;
static unsigned long last_ping_time = 0;

// ฟังก์ชันส่งข้อความ LINE แบบทำงานตรง (Direct Sync Push)
void sendLineDirect(const char* targetUid, String messageText)
{
    Serial.printf("[LINE] Preparing to send to: %s\n", targetUid);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LINE] Wi-Fi lost! Attempting to reconnect...");
        WiFi.reconnect();
        unsigned long start_reconnect = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start_reconnect < 4000)) {
            delay(100);
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LINE FAILED] Wi-Fi is NOT connected. Skipping LINE notification.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // ข้ามการตรวจสอบใบรับรอง SSL
    client.setTimeout(8000);

    HTTPClient http;
    if (http.begin(client, "https://api.line.me/v2/bot/message/push"))
    {
        http.addHeader("Content-Type", "application/json; charset=utf-8");
        http.addHeader("Authorization", String("Bearer ") + channelToken);

        String payload = "{\"to\":\"" + String(targetUid) + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + messageText + "\"}]}";

        Serial.println("[LINE] Sending HTTP POST request...");
        int responseCode = http.POST(payload);

        if (responseCode == 200) {
            Serial.printf("[LINE SUCCESS] Message successfully delivered to %s\n", targetUid);
        } else {
            Serial.printf("[LINE ERROR] HTTP Response Code: %d\n", responseCode);
            Serial.printf("[LINE RESPONSE] %s\n", http.getString().c_str());
        }
        http.end();
    } else {
        Serial.println("[LINE ERROR] Unable to connect to Line API host.");
    }
}

// ฟังก์ชันรองรับการเรียกภายนอก
void sendLine(const char* targetUid, String messageText)
{
    sendLineDirect(targetUid, messageText);
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
        Serial.println("[BOX 1] Servo locked (0°)");
        
        // ส่ง LINE แจ้งเตือน User 1 ทันทีเมื่อปิดประตูกล่อง
        sendLineDirect(user1_id, "📦 EM01 : มีพัสดุมาส่งที่กล่องของคุณ กล่องปิดล็อกเรียบร้อย!");
    }
    else if (box_number == 2)
    {
        servo2.write(0);
        box2_locked = true;
        ir2_waiting = false;
        Serial.println("[BOX 2] Servo locked (0°)");
        
        // ส่ง LINE แจ้งเตือน User 2 ทันทีเมื่อปิดประตูกล่อง
        sendLineDirect(user2_id, "📦 EM02 : มีพัสดุมาส่งที่กล่องของคุณ กล่องปิดล็อกเรียบร้อย!");
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

    Serial.println("[BOX] Dual Box System Initialized (Unlocked & Standby)");
}

void check_box()
{
    // ตรวจสอบกล่องที่ 1
    if (!box1_locked)
    {
        if (check_package(1))
        {
            if (!ir1_waiting)
            {
                ir1_waiting = true;
                ir1_start = millis();
                Serial.println("[BOX 1] Parcel detected! Locking in 10s...");
            }
            else if (millis() - ir1_start >= 10000)
            {
                lock_box(1);
            }
        }
        else
        {
            if (ir1_waiting) {
                ir1_waiting = false;
                Serial.println("[BOX 1] Parcel removed. Timer reset.");
            }
        }
    }

    // ตรวจสอบกล่องที่ 2
    if (!box2_locked)
    {
        if (check_package(2))
        {
            if (!ir2_waiting)
            {
                ir2_waiting = true;
                ir2_start = millis();
                Serial.println("[BOX 2] Parcel detected! Locking in 10s...");
            }
            else if (millis() - ir2_start >= 10000)
            {
                lock_box(2);
            }
        }
        else
        {
            if (ir2_waiting) {
                ir2_waiting = false;
                Serial.println("[BOX 2] Parcel removed. Timer reset.");
            }
        }
    }
}

// --- ฟังก์ชัน Backlight และเซนเซอร์วัดระยะ ---
void set_backlight_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    
#if INVERT_BACKLIGHT
    uint32_t duty = (255 * (100 - percent)) / 100;
#else
    uint32_t duty = (255 * percent) / 100;
#endif

    ledcWrite(BL_PWM_CHANNEL, duty);
}

void backlight_sensor_init()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    pinMode(BL_PIN, OUTPUT);
    ledcSetup(BL_PWM_CHANNEL, BL_FREQ, BL_RESOLUTION);
    ledcAttachPin(BL_PIN, BL_PWM_CHANNEL);
    
    // บังคับหรี่แสงทันทีที่เริ่มทำงาน
    set_backlight_percent(BRIGHTNESS_DIM_PERCENT);
    is_dimmed = true;
    last_detected_time = millis();
    Serial.println("[BL] Initialized: Screen Dimmed (Standby)");
}

static long read_ultrasonic_distance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 25000);
    if (duration == 0) return 999;

    return duration * 0.034 / 2;
}

void check_proximity()
{   
    // เงื่อนไขเด็ดขาด: ถ้ากล่องเปิดอยู่ทั้งสองกล่อง จอต้องหรี่เท่านั้น
    if (!box1_locked && !box2_locked)
    {
        if (!is_dimmed)
        {
            set_backlight_percent(BRIGHTNESS_DIM_PERCENT);
            is_dimmed = true;
            Serial.println("[BL] Both boxes open -> Force Dimmed");
        }
        return; // ตัดทิ้งทันที ไม่ให้อ่าน Ultrasonic เด็ดขาด
    }

    // มีกล่องล็อกแล้วอย่างน้อย 1 กล่อง -> จึงเริ่มวัดระยะคนเพื่อเร่งไฟ
    if (millis() - last_ping_time >= 200)
    {
        last_ping_time = millis();
        long distance = read_ultrasonic_distance();

        // มีคนเดินเข้ามาใกล้กล่องที่ล็อกอยู่ (ระยะ <= 50 ซม.) -> สว่าง 100%
        if (distance > 0 && distance <= DETECT_DIST_CM)
        {
            last_detected_time = millis();
            if (is_dimmed)
            {
                set_backlight_percent(BRIGHTNESS_FULL_PERCENT);
                is_dimmed = false;
                Serial.printf("[BL] Box Locked & Person detected (%ld cm) -> Screen 100%%\n", distance);
            }
        }
        else
        {
            // พ้นระยะเกิน 5 วินาที -> หรี่กลับไปที่ระดับเดิม
            if (!is_dimmed && (millis() - last_detected_time >= 5000))
            {
                set_backlight_percent(BRIGHTNESS_DIM_PERCENT);
                is_dimmed = true;
                Serial.println("[BL] Out of range -> Screen Dimmed");
            }
        }
    }
}