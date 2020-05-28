// cmdwrite.cpp - command to write to memory


#include <Arduino.h>
#include "cmd.h"
#include "cmdwrite.h"


// Next addr to write to in streaming mode
static uint16_t cmdwrite_addr;


// This notification called for a write command; first changed address is passed.
// For laziness, the handling is implemented here, not on app level
static void cmdwrite_notify(uint16_t addr) {
  // set read/dasm pointer (so that they can show what has just been written)
  extern uint16_t cmddasm_addr;
  cmddasm_addr= addr; 
  extern uint16_t cmdread_addr;
  cmdread_addr= addr; 
}


static void cmdwrite_stream( int argc, char * argv[] ) {
  if( argc==0 ) { // no arguments toggles streaming mode
    if( cmd_get_streamfunc()==0 ) cmd_set_streamfunc(cmdwrite_stream); else cmd_set_streamfunc(0);
  }
  for( int i=0; i<argc; i++ ) {
    uint16_t data;
    if( !cmd_parse(argv[i],&data) || data>0xFF ) { 
      Serial.print(F("ERROR: write: <data> must be 00..FF, not '")); 
      Serial.print(argv[i]); Serial.print(F(", "));
      if( i<argc-1 ) Serial.print(F("rest ")); 
      Serial.println(F("ignored"));
      break; // Not return, we need to update streaming prompt
    }
    mem_write(cmdwrite_addr, data);
    cmdwrite_addr++;
  }
  // Set the streaming prompt (will only be shown in streaming mode)
  char buf[8]; snprintf_P(buf,sizeof buf, PSTR("w%04x> "),cmdwrite_addr); cmd_set_streamprompt(buf);
}


// The statistics command handler
static void cmdwrite_main(int argc, char * argv[]) {
  uint16_t addr;
  if( argc<2 ) {  Serial.println(F("ERROR: write: insufficient arguments (<addr>)")); return; }
  if( !cmd_parse(argv[1],&addr) ) {  Serial.println(F("ERROR: write: expected hex <addr>")); return; }
  cmdwrite_addr= addr;
  cmdwrite_notify(addr); // Notify first changes address
  cmdwrite_stream(argc-2,argv+2); // skip 'write addr'
}


// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
static const char cmdwrite_longhelp[] PROGMEM = 
  "SYNTAX: write <addr> <data>...\n"
  "- writes <data> byte to memory location <addr>\n"
  "- multiple <data> bytes allowed (auto increment of <addr>)\n"
  "- if <data> is absent, starts streaming mode (empty line ends it)\n"
  "NOTE:\n"
  "- <data> is 00..FF\n"
  "- <addr> is 0000..FFFF, but physical memory is limited and mirrored\n"
;


// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdwrite_register(void) {
  cmd_register(cmdwrite_main, PSTR("write"), PSTR("write to memory (supports streaming)"), cmdwrite_longhelp);
}

