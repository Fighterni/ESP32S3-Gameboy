#include "sd.h"

#include <Arduino.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define SD_CS GPIO_NUM_16
#define SD_MISO GPIO_NUM_12
#define SD_MOSI GPIO_NUM_13
#define SD_CLK GPIO_NUM_14

void sd_init() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  esp_err_t ret;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};
  sdmmc_card_t *card;
  const char mount_point[] = "/sdcard";
  printf("Initializing SD card\n");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = SD_MOSI,
      .miso_io_num = SD_MISO,
      .sclk_io_num = SD_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };
  ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg,
                           SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    printf("Failed to initialize SPI bus for SD.\n");
    return;
  }

  // This initializes the slot without card detect (CD) and write protect (WP)
  // signals. Modify slot_config.gpio_cd and slot_config.gpio_wp if your board
  // has these signals.
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = SD_CS;
  slot_config.host_id = (spi_host_device_t)host.slot;

  printf("Mounting filesystem\n");
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config,
                                &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      printf("Failed to mount filesystem.\n");
    } else {
      printf(
          "Failed to initialize the card. Make sure SD card lines have pull-up "
          "resistors in place.\n");
    }
    return;
  }
  printf("SD filesystem mounted\n");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);
}

void sd_list_files() {}
