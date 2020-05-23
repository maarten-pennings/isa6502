# isa6502
Arduino library with tables describing 6502 instructions

## Introduction

This library contains several table that store the 6502 instruction set.
See for example [this excellent work](https://www.masswerk.at/6502/6502_instruction_set.html).
The following tables are available:

- addressing mode  
  example: `ABS` (absolute), `ZPG` (zero page)
- instruction  
  example: `LDA` (load accumulator), `JMP` (jump)
- opcode  
  example: 0xA9 (`LDA.IMM`)
  
In order to save RAM, the tables are stored in flash.
On AVR, that is known as PROGMEM.
Note that the AVRs have a Harvard architecture.
This means that every address occurs twice, once as DATAMEM (RAM), once as PROGMEM (flash).
This means that if a pointer like `char * p` has the value 0x1000, it could address location 0x1000 in RAM 
or 0x1000 in flash. See details [below](#progmem-details).

## Examples

There is a simple [example](isa6502/isa6502.ino) that prints all tables.

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
