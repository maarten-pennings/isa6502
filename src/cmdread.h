// cmdread.h - command to read from memory
#ifndef __CMDREAD_H__
#define __CMDREAD_H__


// The context is expected to implement
#include <stdint.h>
extern uint8_t  mem_read(uint16_t addr);
extern void     mem_write(uint16_t addr, uint8_t data);


// Next address to show; the default for the read command
extern uint16_t cmdread_addr; 
// This module implements a command
void cmdread_register(void);


#endif
