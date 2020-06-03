// cmdprog.h - a command to edit and compile an assembler program
#ifndef __CMDPROG_H__
#define __CMDPROG_H__

// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdprog_register(void);

#endif
