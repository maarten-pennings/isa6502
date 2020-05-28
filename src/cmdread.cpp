// cmdread.cpp - command to read from memory

#include <Arduino.h>
#include "cmd.h"
#include "cmdread.h"


// Next addr to read
uint16_t cmdread_addr; // Not extern, set by write/dasm


// Reads 'num' bytes from memory, starting at 'addr'.
// Prints all values to Serial (in lines of 'CMD_BYTESPERLINE' bytes).
#define CMD_BYTESPERLINE 16
static void cmdread_read( uint16_t addr, uint16_t num ) {
  char buf[8];
  int n= 0;
  while( num>0 ) {
    if( n%CMD_BYTESPERLINE==0 ) { snprintf_P(buf,sizeof(buf),PSTR("%04x:"),addr); Serial.print(buf); }
    uint8_t data= mem_read(addr);
    snprintf_P(buf,sizeof(buf),PSTR(" %02x"),data); Serial.print(buf);
    n++; num--; addr++; // auto wraps
    if( n%CMD_BYTESPERLINE==0 ) { Serial.println(); }
  }
  cmdread_addr= addr;
}


#define CMDREAD_NUM 0x40 // also in help
// The handler for the "read" command
static void cmdread_main( int argc, char * argv[] ) {
  // read [ <addr> [ <num> ] ]
  if( argc==1 ) { cmdread_read(cmdread_addr,CMDREAD_NUM); return; }
  // Parse addr
  uint16_t addr;
  if( argv[1][0]=='-' && argv[1][1]=='\0' ) 
    addr= cmdread_addr;
  else 
    if( !cmd_parse(argv[1],&addr) ) { Serial.println(F("ERROR: read: expected hex <addr>")); return; }
  if( argc==2 ) { cmdread_read(addr,CMDREAD_NUM); return; }
  // Parse num
  uint16_t num;
  if( !cmd_parse(argv[2],&num) ) { Serial.println(F("ERROR: read: expected hex <num>")); return; }
  if( argc==3 ) { cmdread_read(addr,num); return; }
  Serial.println(F("ERROR: read: too many arguments"));
}


static const char cmdread_longhelp[] PROGMEM = 
  "SYNTAX: read [ <addr> [ <num> ] ]\n"
  "- reads <num> bytes from memory, starting at location <addr>\n"
  "- when <num> is absent, it defaults to 40\n"
  "- when <addr> is absent or -, it defaults to \"previous\" address\n"
  "- <addr> and <num> are 0000..FFFF\n"
;

  
// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdread_register(void) {
  cmd_register(cmdread_main, PSTR("read"), PSTR("read from memory"), cmdread_longhelp);
}
