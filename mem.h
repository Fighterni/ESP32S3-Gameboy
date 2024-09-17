#ifndef MEM_H
#define MEM_H
#ifdef __cplusplus

extern "C" {

#endif

#include <cinttypes>

#include "rom.h"
void gameboy_mem_init(void);
unsigned char mem_get_byte(unsigned short);
unsigned short mem_get_word(unsigned short);
void mem_write_byte(unsigned short, unsigned char);
void mem_write_word(unsigned short, unsigned short);
void mem_bank_switch(unsigned int);
const unsigned char *mem_get_raw();
uint32_t mem_get_bank_switches();
#ifdef __cplusplus
}

#endif /* end of __cplusplus */
#endif
