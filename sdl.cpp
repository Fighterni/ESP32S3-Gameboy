#include "sdl.h"

#include <Arduino_GFX_Library.h>
#include "SPI.h"

#define _cs 5     // 3 goes to TFT CS
#define _dc 21    // 4 goes to TFT DC
#define _mosi 23  // 5 goes to TFT MOSI
#define _sclk 18  // 6 goes to TFT SCK/CLK
#define _rst 25   // ESP RST to TFT RESET
#define _miso 19  // Not connected
#define _led 22
//       3.3V     // Goes to TFT LED
//       5v       // Goes to TFT Vcc
//       Gnd      // Goes to TFT Gnd
#define _left GPIO_NUM_4
#define _right GPIO_NUM_12
#define _up GPIO_NUM_17
#define _down GPIO_NUM_16
#define _select GPIO_NUM_34
#define _start GPIO_NUM_35
#define _a GPIO_NUM_39
#define _b GPIO_NUM_36

Arduino_DataBus *bus = new Arduino_ESP32SPI(_dc, _cs, _sclk, _mosi, _miso);
Arduino_GFX *tft = new Arduino_ILI9341(bus, _rst, 3 /* rotation */);

void backlighting(bool state) {
  digitalWrite(_led, state ? HIGH : LOW);
}

#define GAMEBOY_HEIGHT 144
#define GAMEBOY_WIDTH 160
#define DRAW_HEIGHT 216
#define DRAW_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320

#define SPI_FREQ 40000000

static uint8_t *frame_buffer;

static int button_start, button_select, button_a, button_b, button_down,
    button_up, button_left, button_right;

static volatile bool frame_ready = false;
TaskHandle_t draw_task_handle;

void draw_button(bool value, int x_pos, int y_pos, const char *label = nullptr) {
  uint16_t color = value ? 0xffff : 0x0000;
  tft->fillCircle(x_pos, y_pos, 7, color);
  if (label) {
    int len = strlen(label);
    constexpr int char_len = 8;
    int width = char_len * len;
    for (int i = 0; i < len; ++i) {
      tft->drawChar(x_pos - width / 2 + char_len * i, y_pos + 20, label[i], 0xffff, BLACK);
    }
  }
}

void draw_task(void *parameter) {
    uint16_t color_palette[] = {0xffff, 0x07e0, 0x001f, 0x0000}; // White, Green, Blue, Black
    int h_offset = (SCREEN_WIDTH - DRAW_WIDTH) / 2;
    int v_offset = (SCREEN_HEIGHT - DRAW_HEIGHT) / 2;
    
    while (true) {
        while (!frame_ready) {
            delay(1);
        }
        frame_ready = false;
        tft->drawIndexedBitmap(h_offset, v_offset, frame_buffer, color_palette, DRAW_WIDTH, DRAW_HEIGHT);
    }
}

void sdl_init(void) {
  frame_buffer = new uint8_t[DRAW_WIDTH * DRAW_HEIGHT];
  tft->begin(SPI_FREQ);
  pinMode(_led, OUTPUT);
  backlighting(true);
  tft->fillScreen(RED);

  gpio_num_t gpios[] = {_left, _right, _down, _up, _start, _select, _a, _b};
  for (gpio_num_t pin : gpios) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    // Uncomment to use builtin pullup resistors
    // gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
  }
  xTaskCreatePinnedToCore(draw_task, "drawTask", 10000, NULL, 0, &draw_task_handle, 0);
}

int sdl_update(void) {
  button_up = !gpio_get_level(_up);
  button_left = !gpio_get_level(_left);
  button_down = !gpio_get_level(_down);
  button_right = !gpio_get_level(_right);

  button_start = !gpio_get_level(_start);
  button_select = !gpio_get_level(_select);

  button_a = !gpio_get_level(_a);
  button_b = !gpio_get_level(_b);
  sdl_frame();
  return 0;
}

unsigned int sdl_get_buttons(void) {
  return (button_start * 8) | (button_select * 4) | (button_b * 2) | button_a;
}

unsigned int sdl_get_directions(void) {
  return (button_down * 8) | (button_up * 4) | (button_left * 2) | button_right;
}

uint8_t *sdl_get_framebuffer(void) { return frame_buffer; }

void sdl_frame(void) { frame_ready = true; }
