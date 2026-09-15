#include "event.h"
#include <Arduino.h>
#include <lvgl.h>
#include "src/misc/lv_ll.h"
#include "box.h"

extern void set_screen_sleep(bool sleep);

int selected_user = 0;
static bool is_processing = false;

const char* uid_user1 = "U661b5c343d7ab14120820bf92e9867d3";
const char* uid_user2 = "Uc97ac7d0267eb92af50daffac3402b83";

static void timer_success_cb(lv_timer_t *timer)
{
    lv_textarea_set_password_mode(objects.text_p, true);
    lv_textarea_set_text(objects.text_p, "");
    
    // โหลดหน้าหลักเตรียมไว้เบื้องหลัง
    lv_scr_load(objects.main);
    
    // บังคับดับจอทันทีหลังจากรีเซ็ตหน้าจอเสร็จ
    set_screen_sleep(true);
    
    is_processing = false;
}

static void timer_wrong_cb(lv_timer_t *timer)
{
    lv_textarea_set_password_mode(objects.text_p, true);
    lv_textarea_set_text(objects.text_p, "");
    is_processing = false;
}

void user_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        lv_obj_t *btn = lv_event_get_target(e);
        if (btn == objects.user1) selected_user = 1;
        else if (btn == objects.user2) selected_user = 2;

        is_processing = false;
        lv_keyboard_set_mode(objects.keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_keyboard_set_textarea(objects.keyboard, objects.text_p);
        lv_textarea_set_password_mode(objects.text_p, true);
        lv_textarea_set_text(objects.text_p, "");
        lv_scr_load_anim(objects.password, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}

void back_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        selected_user = 0;
        is_processing = false;
        lv_textarea_set_password_mode(objects.text_p, true);
        lv_textarea_set_text(objects.text_p, "");
        lv_scr_load_anim(objects.main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    }
}

void password_check_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY)
    {
        if (is_processing) return;
        is_processing = true;

        const char *entered_pass = lv_textarea_get_text(objects.text_p);
        bool access_granted = false;

        // User 1 = 1234, User 2 = 2234
        if (selected_user == 1 && strcmp(entered_pass, "1234") == 0) access_granted = true;
        else if (selected_user == 2 && strcmp(entered_pass, "2234") == 0) access_granted = true;

        if (access_granted)
        {
            Serial.println("[Auth] Password Correct -> PASS");
            lv_textarea_set_password_mode(objects.text_p, false);
            lv_textarea_set_text(objects.text_p, "PASS");

            // ปลดล็อกกล่อง
            unlock_box(selected_user);

            // ส่ง LINE แจ้งเตือน
            if (selected_user == 1) {
                sendLine(uid_user1, "🔓 EM01: กล่องเปิดแล้ว! นำพัสดุออกเรียบร้อย");
            } else if (selected_user == 2) {
                sendLine(uid_user2, "🔓 EM02: กล่องเปิดแล้ว! นำพัสดุออกเรียบร้อย");
            }

            // แสดงคำว่า PASS 1.5 วินาที แล้ว timer_success_cb จะพาไปหน้าหลักและดับจอดำสนิท
            lv_timer_t *t = lv_timer_create(timer_success_cb, 1500, NULL);
            lv_timer_set_repeat_count(t, 1);
            selected_user = 0;
        }
        else
        {
            Serial.println("[Auth] WRONG PASSWORD!");
            lv_textarea_set_password_mode(objects.text_p, false);
            lv_textarea_set_text(objects.text_p, "WRONG");

            if (selected_user == 1) {
                sendLine(uid_user1, "⚠️ EM01: มีผู้พยายามใส่รหัสผ่านกล่องรับพัสดุของคุณ!");
            } else if (selected_user == 2) {
                sendLine(uid_user2, "⚠️ EM01: มีผู้พยายามใส่รหัสผ่านกล่องรับพัสดุของคุณ!");
            }

            lv_timer_t *t = lv_timer_create(timer_wrong_cb, 1500, NULL);
            lv_timer_set_repeat_count(t, 1);
        }
    }
}