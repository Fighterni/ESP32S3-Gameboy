#ifndef SD_H
#define SD_H

void sd_init();

#define MAX_FILES 5       // Maximum number of files to store
#define MAX_FILENAME_LEN 50 // Maximum length of each file name



// Declaration of the function that lists the files
void sd_list_files(char file_list[MAX_FILES][MAX_FILENAME_LEN], int *file_count);

#endif
