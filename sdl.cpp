#include "sdl.h"

#include <Arduino_GFX_Library.h>
#include "SPI.h"

// Pin definitions for the display
#define _cs 15    // Chip Select for TFT
#define _dc 5     // Data/Command for TFT
#define _mosi 13  // Master Out Slave In for TFT
#define _sclk 14  // Clock signal for TFT
#define _rst 4    // Reset pin for TFT
#define _miso 12  // Not connected
//#define _led 9  // Backlight

// Pin definitions for the input buttons
#define _left GPIO_NUM_40
#define _right GPIO_NUM_39
#define _up GPIO_NUM_41
#define _down GPIO_NUM_42
#define _select GPIO_NUM_46
#define _start GPIO_NUM_45
#define _a GPIO_NUM_48
#define _b GPIO_NUM_47

// SPI and display initialization
Arduino_DataBus *bus = new Arduino_ESP32SPI(_dc, _cs, _sclk, _mosi, _miso);
Arduino_GFX *tft = new Arduino_ILI9341(bus, _rst, 3 /* rotation */);

// Constants for screen dimensions and scaling
#define GAMEBOY_HEIGHT 144
#define GAMEBOY_WIDTH 160
#define DRAW_HEIGHT 216
#define DRAW_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320

#define SPI_FREQ 40000000

// Frame buffer for storing graphical data
static uint8_t *frame_buffer;

// Variables for button states
static int button_start, button_select, button_a, button_b, button_down,
  button_up, button_left, button_right;

// Flag to indicate when a new frame is ready
static volatile bool frame_ready = false;
TaskHandle_t draw_task_handle; // Task handle for the draw task

//Uncomment if Backlight is used
/*void backlighting(bool state) {
  if (!state) {
    digitalWrite(_led, LOW);
  } else {
    digitalWrite(_led, HIGH);
  }
}
*/

/**
 * @brief Displays a message indicating the SD card is missing.
 */
void sd_card_missing(void) {
  tft->setTextSize(3);
  tft->printf("SD card missing");
}

// Number of files to display at once
#define DISPLAY_ROWS 6

/**
 * @brief Displays a list of files on the screen and allows selection using buttons.
 *
 * @param file_list List of file names to display.
 * @param file_count Total number of files.
 * @return The index of the selected file.
 */
int display_files_on_lcd(char file_list[MAX_FILES][MAX_FILENAME_LEN], int file_count) {
    int selected_file_index = 0;       // Currently selected file
    int previous_selected_index = -1; // Tracks the last selection
    int window_start_index = 0;       // Start index for visible files

    while (1) { // Infinite loop to handle file selection
        button_update(); // Update button states

        // Logic for navigation
        if (button_up) {
            // Move to the previous file, loop to the last file if at the top
            if (selected_file_index == 0) {
                selected_file_index = file_count - 1;
                window_start_index = file_count - DISPLAY_ROWS;
            } else {
                selected_file_index--;
                if (selected_file_index < window_start_index) {
                    window_start_index--;
                }
            }
        } else if (button_down) {
            // Move to the next file, loop to the first file if at the bottom
            if (selected_file_index == file_count - 1) {
                selected_file_index = 0;
                window_start_index = 0;
            } else {
                selected_file_index++;
                if (selected_file_index >= window_start_index + DISPLAY_ROWS) {
                    window_start_index++;
                }
            }
        } else if (button_a) { // Select the file
            clearScreen();
            printf("Selected file: %s\n", file_list[selected_file_index]);
            return selected_file_index;
        }

        // Only update the display if the selection changes
        if (selected_file_index != previous_selected_index) {
            clearScreen();

            // Display visible files in the current window
            for (int i = 0; i < DISPLAY_ROWS && (window_start_index + i) < file_count; i++) {
                tft->setTextSize(2);

                int file_index = window_start_index + i;

                if (file_index == selected_file_index) {
                    tft->printf("> File: %s\n", file_list[file_index]); // Highlight selected file
                } else {
                    tft->printf("File: %s\n", file_list[file_index]);
                }
            }

            previous_selected_index = selected_file_index;
            delay(100); // Add a delay to avoid rapid button presses
        }
    }
}

/**
 * @brief Clears the screen and resets the cursor position.
 */
void clearScreen(void) {
  tft->fillScreen(BLACK);  // Set the background color to black
  tft->setCursor(0, 0);    // Move the cursor to the top-left corner
}

/**
 * @brief Task to draw the frame buffer to the display.
 *
 * @param parameter Not used in this implementation.
 */
void draw_task(void *parameter) {
  // Color palette for indexed graphics
  uint16_t color_palette[] = { 0xffff, (16 << 11) + (32 << 5) + 16, (8 << 11) + (16 << 5) + 8, 0x0000 };

  int h_offset = (SCREEN_WIDTH - DRAW_WIDTH) / 2;
  int v_offset = (SCREEN_HEIGHT - DRAW_HEIGHT) / 2;

  while (true) {
    while (!frame_ready) {
      delay(1); // Wait for the frame to be ready
    }
    frame_ready = false;

    // Draw the indexed bitmap
    tft->drawIndexedBitmap(h_offset, v_offset, frame_buffer, color_palette, DRAW_WIDTH, DRAW_HEIGHT);
  }
}

/**
 * @brief Initializes the SDL-like environment.
 */
void sdl_init(void) {
  frame_buffer = new uint8_t[DRAW_WIDTH * DRAW_HEIGHT]; // Allocate memory for the frame buffer
  tft->begin(SPI_FREQ); // Initialize the TFT with specified SPI frequency
  //pinMode(_led, OUTPUT); //Uncomment if backlight is used
  //backlighting(true); //Uncomment if backlight is used
  tft->fillScreen(BLACK);

  // Set up the GPIO pins for the buttons
  gpio_num_t gpios[] = { _left, _right, _down, _up, _start, _select, _a, _b };
  for (gpio_num_t pin : gpios) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
  }

  // Start the draw task
  xTaskCreatePinnedToCore(draw_task, "drawTask", 10000, NULL, 0, &draw_task_handle, 0);
}

/**
 * @brief Updates the SDL state and processes frames.
 */
int sdl_update(void) {
  button_update(); // Update button states
  sdl_frame();     // Process a frame
  return 0;
}

/**
 * @brief Updates the state of all buttons.
 */
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

/**
 * @brief Gets the state of the buttons as a bitfield.
 *
 * @return A bitfield representing the state of the buttons.
 */
unsigned int sdl_get_buttons(void) {
  return (button_start * 8) | (button_select * 4) | (button_b * 2) | button_a;
}

/**
 * @brief Gets the state of the directional buttons as a bitfield.
 *
 * @return A bitfield representing the state of the directional buttons.
 */
unsigned int sdl_get_directions(void) {
  return (button_down * 8) | (button_up * 4) | (button_left * 2) | button_right;
}

/**
 * @brief Gets the pointer to the frame buffer.
 *
 * @return Pointer to the frame buffer.
 */
uint8_t *sdl_get_framebuffer(void) {
  return frame_buffer;
}

/**
 * @brief Marks the frame as ready for drawing.
 */
void sdl_frame(void) {
  frame_ready = true;
}
