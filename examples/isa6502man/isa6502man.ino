// isa6502man.ino - demo of the 6502 data tables: man pages

#include "isa.h"
#include "cmd.h"
#include "cmdman.h"

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print( F("Welcome to isa6502man lib V") ); Serial.println(ISA_VERSION);
  Serial.println( );
  Serial.println( F("Type 'help' for help") );
  Serial.println( F("This is a demo of the 'man' command") );
  cmd_begin();
  cmdecho_register();
  cmdhelp_register();
  cmdman_register();  // Register only the man command (inspecting tables)
}

void loop() {
  cmd_pollserial();
}