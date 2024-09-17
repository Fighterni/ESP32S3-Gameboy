#ifndef CPU_H
#define CPU_H
#include "rom.h"
void cpu_init(void);
unsigned int cpu_cycle(void);
unsigned short cpu_get_pc();
unsigned int cpu_get_cycles(void);
void cpu_interrupt(unsigned short);
#endif
