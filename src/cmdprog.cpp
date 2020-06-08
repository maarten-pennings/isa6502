// cmdprog.cpp - a command (prog) to edit and compile an assembler program

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include "isa.h"
#include "cmd.h"


// todo: all strings get F() - also in sprintf
// todo: reduce print output (all prefixes)
// todo: prog file [load name, save name, del name, dir]
// todo: extend "prog long help" to explain <line> syntax 
// todo: all hex output in uppercase (so that labels can use lower case)
// todo: check that two .ORGs do not overwrite each other
// todo: .BYTES with 8 bytes gives error, but compile seems to still use 8
// todo: ".WORDS label" does not work

// fixed length string =======================================================================

// A memory manager for strings of (max) length FS_SIZE.
// The memory manager has FS_NUM slots (blocks of FS_SIZE bytes).
#define FS_NUM 16    // Number of fixed-length-strings (power of two is good for the "mod")
#define FS_SIZE 8    // Length of the fixed-length-strings (padded with 0s; terminating 0 is not stored)
typedef char   fs_t[FS_SIZE];
static fs_t    fs_store[FS_NUM]; // slot 0 not used
static uint8_t fs_rover; // Pointer used to search next free slot

// Initialize the memory manager
static void fs_init() {
  for(int fsx=0; fsx<FS_NUM; fsx++) fs_store[fsx][0]='\0'; // mark all as free
  // Note slot 0 will always store the empty string
  fs_rover=0;
}

// This is the "malloc" of the fixed string memory manager. 
// It allocates a slot and stores `s`.
// Returns the slot where `s` is stored, or 0 for error.
// When 0 is returned, there could be multiple reasons for error
//  - no string was passed (s==0)
//  - an empty string is passed (*s=0)
//  - the string is too long
//  - the string store is depleted (no more blocks)
static uint8_t fs_add(char*s) {
  if( s==0 || *s==0 ) return 0; // no empty strings
  if( strlen(s)>FS_SIZE ) return 0; // this store accepts strings with the length equaling the block size (so no terminating 0 is stored), but not more
  int fs_rover_old= fs_rover;
  while( 1 ) {
    if( fs_rover==0 ) {fs_rover++; continue; } // skip slot 0
    if( fs_store[fs_rover][0]=='\0' ) { 
      int i=0;
      while( *s!='\0' && i<FS_SIZE ) fs_store[fs_rover][i++]= *s++; // copy the chars of the string
      while( i<FS_SIZE ) fs_store[fs_rover][i++]= '\0'; // pad slot with zero's
      return fs_rover;
    }
    fs_rover= (fs_rover+1)%FS_NUM;
    if( fs_rover==fs_rover_old ) return 0; // store full
  }
}

// This is the "free" of the memory manager.
// Marks slot `fsx` as empty, making it available for `add` again.
// It is safe to fs_del(0) - it does nothing
static void fs_del(uint8_t fsx) {
  // Serial.print("fs_del:"); Serial.println(fsx);
  if( fsx<1 || fsx>=FS_NUM ) return; // index out of range
  fs_store[fsx][0]='\0';  // mark slot as empty
}

// This is an alternative "malloc" for the fixed-string memory manager. 
// It allocates a slot and stores an array `bytes` containing `size` bytes.
// The crucial difference with the plain fs_add is that here the byte array may contain a 0 byte.
// Returns the slot where the array is stored, or 0 for error.
// When 0 is returned, there could be multiple reasons for error
//  - no bytes are passed (bytes==0)
//  - an empty bytes array is passed (size<=0)
//  - the array is too long
//  - the string store is depleted (no more blocks)
static uint8_t fs_add_raw(uint8_t * bytes, int size) {
  // Implementation solves the problem that fs_add() does not allow 0s.
  // For fs_add, a zero signals end of the string (and zeros are used to pad the string).
  // So in the fs_add_raw implementation, the MSB of the bytes are set to 1 (so bytes are never 0),
  // and all those MSB bits are collected in the first byte (which also gets its MSB 1).
  uint8_t encbytes[FS_SIZE+1]; // +1 to have space for the terminating 0
  // This sets all MSB in bytes to 1, because fs_store can not store 00 bytes
  // The MSBs are collected in the first byte (which also has its MSB set to 1)
  if( bytes==0 ) return 0; // no array is not allowed
  if( size<=0 ) return 0; // empty array not allowed
  if( size>FS_SIZE-1 ) return 0; // too long array not allowed (we need one byte for the MSBs)
  if( size>8 ) return 0; // extra check in case the macro FS_SIZE is increased above 8; the below code can not store all MSBs then
  uint8_t msbs=0x80; // This is the first byte, collecting all MSBs. It has its own MSB set.
  for( int i=0; i<size; i++ )  {
    if( bytes[i] & 0x80 ) msbs|= 1<<i;
    encbytes[i+1] = bytes[i] | 0x80; // slot zero to collect all MSBs
  }
  encbytes[size+1]='\0'; // terminate
  encbytes[0]=msbs;
  return fs_add((char*)encbytes);
}

// Returns the string from slot `fsx`. See also fs_snprint().
// If `fsx` is out of range (or deleted, or 0) returns empty string.
// The function returns a pointer to a static buffer, so only one call to this function at a time.
static char * fs_get(uint8_t fsx) {
  if( fsx<1 || fsx>=FS_NUM ) fsx=0; // return empty string
//if( fs_store[fsx][0]=='\0') fsx=0; // return empty string for free slot // if the slot is empty any how, not needed to revert to slot 0
  static char return_buf[FS_SIZE+1]; // +1 for terminating 0
  for( int i=0; i<FS_SIZE; i++) return_buf[i]= fs_store[fsx][i]; // note fs_store[i] is technically not a string; the termination 0 may be absent
  return_buf[FS_SIZE]='\0';
  return &return_buf[0];
}

// Writes the `fsx`-string (the string from slot `fsx`) to `str`.
// Writes at most `size` bytes to `str`.
// When size>0, a terminating zero will be added;
// even if the `fsx`-string is longer than `size` (the `fsx`-string will be truncated).
// If the `fsx`-string is shorter than `minlen`, spaces will be appended until `fsx`-string plus spaces equals `minlen`.
// Returns the number of characters that would be written if size were high enough (excluding the terminating 0).
// In other words, when returnvalue<size the complete `fsx`-string (with padding _and_ terminating 0) is written.
static int fs_snprint(char*str, int size, int minlen, uint8_t fsx) {
  if( fsx>=FS_NUM ) return 0; // index out of range (0 is accepted)
  if( fsx!=0 && fs_store[fsx][0]=='\0' ) return 0; // empty slot
  char * s= fs_store[fsx];
  int len=0;
  // Write `fsx`-string
  while( *s!='\0' && len<FS_SIZE ) {
    if( size>0 ) { size--; *str++= (size==0)?'\0':*s;  }
    s++; len++;
  }
  // Write padding
  while( len<minlen ) {
    if( size>0 ) { size--; *str++= (size==0)?'\0':' ';  }
    len++;
  }  
  // Write terminating 0
  if( size>0 ) { size--; *str++='\0'; }
  // len++; // snprintf doesn't count the terminating 0
  // Returns wish length
  return len;
}

// Returns the raw bytes from slot `fsx`, by writing them into `buf`.
// `bytes` must have size FS_SIZE (actually FS_SIZE-1 is enough).
// Return value is amount of valid bytes in buf.
// Note bytes may be 0 in which case still the size is returned.
static int fs_get_raw(uint8_t fsx, uint8_t * bytes) {
  if( fsx<1 || fsx>=FS_NUM || fs_store[fsx][0]=='\0' ) return 0; // index out of range or free slot
  uint8_t * encbytes= (uint8_t*)fs_store[fsx];
  uint8_t msbs=encbytes[0];
  uint8_t * p = &encbytes[1]; // item 0 is MSBs so start reading at 1
  int ix = 0; // start writing bytes[] at 0
  while( *p!=0 && ix<FS_SIZE ) {
    uint8_t bit= msbs & 1;
    msbs>>= 1;
    if( bytes ) { if( bit ) bytes[ix]=*p|0x80; else bytes[ix]=*p&~0x80; }
    p++; ix++;
  }
  return ix;
}

// Returns number of free slots.
static int fs_free(void) {
  int free= FS_NUM-1; // slot 0 excluded
  for( int fsx=1; fsx<FS_NUM; fsx++)
    if( fs_store[fsx][0]!='\0' ) free--;
  return free;
}

// For debugging: prints all slots
static void fs_dump(void) {
  Serial.print(F("String store - ")); Serial.print(fs_free()); Serial.println(F(" slots free"));
  for( int fsx=0; fsx<FS_NUM; fsx++) {
    Serial.print(fsx); Serial.print(F(". "));
    if( fsx==0 ) {
      Serial.println(F("reserved"));
    } else if( fs_store[fsx][0]=='\0' ) {
      Serial.println(F("free"));
    } else {
      char * s= fs_get(fsx);
      Serial.print('"'); Serial.print(s); Serial.print('"');
      if( s[0] & 0x80 ) { // might be bytes
        uint8_t bytes[FS_SIZE];
        Serial.print('=');
        char c='(';
        int len= fs_get_raw(fsx,bytes);
        for( int i=0; i<len; i++) { Serial.print(c); Serial.print(bytes[i],HEX); c=','; }
        Serial.print(')');
      }
      Serial.println();
    }
  }
}

// Lines (store of the program) ===================================================================

#define PACKED __attribute__((packed))

// These are the tags of the line types
#define LN_TAG_ERR            0
#define LN_TAG_COMMENT        1 // ; This is a silly program
#define LN_TAG_PRAGMA_ORG     2 //          .ORG 0200
#define LN_TAG_PRAGMA_BYTES   3 // data     .BYTES 03,01,04,01,05,09
#define LN_TAG_PRAGMA_WORDS   4 // vects    .WORDS 1234,5678,9abc
#define LN_TAG_PRAGMA_EQBYTE  5 // pi1      .EQBYTE 31
#define LN_TAG_PRAGMA_EQWORD  6 // pi2      .EQWORD 3141
#define LN_TAG_INST           7 // loop     LDA #12

// Stores "; This is a silly program"
typedef struct ln_comment_s {
  uint8_t cmt_fsxs[5]; // split the (long) comment over multiple fixed strings (5 because that is the longest other type: ln_inst_s)
} PACKED ln_comment_t;

// Stores "         .ORG 0200"
typedef struct ln_org_s {
  uint16_t addr;
} PACKED ln_org_t;

// Stores "data     .BYTES 03,01,04,01,05,09"
typedef struct ln_bytes_s {
  uint8_t lbl_fsx;
  uint8_t bytes_fsx;
} PACKED ln_bytes_t;

// Stores "vects    .WORDS 1234,5678,9abc"
typedef struct ln_words_s {
  uint8_t lbl_fsx;
  uint8_t words_fsx;
} PACKED ln_words_t;

// Stores "pi1      .EQBYTE 31"
typedef struct ln_eqbyte_s {
  uint8_t lbl_fsx;
  uint8_t byte;
} PACKED ln_eqbyte_t;

// Stores "pi2      .EQWORD 3141"
typedef struct ln_eqword_s {
  uint8_t lbl_fsx;
  uint16_t word;
} PACKED ln_eqword_t;

#define LN_FLAG_SYM       1 // operand is symbol (so label is used instead of number)
#define LN_FLAG_ABSforREL 2 // branch uses ABS notation (so '+' prefix is dropped)
// We could add a flags for dec,hec,oct,bin.
// We could add flags for expressions <, >, +1

// Stores "loop     LDA #12"
typedef struct ln_inst_s {
  uint8_t lbl_fsx;
  uint8_t opcode;
  uint16_t op; // either byte, or word, or fsx
  uint8_t flags;
} PACKED ln_inst_t;

// Stores any line (tag tell which)
typedef struct ln_s {
  uint8_t tag;
  union {
    ln_comment_t  cmt;
    ln_org_t      org;
    ln_bytes_t    bytes;
    ln_words_t    words;
    ln_eqbyte_t   eqbyte;
    ln_eqword_t   eqword;
    ln_inst_t     inst;
  };
} PACKED ln_t ;


// Parsing of lines =============================================================================

// Returns 1 if `s` has the chars allowed in a label - same as identifier in other languages
// Returns 0 otherwise (empty string, illegal chars, starts with digit)
static int ln_islabel(char * s) {
  if( s==0 || *s=='\0' )  return 0;
  int foundalpha=0;
  while( *s!=0 ) {
    char c= *s++;
    if( c=='_' ) { foundalpha=1; continue; }
    if( 'a'<=c && c<='z' ) { foundalpha=1; continue; }
    if( 'A'<=c && c<='Z' ) { foundalpha=1; continue; }
    if( '0'<=c && c<='9' && foundalpha ) continue;
    return 0;
  }
  return 1;
}

// Returns 1 if the `s` is a reserved word.
// Reserved words are all mnemonics (eg LDA) and addressing mode names (eg IND),
// but also all register names.
static int ln_isreserved(char * s) {
  if( s==0 || *s==0 ) return 0;
  if( isa_instruction_find(s) ) return 1;
  if( isa_addrmode_find(s)    ) return 1;
  if( strcasecmp_P(s,PSTR("A"  ))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("X"  ))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("Y"  ))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("S"  ))==0  ) return 1; // stack
  if( strcasecmp_P(s,PSTR("PCL"))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("PCH"))==0  ) return 1;
  if( strcasecmp_P(s,PSTR("PC" ))==0  ) return 1; // for PLH+PCL
  if( strcasecmp_P(s,PSTR("PSR"))==0  ) return 1; // status
  if( strcasecmp_P(s,PSTR("SR" ))==0  ) return 1; // alternative for status
  int ishex=1;
  while( *s!='\0' ) {
    ishex= ishex && ( ('a'<=*s && *s<='f') || ('A'<=*s && *s<='F') || ('0'<=*s && *s<='9') ); 
    s++;
  }
  return ishex;
}

#define LN_NUM 32
static uint16_t ln_num;
static ln_t ln_store[LN_NUM];

// Initializes the program store (ie clears it).
static void ln_init(void) {
  ln_num=0;
  if( sizeof(ln_t)!=6 ) Serial.println(F("ERROR: prog: packing or padding problem"));
}

static ln_t ln_temp; // buffer used for all ln_parse_xxx() parsers

// Parses `argc`/`argv` for a comment, and returns the parsed comment line (or 0 on parse error)
static ln_t * ln_parse_comment(int argc, char* argv[]) {
  ln_temp.tag= LN_TAG_COMMENT;

  char buf[FS_SIZE+1];
  unsigned int cmtix=0; // index into ln_temp.cmt.cmt_fsxs[]
  int bufix=0; // index into buf[]
  int argix=0; // index into argv[]
  int charix= 0; // index into argv[argix]
  while( argix<argc ) {
    char ch;
    if( argv[argix][charix]=='\0' ) {
      argix++; charix=0; // next argv
      ch=' ';
      if( argix==argc ) break;
    } else {
      ch= argv[argix][charix++];
    }
    if( cmtix==sizeof(ln_temp.cmt.cmt_fsxs) ) { Serial.println(F("WARNING: prog: comment truncated (line too long)")); return 0; }
    buf[bufix++]= ch;
    if( bufix==FS_SIZE ) {
      buf[FS_SIZE]= 0;
      ln_temp.cmt.cmt_fsxs[cmtix]= fs_add(buf);
      if( ln_temp.cmt.cmt_fsxs[cmtix]==0 ) { Serial.println(F("WARNING: prog: comment truncated (out of string memory)")); return 0; } 
      cmtix++;
      bufix= 0;
    }
  }
  if( bufix>0 ) {
    buf[bufix]= 0;
    ln_temp.cmt.cmt_fsxs[cmtix]= fs_add(buf);
    if( ln_temp.cmt.cmt_fsxs[cmtix]==0 ) Serial.println(F("WARNING: prog: comment truncated (out of string memory)"));
    cmtix++;
  }
  if( cmtix<sizeof(ln_temp.cmt.cmt_fsxs) ) ln_temp.cmt.cmt_fsxs[cmtix]=0;
  return &ln_temp;
}

// Parses the line consisting of three fragments: `label` `pragma` `operand` for a pragma.
// Either `label` or `operand` may be 0.
// Returns the parsed pragma line (or 0 on parse error)
static ln_t * ln_parse_pragma( char * label, char * pragma, char * operand ) {
  if( ln_isreserved(label) ) {
    Serial.println(F("ERROR: prog: label uses reserved word (or hex lookalike)")); 
    return 0; 
  }
  uint8_t lbl_fsx= fs_add(label); 
  if( label!=0 && *label!='\0' && lbl_fsx==0 ) {
    Serial.println(F("ERROR: prog: label too long or out of string memory")); 
    return 0; 
  }
  // WARNING: free resource lbl_fsx if we can not add line

  if( strcasecmp(pragma,".ORG")==0 ) {
    uint16_t addr;
    if( !cmd_parse(operand,&addr) ) {
      Serial.println(F("ERROR: prog: .org: addr must be 0000..FFFF"));   
      goto free_lvl_fsx;      
    }
    if( lbl_fsx!=0 ) { fs_del(lbl_fsx); Serial.println(F("WARNING: prog: .org: no label expected")); } 
    ln_temp.tag= LN_TAG_PRAGMA_ORG;
    ln_temp.org.addr= addr;
    return &ln_temp;
  } else if( strcasecmp(pragma,".BYTES")==0 ) {
    uint8_t bytes[FS_SIZE-1]; // to collect the bytes
    int bytesix=0;
    while( *operand!='\0' ) {
      // The string is not empty, so get a number
      char buf[3]; // to collect hex chars of the bytes (max "12\0")
      int bufix= 0;
      while( *operand!=',' && *operand!='\0' ) {
        buf[bufix++]= *operand++;
        if( bufix==sizeof(buf) ) {
          Serial.print(F("ERROR: prog: .bytes: byte ")); Serial.print(bytesix+1); Serial.println(F("must be 00..FF")); 
          goto free_lvl_fsx;        
        }
      }
      buf[bufix++]='\0';
      uint16_t word;
      if( !cmd_parse(buf,&word) || word>0xff ) {
        Serial.print(F("ERROR: prog: .bytes: byte ")); Serial.print(bytesix+1); Serial.println(F("must be 00..FF")); 
        goto free_lvl_fsx;      
      }
      if( bytesix==sizeof(bytes) ) {
        Serial.println(F("ERROR: prog: .bytes: too many bytes")); 
        goto free_lvl_fsx;        
      }
      bytes[bytesix++]= word;
      if( *operand==',' ) operand++;
    }
    if( bytesix==0 ) {
      Serial.println(F("ERROR: prog: .bytes: bytes missing")); 
      goto free_lvl_fsx;        
    }
    uint8_t bytes_fsx= fs_add_raw(bytes,bytesix);
    if( bytes_fsx==0 ) {
      Serial.println(F("ERROR: prog: out of string memory for bytes")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_BYTES;
    ln_temp.bytes.lbl_fsx= lbl_fsx;
    ln_temp.bytes.bytes_fsx= bytes_fsx;
    return &ln_temp;
  } else if( strcasecmp(pragma,".WORDS")==0 ) {
    uint16_t words[(FS_SIZE-1)/2]; // to collect the words
    int wordsix=0;
    while( *operand!='\0' ) {
      // The string is not empty, so get a number
      char buf[5]; // to collect hex chars of the words (max "1234\0")
      int bufix= 0;
      while( *operand!=',' && *operand!='\0' ) {
        buf[bufix++]= *operand++;
        if( bufix==sizeof(buf) ) {
          Serial.print(F("ERROR: prog: .words: word ")); Serial.print(wordsix+1); Serial.println(F("must be 0000..FFFF")); 
          goto free_lvl_fsx;        
        }
      }
      buf[bufix++]='\0';
      uint16_t word;
      if( !cmd_parse(buf,&word) ) {
        Serial.print(F("ERROR: prog: .words: word ")); Serial.print(wordsix+1); Serial.println(F("must be 0000..FFFF")); 
        goto free_lvl_fsx;      
      }
      if( wordsix==sizeof(words) ) {
        Serial.println(F("ERROR: prog: .words: too many words")); 
        goto free_lvl_fsx;        
      }
      words[wordsix++]= word;
      if( *operand==',' ) operand++;
    }
    if( wordsix==0 ) {
      Serial.println(F("ERROR: prog: .words: words missing")); 
      goto free_lvl_fsx;        
    }
    uint8_t words_fsx= fs_add_raw((uint8_t*)words,wordsix*2);
    if( words_fsx==0 ) {
      Serial.println(F("ERROR: prog: out of string memory for words")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_WORDS;
    ln_temp.words.lbl_fsx= lbl_fsx;
    ln_temp.words.words_fsx= words_fsx;
    return &ln_temp;
  } else if( strcasecmp(pragma,".EQBYTE")==0 ) {
    uint16_t byte;
    if( lbl_fsx==0 ) {
      Serial.println(F("ERROR: prog: .eqbyte: label missing")); 
      goto free_lvl_fsx;      
    }
    if( !cmd_parse(operand,&byte) || byte>0xff ) {
      Serial.println(F("ERROR: prog: .eqbyte: byte must be 00..FF")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_EQBYTE;
    ln_temp.eqbyte.lbl_fsx= lbl_fsx;
    ln_temp.eqbyte.byte= byte;
    return &ln_temp;
  } else if( strcasecmp(pragma,".EQWORD")==0 ) {
    uint16_t word;
    if( lbl_fsx==0 ) {
      Serial.println(F("ERROR: prog: .eqword: label missing")); 
      goto free_lvl_fsx;      
    }
    if( !cmd_parse(operand,&word) ) {
      Serial.println(F("ERROR: prog: .eqword: word must be 0000..FFFF")); 
      goto free_lvl_fsx;      
    }
    ln_temp.tag= LN_TAG_PRAGMA_EQWORD;
    ln_temp.eqword.lbl_fsx= lbl_fsx;
    ln_temp.eqword.word= word;
    return &ln_temp;
  } 
  Serial.println(F("ERROR: prog: unknown pragma")); 
  // goto free_lvl_fsx;
  
free_lvl_fsx:    
  fs_del(lbl_fsx); 
  return 0;
}

// Parses the line consisting of three fragments: `label` `iix` `operand` for an instruction.
// Either `label` or `operand` may be 0. `iix` is the instruction index (isa table)
// Returns the parsed instruction line (or 0 on parse error)
static ln_t * ln_parse_inst( char * label, int iix, char * operand ) {
  char opbuf[16]; // "(<12345678),X"
  if( operand==0 ) opbuf[0]='\0'; else strcpy(opbuf,operand);
  
  // Label
  if( ln_isreserved(label) ) {
    Serial.println(F("ERROR: prog: label uses reserved word (or hex lookalike)")); 
    return 0; 
  }
  uint8_t lbl_fsx= fs_add(label); 
  if( label!=0 && *label!='\0' && lbl_fsx==0 ) {
    Serial.println(F("ERROR: prog: out of string memory for label")); 
    return 0; 
  }
  // WARNING: free resource lbl_fsx if we can not add line

  // Addressing mode and opcode
  uint8_t flags= 0;
  int aix= isa_parse(opbuf); // Note: this isolates the actual operand chars in `opbuf`
  if( aix==0 ) {
    Serial.println(F("ERROR: prog: unknown addressing mode syntax")); 
    goto free_lvl_fsx;      
  }
  if( aix==ISA_AIX_ABS && isa_instruction_opcodes(iix,ISA_AIX_REL)!=ISA_OPCODE_INVALID ) {
    // The syntax is ABS, but the instruction has REL, accept
    flags|= LN_FLAG_ABSforREL;
    aix= ISA_AIX_REL;
  }
  uint8_t opcode;
  opcode= isa_instruction_opcodes(iix,aix);
  if( opcode==ISA_OPCODE_INVALID ) {
    Serial.println(F("ERROR: prog: instruction does not have that addressing mode")); 
    goto free_lvl_fsx;        
  }
  
  // Operand
  uint16_t op;
  if( opbuf[0]=='\0' ) {
    // No argument
  } else if( cmd_parse(opbuf,&op) ) {
    // `flags` should _not_ have LN_FLAG_SYM
    // `op` is the operand 
  } else {
    // not a hex number, so maybe a label
    if( !ln_islabel(opbuf) ) {
      Serial.println(F("ERROR: prog: operand does not have label syntax")); 
      goto free_lvl_fsx;        
    }
    if( ln_isreserved(opbuf) ) {
      Serial.println(F("ERROR: prog: operand uses reserved word (or hex lookalike)")); 
      goto free_lvl_fsx;        
    }
    flags |= LN_FLAG_SYM;
    op= fs_add(opbuf); 
    if( op==0 ) {
      Serial.println(F("ERROR: prog: out of string memory for operand")); 
      goto free_lvl_fsx;        
    }
  }

  // Create line
  ln_temp.tag= LN_TAG_INST;
  ln_temp.inst.lbl_fsx= lbl_fsx;
  ln_temp.inst.opcode= opcode;
  ln_temp.inst.flags= flags;
  ln_temp.inst.op= op;
  
  return &ln_temp;
  
free_lvl_fsx:    
  fs_del(lbl_fsx);
  return 0;  
}

static ln_t * ln_parse(int argc, char* argv[]) {
  if( argc==0 ) { 
    Serial.println(F("ERROR: prog: empty line")); 
    return 0; 
  }
  // ; comment
  // LABEL .PRAGMA OPERAND
  //       .PRAGMA OPERAND
  //       .PRAGMA            (does not exist right now)
  // LABEL OPC OPERAND  
  //       OPC OPERAND  
  // LABEL OPC
  //       OPC
  if( argv[0][0]==';' ) { 
    // ; comment
    if( argv[0][1]=='\0' ) return ln_parse_comment(argc-1,argv+1);
    Serial.println(F("ERROR: prog: comment must have space after ;")); 
    return 0;
  }
  // LABEL .PRAGMA OPERAND
  //       .PRAGMA OPERAND
  //       .PRAGMA            (does not exist right now)
  // LABEL OPC OPERAND  
  //       OPC OPERAND  
  // LABEL OPC
  //       OPC
  if( argc==1 ) {
    //       .PRAGMA            (does not exist right now)
    //       OPC
    if( argv[0][0]=='.' ) return ln_parse_pragma(0,argv[0],0); 
    uint8_t inst= isa_instruction_find(argv[0]);
    if( inst!=0 ) return ln_parse_inst(0,inst,0); 
    Serial.println(F("ERROR: prog: unknown instruction")); 
    return 0; 
  }
  // LABEL .PRAGMA OPERAND
  //       .PRAGMA OPERAND
  // LABEL OPC OPERAND  
  //       OPC OPERAND  
  // LABEL OPC
  if( argc==2 ) {
    //       .PRAGMA OPERAND
    //       OPC OPERAND  
    // LABEL OPC
    if( argv[0][0]=='.' ) return ln_parse_pragma(0,argv[0],argv[1]);
    int iix= isa_instruction_find(argv[0]);
    if( iix!=0 ) return ln_parse_inst(0,iix,argv[1]);
    iix= isa_instruction_find(argv[1]);
    if( iix!=0 ) return ln_parse_inst(argv[0],iix,0);
    Serial.println(F("ERROR: prog: unknown instruction (with label or operand)")); 
    return 0;
  }
  // LABEL .PRAGMA OPERAND
  // LABEL OPC OPERAND  
  if( argc==3 ) {
    // LABEL .PRAGMA OPERAND
    // LABEL OPC OPERAND  
    if( argv[1][0]=='.' ) return ln_parse_pragma(argv[0],argv[1],argv[2]);
    int iix= isa_instruction_find(argv[1]);
    if( iix!=0 ) return ln_parse_inst(argv[0],iix,argv[2]);
    Serial.println(F("ERROR: prog: unknown instruction (with label and operand)")); 
    return 0;
  }
  Serial.println(F("ERROR: prog: too long; expect at most \"label inst op\"")); 
  return 0;
}

void ln_del(ln_t * ln) {
  // Serial.print("ln_del "); Serial.println(ln-&ln_store[0]);
  switch( ln->tag ) {
  case LN_TAG_COMMENT       : 
    for( uint8_t i=0; i<sizeof(ln->cmt.cmt_fsxs); i++ ) fs_del(ln->cmt.cmt_fsxs[i]); 
    return;
  case LN_TAG_PRAGMA_ORG    : 
    // skip
    return;
  case LN_TAG_PRAGMA_BYTES  : 
    fs_del(ln->bytes.lbl_fsx); 
    fs_del(ln->bytes.bytes_fsx);
    return;
  case LN_TAG_PRAGMA_WORDS  : 
    fs_del(ln->words.lbl_fsx); 
    fs_del(ln->words.words_fsx);
    return;
  case LN_TAG_PRAGMA_EQBYTE : 
    fs_del(ln->eqbyte.lbl_fsx); 
    return;
  case LN_TAG_PRAGMA_EQWORD : 
    fs_del(ln->eqword.lbl_fsx); 
    return;
  case LN_TAG_INST          : 
    fs_del(ln->inst.lbl_fsx); 
    if( ln->inst.flags & LN_FLAG_SYM ) fs_del(ln->inst.op); 
    return;
  }
  Serial.print(F("ERROR: prog: delete: internal error (tag ")); Serial.print(ln->tag); Serial.println(')');
}


// Printing of lines =============================================================================


static int ln_snprint_comment(char*str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_COMMENT );
  int res, len=0;
  if( ln->cmt.cmt_fsxs[0]=='\0' ) { if(size>0) *str='\0'; return len; } // skip printing ";"
  res= snprintf(str,size,"; "); str+=res; size-=res; len+=res;
  int i=0;
  while( ln->cmt.cmt_fsxs[i]!=0 ) {
    res= fs_snprint(str,size,0,ln->cmt.cmt_fsxs[i]); str+=res; size-=res; len+=res;
    i++;
  }
  return len;
}

static int ln_snprint_org(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_ORG );
  int res, len=0; 
  // Print indent (we misuse string 0 as empty string to get an indent of FS_SIZE)
  res=fs_snprint(str, size, FS_SIZE, 0); str+=res; size-=res; len+=res;
  // Print org pragma with address
  res=snprintf(str, size, " .ORG %04x",ln->org.addr); str+=res; size-=res; len+=res;
  return len;
}

static int ln_snprint_bytes(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_BYTES );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma
  res=snprintf(str, size, " .BYTES"); str+=res; size-=res; len+=res;
  // Print bytes
  uint8_t bytes[FS_SIZE];
  int num= fs_get_raw(ln->bytes.bytes_fsx,bytes);
  char c=' ';
  for( int i=0; i<num; i++) { 
    res=snprintf(str, size, "%c%02x", c, bytes[i]); str+=res; size-=res; len+=res;
    c=','; 
  }
  return len;
}

static int ln_snprint_words(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_WORDS );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma
  res=snprintf(str, size, " .WORDS"); str+=res; size-=res; len+=res;
  // Print words
  uint16_t words[FS_SIZE/2];
  int num= fs_get_raw(ln->words.words_fsx,(uint8_t*)words);
  char c=' ';
  for( int i=0; i<num/2; i++) { 
    res=snprintf(str, size, "%c%04x", c, words[i]); str+=res; size-=res; len+=res;
    c=','; 
  }
  return len;
}

static int ln_snprint_eqbyte(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_EQBYTE );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma with value
  res=snprintf(str, size, " .EQBYTE %02x",ln->eqbyte.byte); str+=res; size-=res; len+=res;
  return len;
}

static int ln_snprint_eqword(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_PRAGMA_EQWORD );
  int res, len=0; 
  // Print label (or indent)
  res=fs_snprint(str, size, FS_SIZE, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print pragma with value
  res=snprintf(str, size, " .EQWORD %04x",ln->eqword.word); str+=res; size-=res; len+=res;
  return len;
}

static int ln_snprint_inst(char * str, int size, ln_t * ln) {
  // assert( ln->tag == LN_TAG_INST );
  int res, len=0; 
  // Print label (or indent) plus a space
  res=fs_snprint(str, size, FS_SIZE+1, ln->bytes.lbl_fsx); str+=res; size-=res; len+=res;
  // Print mnemonic
  uint8_t iix= isa_opcode_iix(ln->inst.opcode);
  res=isa_snprint_iname(str, size, 4, iix ); str+=res; size-=res; len+=res;
  // Compose operand
  uint8_t aix= isa_opcode_aix(ln->inst.opcode);
  if( ln->inst.flags & LN_FLAG_ABSforREL ) aix= ISA_AIX_ABS;
  uint8_t bytes= isa_addrmode_bytes(aix);
  char opbuf[FS_SIZE+1]; // a label, or byte or word
  if( ln->inst.flags & LN_FLAG_SYM ) fs_snprint(opbuf,FS_SIZE+1,0,ln->inst.op); // op is an fsx
  else if( bytes==1 ) opbuf[0]='\0';
  else if( bytes==2 ) snprintf(opbuf,FS_SIZE+1,"%02x",ln->inst.op); // op is a byte
  else if( bytes==3 ) snprintf(opbuf,FS_SIZE+1,"%04x",ln->inst.op); // op is a word
  // Print operand (opbuf)
  res=isa_snprint_op(str,size,aix,opbuf); str+=res; size-=res; len+=res;
  return len;
}

static int ln_snprint(char * str, int size, ln_t * ln) {
  switch( ln->tag ) {
  case LN_TAG_COMMENT       : return ln_snprint_comment(str,size,ln);
  case LN_TAG_PRAGMA_ORG    : return ln_snprint_org(str,size,ln);
  case LN_TAG_PRAGMA_BYTES  : return ln_snprint_bytes(str,size,ln);
  case LN_TAG_PRAGMA_WORDS  : return ln_snprint_words(str,size,ln);
  case LN_TAG_PRAGMA_EQBYTE : return ln_snprint_eqbyte(str,size,ln);
  case LN_TAG_PRAGMA_EQWORD : return ln_snprint_eqword(str,size,ln);
  case LN_TAG_INST          : return ln_snprint_inst(str,size,ln);
  }
  return snprintf(str,size,"internal error (tag %d)",ln->tag);
}


// Compiling =====================================================================================

// Stores compile info on a line
typedef struct comp_ln_s {
  uint16_t addr;
} comp_ln_t;
// Stores compile info on a fixed string
#define COMP_FLAGS_FSUSE   1 // When fixed string is a "using occurrence" (right hand side label)
#define COMP_FLAGS_FSDEF   2 // When fixed string is a "defining occurrence" (left hand side label)
#define COMP_FLAGS_FSOTHER 4 // When fixed string is a other (comment, bytes, words)
#define COMP_FLAGS_REFD    8 // When defining occurrence is referenced (there is a using occurrence)
#define COMP_FLAGS_BYTE   16 // When defining occurrence is a byte
typedef struct comp_fs_s {
  uint8_t  flags; // from the above COMP_FLAGS_XXX
  uint16_t val;   // for an FSDEF the value (the address, or the [eq]byte or [eq]word)
  uint8_t  first; // computed in second pass
} comp_fs_t;
// Stores compile info on entire program
typedef struct comp_s {
  comp_ln_t ln[LN_NUM+1]; // for end address
  comp_fs_t fs[FS_NUM];
  bool reset_vector_present; // if false, poke 0200 at fffc/fffd
} PACKED comp_t ;

static comp_t comp_result;

static bool comp_compile( void ) {
  int warnings=0;
  int errors=0;
  bool found_org= false;
  bool found_fffc= false; 
  bool found_fffd= false; 
  uint16_t addr=0x200;
  for( uint16_t lix=0; lix<ln_num; lix++ ) {
    ln_t * ln= &ln_store[lix]; 
    if( ln->tag==LN_TAG_COMMENT ) { 
      continue;
    }
    if( ln->tag==LN_TAG_PRAGMA_EQBYTE ) {
      uint8_t lbl= ln->eqbyte.lbl_fsx;
      if( lbl==0 ) {
        Serial.println(F("ERROR: prog: compile: label missing for .EQBYTE")); 
        warnings++;
      } else {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF | COMP_FLAGS_BYTE;
        cfs->val= ln->eqbyte.byte;
      }
      continue;
    }
    if( ln->tag==LN_TAG_PRAGMA_EQWORD ) { 
      uint8_t lbl= ln->eqword.lbl_fsx;
      if( lbl==0 ) {
        Serial.println(F("ERROR: prog: compile: label missing for .EQWORD")); 
        warnings++;
      } else {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF;
        cfs->val= ln->eqword.word;
      }
      continue;
    }
    if( ln->tag==LN_TAG_PRAGMA_ORG ) { 
      found_org= true;
      addr= ln->org.addr;
      continue;
    } 
    if( ! found_org ) {
      Serial.println(F("WARNING: prog: compile: no .ORG, assuming 0x200")); 
      found_org= true; // We "found"it, using the default
      warnings++;
    }
    if( ln->tag==LN_TAG_PRAGMA_BYTES ) { 
      uint8_t lbl= ln->bytes.lbl_fsx;
      if( lbl==0 ) {
        // skip
      } else {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF;
        cfs->val= addr;
      }
      comp_result.ln[lix].addr= addr;
      addr+= fs_get_raw(ln->bytes.bytes_fsx, 0);
      continue;
    } 
    if( ln->tag==LN_TAG_PRAGMA_WORDS ) { 
      uint8_t lbl= ln->words.lbl_fsx;
      if( lbl==0 ) {
        // skip
      } else {
        comp_fs_t * cfs= &comp_result.fs[lbl];
        cfs->flags= COMP_FLAGS_FSDEF;
        cfs->val= addr;
      }
      comp_result.ln[lix].addr= addr;
      addr+= fs_get_raw(ln->words.words_fsx, 0);
      continue;
    } 
    // Does inst have a label
    uint8_t lbl= ln->inst.lbl_fsx;
    if( lbl==0 ) {
      // skip
    } else {
      comp_fs_t * cfs= &comp_result.fs[lbl];
      cfs->flags= COMP_FLAGS_FSDEF;
      cfs->val= addr;
    }
    // Get instruction size
    uint8_t opcode= ln->inst.opcode;
    uint8_t aix= isa_opcode_aix(opcode);
    uint8_t bytes= isa_addrmode_bytes(aix);
    comp_result.ln[lix].addr= addr;
    if( addr<=0xfffc && 0xfffc<addr+bytes) found_fffc= true; // todo: does not work for .WORDS/.BYTES
    if( addr<=0xfffd && 0xfffd<addr+bytes) found_fffd= true; // todo: does not work for .WORDS/.BYTES
    addr+= bytes;
  }
  comp_result.ln[ln_num].addr= addr;
  comp_result.reset_vector_present= found_fffc && found_fffd;
  if( !comp_result.reset_vector_present ) { Serial.println(F("WARNING: prog: compile: reset vector missing (FFFC and/or FFFD), assuming 0200")); warnings++; }
  Serial.print(F("INFO: errors ")); Serial.print(errors); Serial.print(F(", warnings ")); Serial.println(warnings); 
  Serial.println(); 
  return errors==0;
}


static void comp_list( void ) {
  char buf[40]; 
  uint8_t bytes[FS_SIZE];
  int bix=0,len=0;
  for(uint16_t lix=0; lix<ln_num; lix++) {
    ln_t * ln= &ln_store[lix]; 
    if( ln->tag==LN_TAG_COMMENT || ln->tag==LN_TAG_PRAGMA_ORG || ln->tag==LN_TAG_PRAGMA_EQBYTE || ln->tag==LN_TAG_PRAGMA_EQWORD ) { 
      Serial.print(F("     |             ")); 
    } else if( ln->tag==LN_TAG_PRAGMA_BYTES ) { 
      snprintf(buf,40,"%04x | ",comp_result.ln[lix].addr); 
      Serial.print(buf); 
      len= fs_get_raw(ln->bytes.bytes_fsx,bytes);
      bix=0; while( bix<len && bix<4 ) {
        snprintf(buf,40,"%02x ",bytes[bix++]); 
        Serial.print(buf); 
      }
      for(int i=len; i<4; i++ ) Serial.print(F("   "));
    } else if( ln->tag==LN_TAG_PRAGMA_WORDS ) { 
      snprintf(buf,40,"%04x | ",comp_result.ln[lix].addr); 
      Serial.print(buf); 
      len= fs_get_raw(ln->words.words_fsx,bytes);
      bix=0; while( bix<len && bix<4 ) {
        snprintf(buf,40,"%02x ",bytes[bix++]); 
        Serial.print(buf); 
      }
      for(int i=len; i<4; i++ ) Serial.print(F("   "));
    } else {
      // Print computed address
      snprintf(buf,40,"%04x | ",comp_result.ln[lix].addr); 
      Serial.print(buf); 
      // Print opcode
      uint8_t opcode= ln->inst.opcode;
      snprintf(buf,40,"%02x ",opcode); Serial.print(buf); 
      // Print operand(s)
      uint8_t aix= isa_opcode_aix(opcode);
      uint8_t bytes= isa_addrmode_bytes(aix);
      if(      bytes==1 ) snprintf(buf,40,"         "); 
      else if( bytes==2 ) snprintf(buf,40,"%02x       ",(ln->inst.op)&0xFF ); // todo: check op is a byte (when it is a symbol)
      else if( bytes==3 ) snprintf(buf,40,"%02x %02x    ",(ln->inst.op)&0xFF, (ln->inst.op>>8)&0xFF ); // todo: op is a word (when it is a symbol)
      Serial.print(buf); 
    }
    // Print linenum
    snprintf(buf,40,"| %03x ",lix);
    Serial.print(buf); 
    // Print source
    ln_snprint(buf,40,ln);
    Serial.println(buf); 
    // Special case: too many bytes in .BYTES or .WORDS, so we print a next line
    if( ln->tag==LN_TAG_PRAGMA_BYTES && bix<len ) { 
      snprintf(buf,40,"%04x | ",comp_result.ln[lix].addr+bix); 
      Serial.print(buf); 
      while( bix<len ) {
        snprintf(buf,40,"%02x ",bytes[bix++]); 
        Serial.print(buf); 
      }
      for(int i=len; i<8; i++ ) Serial.print(F("   "));
      Serial.println(F("| more bytes")); 
    } 
    if( ln->tag==LN_TAG_PRAGMA_WORDS && bix<len ) { 
      snprintf(buf,40,"%04x | ",comp_result.ln[lix].addr+bix); 
      Serial.print(buf); 
      while( bix<len ) {
        snprintf(buf,40,"%02x ",bytes[bix++]); 
        Serial.print(buf); 
      }
      for(int i=len; i<8; i++ ) Serial.print(F("   "));
      Serial.println(F("| more words")); 
    }    
  }
  // The end address
  snprintf(buf,40,"%04x |             | end\n",comp_result.ln[ln_num].addr); 
  Serial.print(buf); 
  // Vector?
  if( !comp_result.reset_vector_present ) Serial.println(F("FFFC | 00 02       | implicit reset vector")); 
}

// Command handling ==============================================================================


static void cmdprog_list(int argc, char * argv[]) {
  uint16_t num1, num2;
  if( argc==2 ) {
    num1=0; num2=ln_num-1; 
  } else if( argc==3 ) { 
    if( !cmd_parse(argv[2],&num1) ) { Serial.println(F("ERROR: prog: list: expected hex <num1>")); return; }
    if( num1>=ln_num ) { Serial.println(F("ERROR: prog: list: <num1> too high")); return; }
    num2= num1;
  } else if( argc==4 ) {
    if( argv[2][0]=='-' && argv[2][1]=='\0' ) { num1=0; }
    else if( !cmd_parse(argv[2],&num1) ) { Serial.println(F("ERROR: prog: list: expected hex <num1>")); return; }
    if( num1>=ln_num ) { Serial.println(F("ERROR: prog: list: <num1> too high")); return; }
    if( argv[3][0]=='-' && argv[3][1]=='\0' ) { num2=ln_num-1; }    
    else if( !cmd_parse(argv[3],&num2) ) { Serial.println(F("ERROR: prog: list: expected hex <num2>")); return; }
    if( num2>=ln_num ) num2=ln_num-1;
    if( num2<num1 ) { Serial.println(F("ERROR: prog: list: <num2> less than <num1>")); return; }
  } else {
    Serial.println(F("ERROR: prog: list: too many arguments")); return;
  }
  char buf[40];
  if( ln_num>0 ) for(uint16_t i=num1; i<=num2; i++) {
    snprintf(buf,40,"%03x ",i);
    Serial.print(buf); 
    ln_snprint(buf,40,&ln_store[i]);
    Serial.println(buf); 
  }
}

static void cmdprog_delete(int argc, char * argv[]) {
  uint16_t num1, num2;
  if( argc==2 ) {
    Serial.println(F("ERROR: prog: delete: expected <num1> and <num2>")); return;
  } else if( argc==3 ) { 
    if( !cmd_parse(argv[2],&num1) ) { Serial.println(F("ERROR: prog: delete: expected hex <num1>")); return; }
    num2= num1;
  } else if( argc==4 ) {
    if( argv[2][0]=='-' && argv[2][1]=='\0' ) { num1=0; }
    else if( !cmd_parse(argv[2],&num1) ) { Serial.println(F("ERROR: prog: delete: expected hex <num1>")); return; }
    if( num1>=ln_num ) { Serial.println(F("ERROR: prog: delete: <num1> too high")); return; }
    if( argv[3][0]=='-' && argv[3][1]=='\0' ) { num2=ln_num-1; }    
    else if( !cmd_parse(argv[3],&num2) ) { Serial.println(F("ERROR: prog: delete: expected hex <num2>")); return; }
    if( num2>=ln_num ) { Serial.println(F("ERROR: prog: delete: <num2> too high")); return; }
    if( num2<num1 ) { Serial.println(F("ERROR: prog: delete: <num2> less than <num1>")); return; }
  } else {
    Serial.println(F("ERROR: prog: delete: too many arguments")); return;
  }
  uint16_t len= num2-num1+1;
  for( uint16_t i=num1; i<=num2; i++ ) { ln_del(&ln_store[i]); if( i+len<ln_num ) ln_store[i]= ln_store[i+len]; }
  ln_num-= len;
  Serial.print(F("deleted ")); Serial.print(len); Serial.println(F(" lines"));
}

static void cmdprog_move(int argc, char * argv[]) {
  if( argc<5 ) { Serial.println(F("ERROR: prog: move: expected 3 line numbers")); return; }
  uint16_t num1;
  if( !cmd_parse(argv[2],&num1) ) { Serial.println(F("ERROR: prog: move: expected hex <num1>")); return; }    
  uint16_t num2;
  if( !cmd_parse(argv[3],&num2) ) { Serial.println(F("ERROR: prog: move: expected hex <num2>")); return; }    
  uint16_t num3;
  if( !cmd_parse(argv[4],&num3) ) { Serial.println(F("ERROR: prog: move: expected hex <num3>")); return; }    
  if( num1>=ln_num ) { Serial.println(F("ERROR: prog: move: <num1> does not exist")); return; }
  if( num2>=ln_num ) { Serial.println(F("ERROR: prog: move: <num2> does not exist")); return; }
  if( num1>num2 ) { Serial.println(F("ERROR: prog: move: <num2> must be at least <num1>")); return; }
  if( num3>=ln_num ) { Serial.println(F("ERROR: prog: move: <num3> to high")); return; }
  if( num1<=num3 && num3<=num2 ) { Serial.println(F("ERROR: prog: move: <num3> can not be within <num1>..<num2>")); return; }
  if( num3==num2+1 ) { Serial.println(F("ERROR: prog: move: move to same location ignored")); return; }
  // Report result before num1 and num2 are changed
  Serial.print(F("INFO: prog: move: moved ")); Serial.print(num2+1-num1); Serial.println(F(" lines")); 
  // Actual move
  while( num1<=num2 ) {
    ln_temp= ln_store[num1];
    if( num3<num1 ) { // move before
       for( uint16_t i=num1; i>num3; i--) ln_store[i]= ln_store[i-1];
       ln_store[num3]= ln_temp;
       num1++; num3++;
    } else { // move after
       for( uint16_t i=num1; i<num3; i++) ln_store[i]= ln_store[i+1];
       ln_store[num3-1]= ln_temp;
       num2--;
    }
  }
}

static uint16_t cmdprog_insert_linenum;

static void cmdprog_insert_stream(int argc, char * argv[]) {
  if( argc==0 ) { // no arguments toggles streaming mode
    if( cmd_get_streamfunc()==0 ) cmd_set_streamfunc(cmdprog_insert_stream); else cmd_set_streamfunc(0);
  } else {
    if( ln_num>=LN_NUM ) { Serial.println(F("ERROR: prog: insert: out of line memory")); return; }
    ln_t * ln= ln_parse(argc, argv);
    if( ln!=0 ) {
      for( uint16_t n=ln_num; n>cmdprog_insert_linenum; n-- ) ln_store[n]= ln_store[n-1];
      ln_store[cmdprog_insert_linenum++]= *ln; 
      ln_num++;
    }
  }
  // Update the streaming prompt (will only be shown in streaming mode)
  char buf[8]; snprintf_P(buf,sizeof buf, PSTR("P:%03x> "),cmdprog_insert_linenum); cmd_set_streamprompt(buf);
  
}

static void cmdprog_compile(int argc, char * argv[]) {
  (void)argc;
  (void)argv;
  bool ok=comp_compile();
  if( ok ) comp_list();  
  // todo: list
  // todo: map
  // todo: install
}

// The main command handler
static void cmdprog_main(int argc, char * argv[]) {
  if( argc>1 && cmd_isprefix(PSTR("insert"),argv[1]) ) { 
    if( argc==2 ) { 
      cmdprog_insert_linenum= ln_num;
      argc-=2; argv+=2; // skip 'prog insert addr'
    } else {
      uint16_t linenum;
      if( !cmd_parse(argv[2],&linenum) ) { Serial.println(F("ERROR: prog: insert: expected hex <linenum>")); return; }    
      if( linenum>ln_num ) { Serial.println(F("ERROR: prog: insert: <linenum> too high")); return; }
      cmdprog_insert_linenum= linenum;
      argc-=3; argv+=3; // skip 'prog insert addr'
    }
    cmdprog_insert_stream(argc, argv); 
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("replace"),argv[1]) ) { 
    if( argc<4 ) { Serial.println(F("ERROR: prog: replace: expected <linenum> and <line>")); return; }
    uint16_t linenum;
    if( !cmd_parse(argv[2],&linenum) ) { Serial.println(F("ERROR: prog: replace: expected hex <linenum>")); return; }    
    if( linenum>=ln_num ) { Serial.println(F("ERROR: prog: replace: <linenum> does not exist")); return; }
    ln_t * ln= ln_parse(argc-3, argv+3);
    if( ln!=0 ) ln_store[linenum]= *ln; 
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("list"),argv[1]) ) { 
    cmdprog_list(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("move"),argv[1]) ) { 
    cmdprog_move(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("delete"),argv[1]) ) { 
    cmdprog_delete(argc,argv);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("compile"),argv[1]) ) { 
    cmdprog_compile(argc-2,argv+2);
    return;
  }
  if( argc>1 && cmd_isprefix(PSTR("stat"),argv[1]) ) { 
    Serial.print(F("lines  used ")); Serial.print(ln_num); Serial.print('/'); Serial.println(LN_NUM); 
    Serial.print(F("labels used ")); Serial.print(FS_NUM-1-fs_free()); Serial.print('/'); Serial.println(FS_NUM-1); 
    if( argc>2 && cmd_isprefix(PSTR("strings"),argv[1]) ) fs_dump();
    return;
  }
  Serial.println(F("ERROR: prog: unexpected arguments")); 
}


// Note cmd_register needs all strings to be PROGMEM strings. For longhelp we do that manually
const char cmdprog_longhelp[] PROGMEM = 
  "SYNTAX: prog insert [<linenum> [<line>] ]\n"
  "- inserts <line> to program at position <linenum>\n"
  "- if <line> is absent, starts streaming mode (empty line ends it)\n"
  "- if <linenum> is absent, starts streaming mode at end of program\n"
  "SYNTAX: prog replace <linenum> <line> ]\n"
  "- overwrites the program at position <linenum> with <line>\n"
  "SYNTAX: prog list [<num1> [<num2>]]\n"
  "- lists the program from the line number <num1> to <num2>\n"
  "- if both <num1> and <num2> absent lists whole program"
  "- if <num2> is absent lists only line <num1>\n"
  "- if both present, lists lines <num1> upto <num2>\n"
  "- if both present, they may be '-', meaning 0 for <num1> and last for <num2>\n"
  "SYNTAX: prog move <num1> <num2> <num3>\n"
  "- moves lines <num1> up to and including <num2> to just before <num3>\n"
  "SYNTAX: prog delete <num1> [<num2>]\n"
  "- deletes the program lines from the line number <num1> to <num2>\n"
  "- if <num2> is absent deletes only line <num1>\n"
  "- if both present, deletes lines <num1> upto <num2>\n"
  "- if both present, they may be '-', meaning 0 for <num1> and last for <num2>\n"
  "SYNTAX: prog compile [list | map | install]\n"
  "- compiles the program; giving info\n"
  "- 'list' compiles and produces an instruction listing\n"
  "- 'symbols' compiles and produces a map (label values)\n"
  "- 'install' compiles and writes to memory\n"
  "SYNTAX: prog stat [strings]\n"
  "- shows the memory usage of the program (and optionally the string table)\n"
;


// Note cmd_register needs all strings to be PROGMEM strings. For the short string we do that inline with PSTR.
void cmdprog_register(void) {
  fs_init();
  ln_init();
  cmd_register(cmdprog_main, PSTR("prog"), PSTR("edit and compile a program"), cmdprog_longhelp);
}
