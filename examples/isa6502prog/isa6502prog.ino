// isa6502prog.ino - demo of the prog command to edit and compile an assembler program

#include "isa.h"
#include "cmd.h"
#include "cmdman.h"
#include "cmdread.h"
#include "cmdwrite.h"
#include "cmddasm.h"
#include "cmdasm.h"
#include "cmdprog.h"


// The read, write, asm and dasm commands expect a memory
#define MEM_SIZE 1024
uint8_t mem[MEM_SIZE]={0};
uint8_t mem_read(uint16_t addr) { return mem[addr%MEM_SIZE];}
void    mem_write(uint16_t addr, uint8_t data) { mem[addr%MEM_SIZE]=data; }


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print( F("Welcome to isa6502prog, using  is6502 lib V") ); Serial.println(ISA_VERSION);
  Serial.println( );
  Serial.println( F("Type 'help' for help") );
  cmd_begin();
  // Register in alphabetical order
  cmdasm_register();  
  cmddasm_register();  
  cmdecho_register();
  cmdhelp_register();
  cmdman_register();
  cmdread_register(); 
  cmdwrite_register();
  cmdprog_register();
}


void loop() {
  cmd_pollserial();
}
