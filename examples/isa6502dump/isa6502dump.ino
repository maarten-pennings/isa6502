// isa6502dump.ino - demo of dumping the 6502 data tables

#include "isa.h"

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print( F("Welcome to isa6502dump using lib V") ); Serial.println(ISA_VERSION);
  Serial.println();
  
  for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
    Serial.print(aix); Serial.print("; ");
    Serial.print(f(isa_addrmode_aname (aix))); Serial.print("; ");
    Serial.print(  isa_addrmode_bytes (aix) ); Serial.print("; ");
    Serial.print(f(isa_addrmode_desc  (aix))); Serial.print("; ");
    Serial.print(f(isa_addrmode_syntax(aix))); Serial.print("; ");
    for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
      uint8_t opcode= isa_instruction_opcodes(iix,aix);
      if( opcode!=ISA_OPCODE_INVALID ) {
        Serial.print(f(isa_instruction_iname(isa_opcode_iix(opcode)))); Serial.print("; ");
      }
    }
    // Test of find
    // Get the aname of the address mode with index aix
    char aname[4]; strcpy_P(aname,isa_addrmode_aname(aix));
    // Then, find the address mode by name, and print the aix
    Serial.println( isa_addrmode_find(aname) );
  }
  Serial.println();

  for( int iix= ISA_IIX_FIRST; iix<ISA_IIX_LAST; iix++ ) {
    Serial.print(iix); Serial.print("; ");
    Serial.print(f(isa_instruction_iname (iix))); Serial.print("; ");
    Serial.print(f(isa_instruction_desc  (iix))); Serial.print("; ");
    Serial.print(f(isa_instruction_help  (iix))); Serial.print("; ");
    Serial.print(f(isa_instruction_flags (iix))); Serial.print("; ");
    for( int aix= ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++ ) {
      uint8_t opcode= isa_instruction_opcodes(iix,aix);
      if( opcode!=ISA_OPCODE_INVALID ) {
        Serial.print(f(isa_addrmode_aname(isa_opcode_aix(opcode)))); Serial.print("; ");
      }
    }
    // Test of find
    // Get the iname of the instruction with index iix
    char iname[4]; strcpy_P(iname,isa_instruction_iname(iix));
    // Then, find the instruction by name, and print the iix
    Serial.println( isa_instruction_find(iname) );
  }
  Serial.println();

  Serial.print("  |");
  for( int x=0; x<16; x++ ) {
    Serial.print("0");
    Serial.print(x,HEX);
    Serial.print(" |");
  }
  Serial.println();
  for( int y=0; y<16; y++) {
    Serial.print(y,HEX);
    Serial.print("0|");
    for( int x=0; x<16; x++ ) {
      int opcode=y*16+x;
      int iix= isa_opcode_iix(opcode);
      if( iix!=0 ) Serial.print(f(isa_instruction_iname(iix))); else Serial.print("   "); 
      Serial.print("|");
    }
    Serial.println();
    Serial.print("  |");
    for( int x=0; x<16; x++ ) {
      int opcode=y*16+x;
      int aix= isa_opcode_aix(opcode);
      if( aix!=0 ) Serial.print(f(isa_addrmode_aname(aix))); else Serial.print("   ");
      Serial.print("|");
    }
    Serial.println();
  }
  Serial.println();

}


void loop() {
}