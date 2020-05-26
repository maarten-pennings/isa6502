# isa6502
Arduino library with tables describing 6502 instructions

## Introduction

This library contains several table that store the 6502 instruction set.
See for example [this excellent work](https://www.masswerk.at/6502/6502_instruction_set.html).
The following tables are available:

- **addressing mode**  
  example: `ABS` (absolute), `ZPG` (zero page)
- **instruction**  
  example: `LDA` (load accumulator), `JMP` (jump)
- **opcode**  
  example: 0xA9 (`LDA.IMM`)
  
In order to save RAM, the tables are stored in flash.
On AVR, that is known as PROGMEM.
Note that the AVRs have a Harvard architecture.
This means that every address occurs twice, once as DATAMEM (RAM), once as PROGMEM (flash).
This means that if a pointer like `char * p` has the value 0x1000, it could address location 0x1000 in RAM 
or 0x1000 in flash. 

Most of this is hidden in the library.
Except for the return values of type `char *`.
If you want to `print` those, embed them in the `f(...)` macro.
If you want to compare then, or get their length, use the `_P` version from the standard library: `strcmp_P` or `strlen_p`.
See the [isa6502dump](#isa6502dump) example.

For details on PROGMEM see [below](#progmem-details).

## Examples

### isa6502dump

There is a simple [example](examples/isa6502dump) that prints all tables.

Note that it shows how to print using `f()`:

```cpp
  Serial.print(  f(isa_addrmode_aname(aix)) ); 
```

and how to compare a string from a table using an `_p()` variant:

```cpp
  char aname[4]; 
  strcpy_P( aname, isa_addrmode_aname(aix) );
```

### isa6502cmd

This is a [complex example](examples/isa6502cmd), 
using my [command interpreter](https://github.com/maarten-pennings/cmd).
It adds a _man page_ [command](examples/isa6502cmd/cmdman.cpp), which allows the user to query the 6502 tables.

### isa6502dasm

This example also uses my [command interpreter](https://github.com/maarten-pennings/cmd).
It not only has a command to write and read to memory, but also to **disassemble**.
Here is an example run.

```txt
Welcome to isa6502dasm lib V4

Type 'help' for help
>> write fffc 00 00 66 66 78 D8 A2 FF 9A A9 00 8D 00 80 A9 FF 8D 00 80 D0 F4 
>> dasm
fffc 00       BRK
fffd 00       BRK
fffe 66 66    ROR *66
0000 78       SEI
0001 d8       CLD
0002 a2 ff    LDX #ff
0004 9a       TXS
0005 a9 00    LDA #00
>> dasm
0007 8d 00 80 STA 8000
000a a9 ff    LDA #ff
000c 8d 00 80 STA 8000
000f d0 f4    BNE +f4 (0005)
0011 00       BRK
0012 00       BRK
0013 00       BRK
0014 00       BRK
>> read fff0
fff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66 66
0000: 78 d8 a2 ff 9a a9 00 8d 00 80 a9 ff 8d 00 80 d0
0010: f4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
>> 
```

It also has a command to **assemble**.
Here is an example run, for the same program as above.

```txt
Welcome to isa6502dasm lib V5

Type 'help' for help
>> w fffc 00 00
>> write fffc 00 00
>> asm 0000
w0000> sei
w0001> cld
w0002> ldx ff
INFO: asm: suggest ZPG instead of ABS (try - for undo)
w0005> -
w0002> ldx #ff
w0004> txs
w0005> lda #00
w0007> sta 8000
w000a> lda #ff
w000c> sta 8000
w000f> bne 0005
w0011>
>> d
0000 78       SEI
0001 d8       CLD
0002 a2 ff    LDX #ff
0004 9a       TXS
0005 a9 00    LDA #00
0007 8d 00 80 STA 8000
000a a9 ff    LDA #ff
000c 8d 00 80 STA 8000
>> d
000f d0 f4    BNE +f4 (0005)
0011 00       BRK
0012 00       BRK
0013 00       BRK
0014 00       BRK
0015 00       BRK
0016 00       BRK
0017 00       BRK
>>
```

Note

 - At address 0002 I made a mistake, I typed `ldx ff` where I intende `ldx #ff`.
 - The assembler gave a warning, because the operand size (1 byte) did not match the instruction (2 bytes).
 - I used the "undo" feature (go one instruction back), and corrected.
 - At address 000f I used absolute addressing syntax for `bne`, but that instruction only has relative addressing. The assembler silently accepts.
 - I quite the streaming mode ar address 0011 by entering a blank line.
 - Then I gave twice the disassemble command (which knows the last written or shown address). This confirms the assembled code is correct
 
## PROGMEM details

Recall that a pointer `char * p` with a value 0x1000 could address location 0x1000 in DATAMEM (RAM) or 0x1000 in PROGMEM (flash). 
Technically the AVR has two different instructions for that, and the C compiler must generate those.
An unqualified `*p` in C is always a RAM location. To read from flash we need a modifier like `pgm_read_byte(p)`. This inserts
an `lpm` instruction (load from program memory).

The run-time library supports this; for many functions like `memcmp(p1,p2)` (where both p1 and p2 are in RAM) there is
a variant `memcmp_P(p1,p2)` (where `p2` is in PROGMEM).

A special case is a string. If a string is in PROGMEM, all its characters have an address in PROGMEM. 
Arduino could have made a `Serial.print_p()`. But `print()` is an overloaded (cpp) method of Serial.
It magically maps to `print(char)`, `print(int)`, or `print(char*)`. However there is no type difference between a `char *` in RAM 
and a `char *` in flash. The trick Arduino used is to make an empty class (`class __FlashStringHelper`), 
and cast the `char *` to that class if that pointer points to flash. With the trick, overloading can be used again.

To store a variable in PROGMEM, add attribute `PROGMEM` (and make sure it is constant and not on the stack - `static`). 
To print it, use the above mentioned `pgm_read_byte()`/`pgm_read_word()` on the _address_ of the integer.

```cpp
static const int i PROGMEM = 5;
Serial.println( (char) pgm_read_byte(&i) );
```

On my machine PROGMEM is defined in `C:\Users\Maarten\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino5\avr\include\avr\pgmspace.h`, roughly (simplified) as `#define  PROGMEM   __attribute__((__progmem__))`.

To put a whole (character) array in PROGMEM, again add `PROGMEM` but also note the `const` and the `static`. 
To print the first character, use the above mentioned `pgm_read_byte()`. 
To print the whole string, use the `f()` macro, so that the correct overloaded version of `print()` is selected by the compiler.

```cpp
static const char s[] PROGMEM = "The content";
Serial.println( (char) pgm_read_byte(s) );
Serial.println( f(s) );
```

For some reason, the `f()` macro must be defined by ourselves.

```cpp
#include <avr/pgmspace.h>
#define f(s) ((__FlashStringHelper *)(s))
```

An alternative to having an array, is to have a pointer (to an array). Note that `s` is in RAM, but the chars are in PROGMEM.

```cpp
const char * s = PSTR("Content");
Serial.println( (char) pgm_read_byte(s) );
Serial.println( f(s) );
```

For simple literals, for example in `print`, there is no need to make an explicit variable:

```cpp
Serial.println( f( PSTR("Example") ) );
```

This occurs so often, that Arduino has made a macro for that: `F()`. Why `F` is publsihed and `f` not is a mystery to me:

```cpp
Serial.println( F("Example") );
```

It gets even harder when a table of strings needs to be mapped to flash. Note that `strings` is an array of three pointers, 
all of which are in PROGMEM. So, `pgm_read_word` is needed to read the pointer. That pointed is dressed up as a `__FlashStringHelper`
so that the right version of `print` is selected. That `print` uses `pgm_read_byte` to read the characters, which are also in
PROGMEM.

```cpp
static const char s0[] PROGMEM = "foo";
static const char s1[] PROGMEM = "bar";
static const char s2[] PROGMEM = "baz";
static const char * const strings[] PROGMEM = { s0, s1, s2 };
Serial.println( f(pgm_read_word(&strings[2])) );
```

Unfortunately, `PSTR` can only be used within a function, so ths **does not work**.

```cpp
static const char * const strings[] PROGMEM = { PSTR("foo"), PSTR("bar"), PSTR("baz") };
```

(end of doc)
