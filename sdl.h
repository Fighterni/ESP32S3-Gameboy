#include <Arduino.h>
#ifndef SDL_H
#define SDL_H

//Only for the MAX_FILENAME_LEN and MAX_FILES
#include "sd.h"

int sdl_update(void);
void sdl_init(void);
void sdl_frame(void);
void sdl_quit(void);
uint8_t *sdl_get_framebuffer(void);
unsigned int sdl_get_buttons(void);
unsigned int sdl_get_directions(void);
void display_files_on_lcd(char file_list[MAX_FILES][MAX_FILENAME_LEN], int file_count);
#endif
