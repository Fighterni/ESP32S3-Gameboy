#include "sd.h"
#include "sdl.h"
#include <Arduino.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_heap_caps.h"

// GPIO pin definitions for SD card SPI interface
#define SD_CS GPIO_NUM_21   // Chip select (CS) pin
#define SD_MISO GPIO_NUM_16 // Master in, slave out (MISO) pin
#define SD_MOSI GPIO_NUM_17 // Master out, slave in (MOSI) pin
#define SD_CLK GPIO_NUM_18  // Clock (CLK) pin

sdmmc_card_t *card;

/**
 * @brief Initializes the SD card and mounts the filesystem.
 * 
 * This function sets up the SPI bus for SD card communication, initializes the
 * SD card, and mounts the filesystem. If initialization or mounting fails, 
 * it prints error messages and exits. Once mounted, the SD card can be accessed
 * via the specified mount point.
 */
void sd_init() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // Ensure CS pin is inactive (high) initially
  esp_err_t ret;

  // Configuration for mounting the SD card filesystem
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,   // Do not format the card if mounting fails
    .max_files = 5,                   // Max number of files open simultaneously
    .allocation_unit_size = 16 * 1024 // Allocation unit size in bytes
  };

  const char mount_point[] = "/sdcard";
  printf("Initializing SD card\n");

  // Configure SPI bus for SD card communication
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST; // Use SPI3 channel for communication

  spi_bus_config_t bus_cfg = {
    .mosi_io_num = SD_MOSI,
    .miso_io_num = SD_MISO,
    .sclk_io_num = SD_CLK,
    .quadwp_io_num = -1, // No quad write protection pin
    .quadhd_io_num = -1, // No quad hold pin
    .max_transfer_sz = 4000 // Maximum transfer size for SPI transactions
  };

  // Initialize the SPI bus
  ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    printf("Failed to initialize SPI bus for SD.\n");
    return;
  }

  // Configure the SD card device settings
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = SD_CS; // Chip select pin
  slot_config.host_id = (spi_host_device_t)host.slot;

  printf("Mounting filesystem\n");
  
  // Mount the filesystem on the SD card
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      printf("Failed to mount filesystem. Check if the SD card is formatted.\n");
    } else {
      printf(
        "Failed to initialize the card. Make sure SD card lines have pull-up resistors.\n"
      );
    }
    return;
  }
  printf("SD filesystem mounted successfully\n");

  // Print detailed information about the SD card
  sdmmc_card_print_info(stdout, card);
}

#include <dirent.h>
#include <stdio.h>

/**
 * @brief Lists all files with a ".gb" extension in the root of the SD card.
 * 
 * This function scans the SD card directory for files with the ".gb" extension
 * (typically Game Boy ROMs) and stores their names in an array.
 * 
 * @param file_list An array to store the filenames of matching files.
 * @param file_count Pointer to an integer to store the number of matching files found.
 * @return true if the directory was successfully read, false otherwise.
 */
bool sd_list_files(char file_list[MAX_FILES][MAX_FILENAME_LEN], int *file_count) {
  const char *base_path = "/sdcard"; // Root directory of the SD card
  DIR *dir = opendir(base_path);    // Open the directory
  *file_count = 0;                  // Initialize file count

  if (dir == NULL) {
    printf("Failed to open directory: %s\n", base_path);
    return false; // Directory not accessible or SD card not mounted
  }

  struct dirent *entry;
  printf("Listing files in %s:\n", base_path);

  // Iterate through directory entries
  while ((entry = readdir(dir)) != NULL) {
    // Check if the entry is a regular file
    if (entry->d_type == DT_REG) {
      const char *file_name = entry->d_name;
      const char *ext = strrchr(file_name, '.'); // Find the last dot in the filename

      // Check if the file has a ".gb" extension
      if (ext != NULL && strcmp(ext, ".gb") == 0) {
        // Copy the filename to the output list
        strncpy(file_list[*file_count], file_name, MAX_FILENAME_LEN - 1);
        file_list[*file_count][MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
        (*file_count)++; // Increment the count of files
      }
    }
  }

  closedir(dir); // Close the directory
  return true;   // Successfully listed files
}

/**
 * @brief Reads the contents of a file from the SD card into memory.
 * 
 * This function reads the entire contents of a specified file into a dynamically
 * allocated buffer in PSRAM. It validates file size, checks memory availability,
 * and handles errors gracefully.
 * 
 * @param file_name Name of the file to be read (relative to the SD card root).
 * @return A pointer to the buffer containing the file contents, or NULL if an error occurs.
 *         The caller is responsible for freeing the allocated memory.
 */
unsigned char *sd_read_file(const char *file_name) {
  // Construct the full path to the file
  char full_path[MAX_FILENAME_LEN + 8]; // Extra space for "/sdcard/"
  snprintf(full_path, sizeof(full_path), "/sdcard/%s", file_name);

  printf("File path: %s\n", full_path); // Debug output of the file path

  // Open the file in binary read mode
  FILE *file = fopen(full_path, "rb");
  if (file == NULL) {
    printf("Error opening file: %s\n", full_path);
    return NULL; // Return NULL if the file cannot be opened
  }

  // Determine the size of the file
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Check for valid file size
  if (file_size <= 0) {
    printf("Invalid file size: %ld\n", file_size);
    fclose(file);
    return NULL;
  }

  printf("File size: %ld bytes\n", file_size); // Debug output

  // Check available PSRAM
  size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  printf("Available memory: %zu bytes\n", free_mem);

  if (file_size > free_mem) {
    printf("Not enough memory to load the file (file size: %ld bytes).\n", file_size);
    fclose(file);
    return NULL; // Insufficient memory
  }

  // Allocate memory for the file contents
  unsigned char *buffer = (unsigned char *)heap_caps_malloc(file_size + 1, MALLOC_CAP_SPIRAM);
  if (buffer == NULL) {
    printf("Memory allocation failed for file content.\n");
    fclose(file);
    return NULL;
  }

  printf("Memory allocation successful.\n");
  printf("Remaining memory after allocation: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  // Read the file contents into the buffer
  size_t bytes_read = fread(buffer, 1, file_size, file);
  if (bytes_read != file_size) {
    printf("Failed to read the entire file (read: %zu, expected: %ld).\n", bytes_read, file_size);
    free(buffer); // Free allocated memory on failure
    fclose(file);
    return NULL;
  }

  buffer[file_size] = '\0'; // Null-terminate the buffer for safety

  fclose(file); // Close the file
  return buffer; // Return the buffer containing the file contents
}
