// isa6502dasm.ino - demo of the 6502 data tables: disassembler

#include "isa.h"
#include "cmd.h"

// we are lazy: cmdmem.h does not exist because it has just the registration functions
extern void cmdread_register(void); 
extern void cmdwrite_register(void); 
extern void cmdmem_register(void); 
extern void cmddasm_register(void); 


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print( F("Welcome to isa6502dasm lib V") ); Serial.println(ISA_VERSION);
  Serial.println( );
  Serial.println( F("Type 'help' for help") );
  cmd_begin();
  // Register in alphabetical order
  cmddasm_register();  
  cmd_register_echo();
  cmd_register_help();
  cmdmem_register();  
  cmdread_register(); 
  cmdwrite_register();
}


void loop() {
  cmd_pollserial();
}
