// isa6502cmd.ino - demo of the 6502 data tables: manual pages

#include "isa.h"
#include "cmd.h"
extern void cmdman_register(void); // we are lazy: cmdman.h does not exist because it has only one line

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print( F("Welcome to isa6502cmd lib V") ); Serial.println(ISA_VERSION);
  Serial.println( );
  Serial.println( F("Type 'help' for help") );
  cmd_begin();
  cmd_register_echo();
  cmd_register_help();
  cmdman_register();  // Register our own man command
}

void loop() {
  cmd_pollserial();
}
