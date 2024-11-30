#include "sdl.h"

#include <Arduino_GFX_Library.h>

#include "SPI.h"


#define _cs 15     // 3 goes to TFT CS
#define _dc 5    // 4 goes to TFT DC
#define _mosi 13  // 5 goes to TFT MOSI
#define _sclk 14  // 6 goes to TFT SCK/CLK
#define _rst 4   // ESP RST to TFT RESET
#define _miso 12  // Not connected
#define _led 9 //(Falsch)

//       3.3V     // Goes to TFT LED
//       5v       // Goes to TFT Vcc
//       Gnd      // Goes to TFT Gnd
#define _left GPIO_NUM_40
#define _right GPIO_NUM_39
#define _up GPIO_NUM_41
#define _down GPIO_NUM_42
#define _select GPIO_NUM_46
#define _start GPIO_NUM_45
#define _a GPIO_NUM_48
#define _b GPIO_NUM_47

//Arduino_DataBus *bus = new Arduino_ESP32SPI(_dc);
Arduino_DataBus *bus = new Arduino_ESP32SPI(_dc, _cs, _sclk, _mosi, _miso);
Arduino_GFX *tft = new Arduino_ILI9341(bus, _rst, 3 /* rotation */);

void backlighting(bool state) {
  if (!state) {
    digitalWrite(_led, LOW);
  } else {
    digitalWrite(_led, HIGH);
  }
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


void sd_card_missing(void){
tft->setTextSize(3);
tft->printf("SD card missing");
}

#define DISPLAY_ROWS 6  // Anzahl der Dateien, die gleichzeitig auf dem Display angezeigt werden

int display_files_on_lcd(char file_list[MAX_FILES][MAX_FILENAME_LEN], int file_count) {
    int selected_file_index = 0;       // Der Index der aktuell ausgewählten Datei
    int previous_selected_index = -1;  // Der vorherige Index, initial auf -1 gesetzt
    int window_start_index = 0;        // Startindex für das Anzeigefenster

    while (1) {  // Endlosschleife, um die Datei auszuwählen
        button_update();

        // Logik für die Tasten
        if (button_up) {  // U für "Up"
            if (selected_file_index == 0) {
                selected_file_index = file_count - 1;  // Gehe zum letzten Element, wenn wir ganz oben sind
                window_start_index = file_count - DISPLAY_ROWS;  // Fenster anpassen
            } else {
                selected_file_index--;  // Gehe eine Datei nach oben
                if (selected_file_index < window_start_index) {
                    window_start_index--;  // Fenster verschieben
                }
            }
        } else if (button_down) {  // D für "Down"
            if (selected_file_index == file_count - 1) {
                selected_file_index = 0;  // Gehe zum ersten Element, wenn wir ganz unten sind
                window_start_index = 0;  // Fenster zurücksetzen
            } else {
                selected_file_index++;  // Gehe eine Datei nach unten
                if (selected_file_index >= window_start_index + DISPLAY_ROWS) {
                    window_start_index++;  // Fenster verschieben
                }
            }
        } else if (button_a) {  // S für "Select"
            clearScreen();
            printf("Ausgewählte Datei: %s\n", file_list[selected_file_index]);
            return selected_file_index;  // Auswahl zurückgeben
        }

        // Nur bei Änderung der Auswahl aktualisieren
        if (selected_file_index != previous_selected_index) {
            clearScreen();

            // Nur die Dateien im aktuellen Fenster anzeigen
            for (int i = 0; i < DISPLAY_ROWS && (window_start_index + i) < file_count; i++) {
                tft->setTextSize(2);  // Normale Größe für alle Dateien

                int file_index = window_start_index + i;  // Datei im Fenster

                if (file_index == selected_file_index) {
                    tft->printf("> File: %s\n", file_list[file_index]);  // Markiere die ausgewählte Datei
                } else {
                    tft->printf("File: %s\n", file_list[file_index]);
                }
            }

            // Speichere den aktuellen Index als vorherigen Index
            previous_selected_index = selected_file_index;
            delay(100);  // Leichte Verzögerung für bessere Lesbarkeit
        }
    }
}
void clearScreen(void) {
  tft->fillScreen(BLACK);  // Setze Hintergrundfarbe (abhängig von deiner Bibliothek)
  tft->setCursor(0, 0);    // Setze den Cursor an die Position (0, 0)
}

void draw_task(void *parameter) {
  uint16_t color_palette[] = { 0xffff, (16 << 11) + (32 << 5) + 16, (8 << 11) + (16 << 5) + 8, 0x0000 };

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
  // GFX_EXTRA_PRE_INIT();
  tft->begin(SPI_FREQ);
  pinMode(_led, OUTPUT);
  backlighting(true);
  tft->fillScreen(BLACK);

  gpio_num_t gpios[] = { _left, _right, _down, _up, _start, _select, _a, _b };
  for (gpio_num_t pin : gpios) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    // uncomment to use builtin pullup resistors
    //    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
  }

  xTaskCreatePinnedToCore(draw_task,         /* Function to implement the task */
                          "drawTask",        /* Name of the task */
                          10000,             /* Stack size in words */
                          NULL,              /* Task input parameter */
                          0,                 /* Priority of the task */
                          &draw_task_handle, /* Task handle. */
                          0);                /* Core where the task should run */
}

int sdl_update(void) {
  button_update();
  sdl_frame();
  return 0;
}
void button_update(void) {
  button_up = gpio_get_level(_up);
  button_left = gpio_get_level(_left);
  button_down = gpio_get_level(_down);
  button_right = gpio_get_level(_right);

  button_start = gpio_get_level(_start);
  button_select = gpio_get_level(_select);

  button_a = gpio_get_level(_a);
  button_b = gpio_get_level(_b);
}

unsigned int sdl_get_buttons(void) {
  unsigned int buttons =
    (button_start * 8) | (button_select * 4) | (button_b * 2) | button_a;
  return buttons;
}

unsigned int sdl_get_directions(void) {
  return (button_down * 8) | (button_up * 4) | (button_left * 2) | button_right;
}

uint8_t *sdl_get_framebuffer(void) {
  return frame_buffer;
}

void sdl_frame(void) {
  frame_ready = true;
}
