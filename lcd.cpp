#include "lcd.h"
#include <Arduino.h>
#include "cpu.h"
#include "interrupt.h"
#include "mem.h"
#include "sdl.h"

static int lcd_line;
static int lcd_ly_compare;

/* LCD STAT */
static int ly_int; /* LYC = LY coincidence interrupt enable */
static int mode2_oam_int;
static int mode1_vblank_int;
static int mode0_hblank_int;
static int ly_int_flag;
static int lcd_mode;

/* LCD Control */
static int lcd_enabled;
static int window_tilemap_select;
static int window_enabled;
static int tilemap_select;
static int bg_tiledata_select;
static int sprite_size;
static int sprites_enabled;
static int bg_enabled;
static int scroll_x, scroll_y;
static int window_x, window_y;

static byte bgpalette[] = {3, 2, 1, 0};
static byte sprpalette1[] = {0, 1, 2, 3};
static byte sprpalette2[] = {0, 1, 2, 3};

struct sprite {
    int y, x, tile, flags;
};

enum { PRIO = 0x80, VFLIP = 0x40, HFLIP = 0x20, PNUM = 0x10 };

unsigned char lcd_get_stat(void) { 
    return (ly_int << 6) | lcd_mode; 
}

void lcd_write_bg_palette(unsigned char n) {
    bgpalette[0] = (n >> 0) & 3;
    bgpalette[1] = (n >> 2) & 3;
    bgpalette[2] = (n >> 4) & 3;
    bgpalette[3] = (n >> 6) & 3;
}

void lcd_write_spr_palette1(unsigned char n) {
    sprpalette1[0] = 0;
    sprpalette1[1] = (n >> 2) & 3;
    sprpalette1[2] = (n >> 4) & 3;
    sprpalette1[3] = (n >> 6) & 3;
}

void lcd_write_spr_palette2(unsigned char n) {
    sprpalette2[0] = 0;
    sprpalette2[1] = (n >> 2) & 3;
    sprpalette2[2] = (n >> 4) & 3;
    sprpalette2[3] = (n >> 6) & 3;
}

void lcd_write_scroll_x(unsigned char n) {
    scroll_x = n;
}

void lcd_write_scroll_y(unsigned char n) { 
    scroll_y = n; 
}

int lcd_get_line(void) { 
    return lcd_line; 
}

void lcd_write_stat(unsigned char c) { 
    ly_int = !!(c & 0x40); 
}

void lcd_write_control(unsigned char c) {
    bg_enabled = !!(c & 0x01);
    sprites_enabled = !!(c & 0x02);
    sprite_size = !!(c & 0x04);
    tilemap_select = !!(c & 0x08);
    bg_tiledata_select = !!(c & 0x10);
    window_enabled = !!(c & 0x20);
    window_tilemap_select = !!(c & 0x40);
    lcd_enabled = !!(c & 0x80);
}

void lcd_set_ly_compare(unsigned char c) { 
    lcd_ly_compare = c; 
}

void lcd_set_window_y(unsigned char n) { 
    window_y = n; 
}

void lcd_set_window_x(unsigned char n) { 
    window_x = n; 
}

static void swap(struct sprite *a, struct sprite *b) {
    struct sprite c;
    c = *a;
    *a = *b;
    *b = c;
}

static void sort_sprites(struct sprite *s, int n) {
    int swapped, i;
    do {
        swapped = 0;
        for (i = 0; i < n - 1; i++) {
            if (s[i].x < s[i + 1].x) {
                swap(&s[i], &s[i + 1]);
                swapped = 1;
            }
        }
    } while (swapped);
}

#define GAMEBOY_HEIGHT 144
#define GAMEBOY_WIDTH 160
#define TARGET_HEIGHT 216
#define TARGET_WIDTH 240

static void draw_bg_and_window(uint8_t *frame_buffer, int line, const unsigned char *raw_mem) {
    unsigned int map_select, map_offset, tile_num, tile_addr, xm, ym;
    unsigned char b1, b2, mask, colour;

    if (window_enabled && line >= window_y && (line - window_y) < GAMEBOY_HEIGHT) {
        xm = 0;
        ym = line - window_y;
        map_select = window_tilemap_select;
    } else {
        if (!bg_enabled) {
            memset(frame_buffer + line * TARGET_WIDTH, 0, TARGET_WIDTH);
            return;
        }
        xm = scroll_x % 256;
        ym = (line + scroll_y) % 256;
        map_select = tilemap_select;
    }

    // Scaling factors
    const float scaleX = static_cast<float>(TARGET_WIDTH) / GAMEBOY_WIDTH;
    const float scaleY = static_cast<float>(TARGET_HEIGHT) / GAMEBOY_HEIGHT;

    // Draw background and window
    for (int x = 0; x < GAMEBOY_WIDTH; ++x) {
        map_offset = (ym / 8) * 32 + xm / 8;
        tile_num = raw_mem[0x9800 + map_select * 0x400 + map_offset];
        tile_addr = bg_tiledata_select ? 0x8000 + tile_num * 16 : 0x9000 + ((signed char)tile_num) * 16;

        b1 = raw_mem[tile_addr + (ym % 8) * 2];
        b2 = raw_mem[tile_addr + (ym % 8) * 2 + 1];
        mask = 128 >> (xm % 8);
        colour = (!!(b2 & mask) << 1) | !!(b1 & mask);

        int scaledX = static_cast<int>(x * scaleX);
        int scaledLine = static_cast<int>(line * scaleY);

        for (int sx = 0; sx < scaleX; ++sx) {
            if (scaledX + sx < TARGET_WIDTH) {
                int bufferIndex = scaledLine * TARGET_WIDTH + scaledX + sx;
                if (bufferIndex >= 0 && bufferIndex < TARGET_WIDTH * TARGET_HEIGHT) {
                    frame_buffer[bufferIndex] = bgpalette[colour];
                }
            }
        }

        xm = (xm + 1) % 256;
    }
}

static void draw_sprites(uint8_t *frame_buffer, int line, int nsprites, struct sprite *s, const unsigned char *raw_mem) {
    int i;

    // Scaling factors
    const float scaleX = static_cast<float>(TARGET_WIDTH) / GAMEBOY_WIDTH;
    const float scaleY = static_cast<float>(TARGET_HEIGHT) / GAMEBOY_HEIGHT;

    for (i = 0; i < nsprites; i++) {
        unsigned int b1, b2, tile_addr, sprite_line, x;

        if (s[i].x < -7 || s[i].x >= GAMEBOY_WIDTH) continue; // Sprite is outside the screen

        sprite_line = s[i].flags & VFLIP ? (sprite_size ? 15 : 7) - (line - s[i].y) : line - s[i].y;

        tile_addr = 0x8000 + (s[i].tile * 16) + sprite_line * 2;

        b1 = raw_mem[tile_addr];
        b2 = raw_mem[tile_addr + 1];

        for (x = 0; x < 8; x++) {
            unsigned char mask, colour;
            byte *pal;

            if ((s[i].x + x) < 0 || (s[i].x + x) >= GAMEBOY_WIDTH) continue;

            mask = s[i].flags & HFLIP ? 128 >> (7 - x) : 128 >> x;
            colour = ((!!(b2 & mask)) << 1) | !!(b1 & mask);
            if (colour == 0) continue;

            pal = (s[i].flags & PNUM) ? sprpalette2 : sprpalette1;

            int scaledX = static_cast<int>((s[i].x + x) * scaleX);
            int scaledLine = static_cast<int>(line * scaleY);

            if (s[i].flags & PRIO) {
                int bufferIndex = scaledLine * TARGET_WIDTH + scaledX;
                if (bufferIndex >= 0 && bufferIndex < TARGET_WIDTH * TARGET_HEIGHT) {
                    unsigned int temp = frame_buffer[bufferIndex];
                    if (temp != pal[bgpalette[0]]) continue;
                }
            }

            for (int sx = 0; sx < scaleX; ++sx) {
                int bufferIndex = scaledLine * TARGET_WIDTH + scaledX + sx;
                if (bufferIndex >= 0 && bufferIndex < TARGET_WIDTH * TARGET_HEIGHT) {
                    frame_buffer[bufferIndex] = pal[colour];
                }
            }
        }
    }
}

static void render_line(int line) {
    const unsigned char *raw_mem = mem_get_raw();
    int i, c = 0;

    struct sprite s[10];
    uint8_t *buffer = sdl_get_framebuffer();

    for (i = 0; i < 40; i++) {
        int y = raw_mem[0xFE00 + (i * 4)] - 16;
        if (line < y || line >= y + 8 + (sprite_size * 8)) continue;

        s[c].y = y;
        s[c].x = raw_mem[0xFE00 + (i * 4) + 1] - 8;
        s[c].tile = raw_mem[0xFE00 + (i * 4) + 2];
        s[c].flags = raw_mem[0xFE00 + (i * 4) + 3];
        c++;

        if (c == 10) break;
    }

    if (c) sort_sprites(s, c);

    /* Draw the background layer */
    draw_bg_and_window(buffer, line, raw_mem);

    draw_sprites(buffer, line, c, s, raw_mem);
}

#define CYCLES_PER_FRAME (70224 / 4)
#define CYCLES_PER_LINE (456 / 4)

bool lcd_cycle(unsigned int cycles) {
    static int this_frame_cycles = 0;
    static unsigned int prev_cycles = 0;
    static int sub_line = 0;
    static int prev_update_cycles = 0;

    this_frame_cycles += cycles - prev_cycles;
    prev_cycles = cycles;

    if (this_frame_cycles >= CYCLES_PER_FRAME) {
        this_frame_cycles -= CYCLES_PER_FRAME;
        prev_update_cycles -= CYCLES_PER_FRAME;
    }

    if (this_frame_cycles < 456 / 4) {
        if (this_frame_cycles < 204 / 4)
            lcd_mode = 2;
        else if (this_frame_cycles < 284 / 4)
            lcd_mode = 3;
        else {
            lcd_mode = 0;
            lcd_line = 0;
        }
        return false;
    }

    sub_line += this_frame_cycles - prev_update_cycles;
    prev_update_cycles = this_frame_cycles;

    if (sub_line >= CYCLES_PER_LINE) {
        sub_line -= CYCLES_PER_LINE;

        if (lcd_line < GAMEBOY_HEIGHT) render_line(lcd_line);

        lcd_line += 1;

        if (lcd_line >= GAMEBOY_HEIGHT) lcd_mode = 1;

        if (ly_int && lcd_line == lcd_ly_compare) interrupt(INTR_LCDSTAT);

        if (lcd_line == GAMEBOY_HEIGHT) {
            interrupt(INTR_VBLANK);
            return true;
        }
    }
    return false;
}
