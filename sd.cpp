#include "sd.h"

#include <Arduino.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_heap_caps.h"

#define SD_CS GPIO_NUM_21
#define SD_MISO GPIO_NUM_16
#define SD_MOSI GPIO_NUM_17
#define SD_CLK GPIO_NUM_18

sdmmc_card_t *card;

void sd_init() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  esp_err_t ret;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;
  const char mount_point[] = "/sdcard";
  printf("Initializing SD card\n");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST;  //SPI3 Channal

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

#include <dirent.h>
#include <stdio.h>


bool sd_list_files(char file_list[MAX_FILES][MAX_FILENAME_LEN], int *file_count) {
  const char *base_path = "/sdcard";
  DIR *dir = opendir(base_path);
  *file_count = 0;  // Initialize file count

  if (dir == NULL) {
    printf("Failed to open directory: %s\n", base_path);
    return false; //SD card is missing
  }

  struct dirent *entry;
  printf("Listing files in %s:\n", base_path);
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG) {
      const char *file_name = entry->d_name;
      const char *ext = strrchr(file_name, '.');  // Find the last dot in the file name

      if (ext != NULL && strcmp(ext, ".gb") == 0) {
        strncpy(file_list[*file_count], file_name, MAX_FILENAME_LEN - 1);
        file_list[*file_count][MAX_FILENAME_LEN - 1] = '\0';  // Ensure null termination
        (*file_count)++;
      }
    }
  }

  closedir(dir);
  return true; //Everthing went fine
}

unsigned char *sd_read_file(const char *file_name) {
  // Erstellen des vollständigen Pfads mit einer ausreichenden Länge
  char full_path[MAX_FILENAME_LEN + 8];  // +8 für "/sdcard/"
  snprintf(full_path, sizeof(full_path), "/sdcard/%s", file_name);

  printf("Dateipfad: %s\n", full_path);  // Debug-Ausgabe des Dateipfads

  // Öffne die Datei im Lese-Modus
  FILE *file = fopen(full_path, "rb");
  if (file == NULL) {
    printf("Fehler beim Öffnen der Datei: %s\n", full_path);
    return NULL;  // Rückgabe von NULL, wenn die Datei nicht geöffnet werden kann
  }

  // Bestimmen der Dateigröße
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Überprüfen, ob die Dateigröße gültig ist
  if (file_size <= 0) {
    printf("Ungültige Dateigröße: %ld\n", file_size);
    fclose(file);
    return NULL;
  }

  printf("Dateigröße: %ld Bytes\n", file_size);  // Debug-Ausgabe der Dateigröße
  // Überprüfen, ob genügend interner Speicher verfügbar ist
  size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  printf("Verfügbarer interner Speicher: %zu Bytes\n", free_mem);

  if (file_size > free_mem) {
    printf("Nicht genug RAM, um die Datei vollständig zu laden (Dateigröße: %ld Bytes).\n", file_size);
    fclose(file);
    return NULL;  // Rückgabe von NULL, wenn nicht genug Speicher vorhanden ist
  }

  unsigned char *buffer = (unsigned char *)heap_caps_malloc(file_size + 1, MALLOC_CAP_SPIRAM);
  if (buffer == NULL) {
    printf("Fehler bei der Speicherzuweisung für den Dateiinhalt im PSRAM\n");
    fclose(file);
    return NULL;
  }
  printf("malloc erfolgreich initialisiert\n");
  printf("Verfügbarer PSRAM nach der Zuweisung: %d Bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  // Dateiinhalt lesen
  size_t bytes_read = fread(buffer, 1, file_size, file);
  if (bytes_read != file_size) {
    printf("Fehler beim vollständigen Lesen der Datei (gelesene Bytes: %zu, erwartete Bytes: %ld)\n", bytes_read, file_size);
    free(buffer);  // Speicher freigeben, wenn das Lesen nicht erfolgreich war
    fclose(file);
    return NULL;  // Rückgabe von NULL, wenn nicht alle Bytes gelesen werden konnten
  }

  buffer[file_size] = '\0';  // NULL-Terminierung des Puffers für Textdaten

  fclose(file);   // Schließen der Datei
  return buffer;  // Rückgabe des Puffers mit dem Dateiinhalt
}
