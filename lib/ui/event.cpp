#include "event.h"
#include <Arduino.h>
#include <lvgl.h>
#include "src/misc/lv_ll.h"
#include "box.h"

#ifdef __cplusplus
extern "C" {
#endif

int selected_user = 0;
static bool is_processing = false;

const char* uid_user1 = "U661b5c343d7ab14120820bf92e9867d3";
const char* uid_user2 = "Uc97ac7d0267eb92af50daffac3402b83";

static void timer_success_cb(lv_timer_t *timer)
{
    lv_textarea_set_password_mode(objects.text_p, true);
    lv_textarea_set_text(objects.text_p, "");
    lv_scr_load_anim(objects.main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
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

            // สั่งปลดล็อกกล่องของ User นั้นๆ
            unlock_box(selected_user);

            // ส่ง LINE แจ้งเตือน
            if (selected_user == 1) {
                sendLine(uid_user1, "🔓 กล่องเปิดแล้ว: นำพัสดุออกเรียบร้อย (User 1)");
            } else if (selected_user == 2) {
                sendLine(uid_user2, "🔓 กล่องเปิดแล้ว: นำพัสดุออกเรียบร้อย (User 2)");
            }

            lv_timer_t *t = lv_timer_create(timer_success_cb, 1500, NULL);
            lv_timer_set_repeat_count(t, 1);
            selected_user = 0;
        }
        else
        {
            Serial.println("[Auth] WRONG PASSWORD!");
            lv_textarea_set_password_mode(objects.text_p, false);
            lv_textarea_set_text(objects.text_p, "WRONG");

            // แจ้งเตือนเมื่อกรอกรหัสผิด
            if (selected_user == 1) {
                sendLine(uid_user1, "⚠️ มีผู้พยายามใส่รหัสผ่านกล่องรับพัสดุของคุณ (EM01) ไม่ถูกต้อง!");
            } else if (selected_user == 2) {
                sendLine(uid_user2, "⚠️ มีผู้พยายามใส่รหัสผ่านกล่องรับพัสดุของคุณ (EM02) ไม่ถูกต้อง!");
            }

            lv_timer_t *t = lv_timer_create(timer_wrong_cb, 1500, NULL);
            lv_timer_set_repeat_count(t, 1);
        }
    }
}

#ifdef __cplusplus
}
#endif