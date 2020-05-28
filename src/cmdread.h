// cmdmem.h - implements several commands to access memory (write, read)

#include <stdint.h>


// The context is expected to implements
extern uint8_t mem_read(uint16_t addr);
extern void    mem_write(uint16_t addr, uint8_t data);


// This module implements commands
void cmdread_register(void);
void cmdwrite_register(void);
