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
    int *pLED = (int *)lv_event_get_user_data(e);

    // a. กดปุ่ม valve_bt ติด / ปล่อยดับ
    if (obj == objects.valve_bt)
    {
        if (code == LV_EVENT_PRESSED)
        {
            Serial.println("ON");
            if (pLED) gpio_set_level((gpio_num_t)*pLED, 1);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            Serial.println("OFF");
            if (pLED) gpio_set_level((gpio_num_t)*pLED, 0);
        }
    }
    // b. & c. สวิตช์ sw_pump เปิด/ปิด สัญญาณ PWM ขา 22
    else if (obj == objects.sw_pump && code == LV_EVENT_VALUE_CHANGED)
    {
        if (lv_obj_has_state(objects.sw_pump, LV_STATE_CHECKED))
        {
            char data[32];
            lv_roller_get_selected_str(objects.pwmpercent, data, sizeof(data));
            int data_pwm = atoi(data);

            int pwm_val = (data_pwm * 255) / 100;
            analogWrite(22, pwm_val);
            Serial.printf("PWMVAL: %d\n", pwm_val);
        }
        else
        {
            analogWrite(22, 0);
        }
    }
    // อัปเดต PWM เมื่อหมุนเลื่อน Roller ในขณะที่สวิตช์เปิดอยู่
    else if (obj == objects.pwmpercent && code == LV_EVENT_VALUE_CHANGED)
    {
        if (lv_obj_has_state(objects.sw_pump, LV_STATE_CHECKED))
        {
            char data[32];
            lv_roller_get_selected_str(objects.pwmpercent, data, sizeof(data));
            int data_pwm = atoi(data);

            int pwm_val = (data_pwm * 255) / 100;
            analogWrite(22, pwm_val);
            Serial.printf("PWMVAL: %d\n", pwm_val);
        }
    }
}

// d. อ่านค่า Pushbutton Pin 14 ทุก 0.3 วินาที (300 ms)
void get_sw_state(lv_timer_t *timer)
{
    int *pSW = (int *)timer->user_data;
    int sw_val = digitalRead(*pSW);

    if (sw_val == LOW)
    {
        lv_led_set_color(objects.motor_st, lv_palette_main(LV_PALETTE_GREEN));
        lv_led_on(objects.motor_st);
    }
    else
    {
        lv_led_off(objects.motor_st);
    }
}

// e. อ่านค่า ADC Pin 33 ทุก 0.1 วินาที (100 ms)
void get_adc33_state(lv_timer_t *timer)
{
    int raw = analogRead(33);
    int pct = map(raw, 0, 4095, 0, 100);

    lv_bar_set_value(objects.adc33bar, pct, LV_ANIM_OFF);
    lv_label_set_text_fmt(objects.adc33val, "%d", raw);
}

// ดึง pointer เข็มชี้จาก screens.c
extern "C" lv_meter_indicator_t *meter_needle;

// f. อ่านค่า ADC Pin 34 ทุก 0.2 วินาที (200 ms)
void get_adc34_state(lv_timer_t *timer)
{
    int raw = analogRead(34);
    int pct = map(raw, 0, 4095, 0, 100);

    // สั่งอัปเดตเข็มชี้โดยตรง
    if (meter_needle != NULL)
    {
        lv_meter_set_indicator_value(objects.adc34meter, meter_needle, pct);
    }

    // อัปเดตตัวเลข label
    lv_label_set_text_fmt(objects.adc34val, "%d", pct);
}

#ifdef __cplusplus
}
#endif