# esp32-gameboy

The origin of this project is from binarman(https://github.com/binarman/esp32-gameboy)
This is a port of https://github.com/zid/gameboy to the Espressif ESP32 chip. and this project based on Arduino, and the SPI LCD the low speed. This project is used for self educational purposes and there are a couple of better repos. For example:

- Espeon(https://github.com/Ryuzaki-MrL/Espeon) used for M5STACK.
- ODROID-GO(https://github.com/hardkernel/ODROID-GO) using ESP32-WROVER MCU


# What do I need to use this?

You will need:
* A board containg an ESP32 chip and at least 4MB (32Mbit) of SPI flash & 2MB PSRAM (standard RAM of ESP32 is not enough), plus the tools to program it.
* A backup of a GameBoy ROM game cartridge (optional)
* A 320x240 ILI9341 display, controllable by a 4-wire SPI interface. You can find modules with this LCD by
looking for '2.2 inch SPI 320 240 ILI9341' on eBay or other shopping sites.
* 8 GPIO buttons and 8 pull resistors
* SD card module

# How do I hoop up my board?

### LCD

| LCD Pin | GPIO |
| ------- | ---- |
| MISO    |  19  |
| MOSI    |  23  |
| CLK     |  18  |
| CS      |   5  |
| DC      |  21  |
| RST     |  25  |
| BCKL    |  22  |

(BCKL = backlight enable, could be also marked as LED on module board)

(Make sure to also wire up the backlight and power pins.)

### Buttons

All buttons are attached to MCU board GPIO pins with [pull-up registers](https://en.wikipedia.org/wiki/Pull-up_resistor).
i.e. **VCC(3.3 V)** is connected to **GPIO** through resistor, switch is attached to **GPIO** and **GND**:

```
VCC(+3.3 V)   GND
    |          |
    |          |
 <10 k>         / (switch)
    |          /
    |          |
    +-----+----+
          |
        GPIO
```

| Button | GPIO |
| ------ | ---- |
| up     | 17   |
| down   | 27   |
| left   | 4    |
| righ   | 26   |
| start  | 35   |
| select | 15   |
| A      | 33   |
| B      | 32   |

### SD module

| SD Pin | GPIO |
| ------ | ---- |
| MISO   | 12   |
| MOSI   | 13   |
| CS     | 16   |
| CLK    | 14   |


# How do I program the chip?

1. Compile and upload esp32-gameboy.ino firmware
2. Download max 15 GB ROM you want on an SD card. In the .gb format 
3. Insert SD card and selelct the game you want to play
4. To change game restart the Gameboy with a reset button or by turnig it off and on again.

