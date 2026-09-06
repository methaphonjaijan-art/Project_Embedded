#include "touch.hpp"
#include <Arduino_GFX_Library.h>

#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

/* More dev device declaration: https://github.com/moononournation/Arduino_GFX/wiki/Dev-Device-Declaration */
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
#define GFX_BL 32
Arduino_DataBus *bus = new Arduino_ESP32SPI(2,15,18,23);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 4,2);

#endif /* !defined(DISPLAY_DEV_KIT) */

int16_t w = -1, h = -1;
int16_t point_x[4] = {-1};
int16_t point_y[4] = {-1};
int16_t current_point = -1, next_point = 0;
int16_t touched_x[4] = {-1};
int16_t touched_y[4] = {-1};

void setup(void)
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("Arduino_GFX Touch Calibration example");

  Serial.println("Init display");
  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  // Init touch device
  w = gfx->width();
  h = gfx->height();
  touch_init(w, h, gfx->getRotation());

// not yet know TOUCH_MODULE_ADDR, scan I2C devices
#if defined(TOUCH_SDA) && !defined(TOUCH_MODULE_ADDR)
  for (uint8_t addr = 0x01; addr < 0x7f; addr++)
  {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0)
    {
      Serial.printf("I2C device found at 0x%02X\n", addr);
    }
    else if (error != 2)
    {
      Serial.printf("Error %d at 0x%02X\n", error, addr);
    }
  }
#endif

  // Top left
  point_x[0] = w / 8;
  point_y[0] = h / 8;
  // Top right
  point_x[1] = point_x[0] * 7;
  point_y[1] = point_y[0];
  // Bottom left
  point_x[2] = point_x[0];
  point_y[2] = point_y[0] * 7;
  // Bottom right
  point_x[3] = point_x[1];
  point_y[3] = point_y[2];

  gfx->setCursor(0, 0);
  gfx->setTextColor(RGB565_RED);
  gfx->setTextSize(2);
  gfx->println("Touch Calibration");
}

void loop()
{
  if (current_point != next_point)
  {
    current_point = next_point;

    gfx->drawLine(
        point_x[current_point] - 5,
        point_y[current_point] - 5,
        point_x[current_point] + 5,
        point_y[current_point] + 5,
        RGB565_RED);
    gfx->drawLine(
        point_x[current_point] + 5,
        point_y[current_point] - 5,
        point_x[current_point] - 5,
        point_y[current_point] + 5,
        RGB565_RED);
  }

  if (touch_touched())
  {
    int32_t total_x = touch_raw_x, total_y = touch_raw_y;
    int count = 1;
    while (touch_touched())
    {
      total_x += touch_raw_x;
      total_y += touch_raw_y;
      count++;
      // Serial.printf("touch_raw_x: %d, touch_raw_y: %d, count: %d\n", touch_raw_x, touch_raw_y, count);
    }
    touched_x[current_point] = total_x / count;
    touched_y[current_point] = total_y / count;
    // Serial.printf("touched_x: %d, touched_y: %d\n", touched_x[current_point], touched_y[current_point]);

    if (current_point == 3)
    {
      Serial.printf("touched_x[0]: %d, touched_y[0]: %d\n", touched_x[0], touched_y[0]);
      Serial.printf("touched_x[1]: %d, touched_y[1]: %d\n", touched_x[1], touched_y[1]);
      Serial.printf("touched_x[2]: %d, touched_y[2]: %d\n", touched_x[2], touched_y[2]);
      Serial.printf("touched_x[3]: %d, touched_y[3]: %d\n", touched_x[3], touched_y[3]);
      uint16_t delta_x = (touched_x[0] > touched_x[1]) ? (touched_x[0] - touched_x[1]) : (touched_x[1] - touched_x[0]);
      uint16_t delta_y = (touched_y[0] > touched_y[1]) ? (touched_y[0] - touched_y[1]) : (touched_y[1] - touched_y[0]);

      if (delta_x > delta_y)
      {
        touch_swap_xy = false;
        touch_map_x1 = (touched_x[0] + touched_x[2]) / 2;
        touch_map_x2 = (touched_x[1] + touched_x[3]) / 2;
        touch_map_y1 = (touched_y[0] + touched_y[1]) / 2;
        touch_map_y2 = (touched_y[2] + touched_y[3]) / 2;
      }
      else
      {
        touch_swap_xy = true;
        touch_map_x1 = (touched_y[0] + touched_y[2]) / 2;
        touch_map_x2 = (touched_y[1] + touched_y[3]) / 2;
        touch_map_y1 = (touched_x[0] + touched_x[1]) / 2;
        touch_map_y2 = (touched_x[2] + touched_x[3]) / 2;
      }

      if (touch_map_x1 > touch_map_x2)
      {
        delta_x = (touch_map_x1 - touch_map_x2) / 6;
        touch_map_x1 += delta_x;
        touch_map_x2 -= delta_x;
      }
      else
      {
        delta_x = (touch_map_x2 - touch_map_x1) / 6;
        touch_map_x1 -= delta_x;
        touch_map_x2 += delta_x;
      }

      if (touch_map_y1 > touch_map_y2)
      {
        delta_y = (touch_map_y1 - touch_map_y2) / 6;
        touch_map_y1 += delta_y;
        touch_map_y2 -= delta_y;
      }
      else
      {
        delta_y = (touch_map_y2 - touch_map_y1) / 6;
        touch_map_y1 -= delta_y;
        touch_map_y2 += delta_y;
      }

      Serial.printf("bool touch_swap_xy = %s;\n", touch_swap_xy ? "true" : "false");
      Serial.printf("int16_t touch_map_x1 = %d;\n", touch_map_x1);
      Serial.printf("int16_t touch_map_x2 = %d;\n", touch_map_x2);
      Serial.printf("int16_t touch_map_y1 = %d;\n", touch_map_y1);
      Serial.printf("int16_t touch_map_y2 = %d;\n", touch_map_y2);

      gfx->setCursor(0, point_y[0] + 10);
      gfx->setTextColor(RGB565_WHITE);
      gfx->setTextSize(1);
      gfx->printf("bool touch_swap_xy = %s;\n", touch_swap_xy ? "true" : "false");
      gfx->printf("int16_t touch_map_x1 = %d;\n", touch_map_x1);
      gfx->printf("int16_t touch_map_x2 = %d;\n", touch_map_x2);
      gfx->printf("int16_t touch_map_y1 = %d;\n", touch_map_y1);
      gfx->printf("int16_t touch_map_y2 = %d;\n", touch_map_y2);

      // wait next touch to continue
      while (!touch_touched())
        ;

      gfx->fillScreen(RGB565_BLACK);
      gfx->setCursor(0, 0);
      gfx->setTextColor(RGB565_RED);
      gfx->setTextSize(2);
      gfx->println("Touch Calibration");
    }

    next_point = (current_point == 3) ? 0 : (current_point + 1);
  }

  delay(100);
}
