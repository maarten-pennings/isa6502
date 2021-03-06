# isa6502.py - Instruction set architecture of the 6502: generate various tables opcode tables

import sys
import datetime

version=7

# ADDRESSING MODES ####################################################

# AddrMode     is a class whose objects are address mode definitions - what does ABS mean
# aname        is an addressing mode name like "ABS"
# addrmodes    is a global list of AddrMode objects
# aix          is an index into that list
# amode        is an AddrMode object (an thus an element from that list)

class AddrMode : # Defines one addressing mode like ABS
  def __init__(self,aname,bytes,desc, syntax):
    self.aname= aname     # A three letter name for the addressing mode
    self.bytes= bytes     # Number of bytes this addressing mode takes (opcode plus 0, 1, or 2 bytes for operand)
    self.desc= desc       # Human readable description of the addressing mode
    self.syntax= syntax   # Notation in assembly language
    self.aix= None        # Index into the addrmodes[] list, set by post-processing
    
addrmodes= [ # Global variable of all addressing modes
  AddrMode("0Ea", 1, "?error description", "??err??"), # addrmode-0

  AddrMode("IMP", 1, "implied", "OPC"),
  AddrMode("IMM", 2, "immediate", "OPC #NN"),
  AddrMode("ACC", 1, "accumulator", "OPC A"),

  AddrMode("ABS", 3, "absolute", "OPC HHLL"),
  AddrMode("ABX", 3, "absolute, indexed with x", "OPC HHLL,X"),
  AddrMode("ABY", 3, "absolute, indexed with y", "OPC HHLL,Y"),

  AddrMode("ZPG", 2, "zero page", "OPC *LL"),
  AddrMode("ZPX", 2, "zero page, indexed with x", "OPC *LL,X"),
  AddrMode("ZPY", 2, "zero page, indexed with y", "OPC *LL,Y"),

  AddrMode("ZXI", 2, "zero page, indexed with x, indirect", "OPC (LL,X)"),
  AddrMode("ZIY", 2, "zero page, indirect, indexed with y", "OPC (LL),Y"),

  AddrMode("REL", 2, "relative to PC", "OPC +NN"),
  AddrMode("IND", 3, "indirect", "OPC (HHLL)"),
]

# Make sure instructions are sorted
addrmodes.sort(key=lambda amode:amode.aname)
assert(addrmodes[0].aname=="0Ea")
# Post-processing: Set the aix field of all addrmodes 
for aix,am in enumerate(addrmodes) :
  am.aix= aix
  
def addrmode_find_by_name(aname) :
  for am in addrmodes :
    if am.aname==aname :
      return am
  return None

def addrmode_op_subst(am,op) :
  return am.syntax.replace("OPC","").replace(" ","").replace("NN",op).replace("HHLL",op).replace("LL",op)

# INSTRUCTIONS and VARIANTS ###########################################

# Instruction  is a class whose objects are instruction definitions - what is LDA and which addressing modes variants does it have
# iname        is an instruction name, like "LDA"
# instructions is a global list of Instruction objects
# iix          is an index into that list
# ins          is an Instruction object (an thus an element from that list)


class Variant :  
  def __init__(self,amode,opcode,cycles,xcycles):
    self.amode= amode     # The addressing mode name for this instruction variant (eg "ABS")
    self.opcode= opcode   # The opcode for this instruction variant (0..255)
    self.cycles= cycles   # The (minimal) number of cycles to execute this instruction variant (0..)
    self.xcycles= xcycles # The worst case additional number of cycles to execute this instruction variant (0..2)
    self.ins= None        # Back-link to instruction, set by constructor of Instruction

class Instruction : 
  def __init__(self,iname,desc,help,flags,vars):
    self.iname= iname     # The instruction name (e.g. "LDA")
    self.desc= desc       # The description of the instruction
    self.help= help       # The detailed description of the instruction
    self.flags= flags     # The (program status) flags the instruction updates
    self.vars= vars       # The list of instruction variants (e.g. LDA.ABS, LDA.IMM)
    self.iix= None        # Index into the instructions[] list, set by post-processing
    for var in vars: var.ins= self # Add backlink (a variant knows its instruction)

instructions= [
  Instruction("0Ei", "?Error instruction", "?Error help", "?Errflag", # Instruction-0
    [
    ]
  ),
  Instruction("ADC", "add memory to accumulator with carry", "C <- A + M + C", "NVxbdiZC",
    [
      Variant( addrmode_find_by_name("IMM"), 0x69, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x65, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x75, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x6D, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0x7D, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0x79, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0x61, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0x71, 5, 1),
    ]
  ),  
  Instruction("AND", "AND memory with accumulator", "A <- A AND M", "NvxbdiZc",
    [ 
      Variant( addrmode_find_by_name("IMM"), 0x29, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x25, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x35, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x2D, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0x3D, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0x39, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0x21, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0x31, 5, 1),
    ]
  ),
  Instruction("ASL", "arithmetic shift one bit left (memory or accumulator)", "C <- [76543210] <- 0", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("ACC"), 0x0A, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x06, 5, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x16, 6, 0),
      Variant( addrmode_find_by_name("ABS"), 0x0E, 6, 0),
      Variant( addrmode_find_by_name("ABX"), 0x1E, 7, 0),
    ]
  ),
  Instruction("BCC", "branch on carry clear", "branch on C = 0", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0x90, 2, 2),
    ]
  ),
  Instruction("BCS", "branch on carry set", "branch on C = 1", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0xB0, 2, 2),
    ]
  ),
  Instruction("BEQ", "branch on result zero", "branch on Z = 1", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0xF0, 2, 2),
    ]
  ),
  Instruction("BIT", "test bits in memory with accumulator", "N<-M.7; V<-M.6; Z<-A AND M", "NVxbdiZc",
    [
      Variant( addrmode_find_by_name("ZPG"), 0x24, 3, 0),
      Variant( addrmode_find_by_name("ABS"), 0x2C, 4, 0),
    ]
  ),
  Instruction("BMI", "branch on result minus", "branch on N = 1", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0x30, 2, 2),
    ]
  ),
  Instruction("BNE", "branch on result not zero", "branch on Z = 0", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0xD0, 2, 2),
    ]
  ),
  Instruction("BPL", "branch on result plus", "branch on N = 0", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0x10, 2, 2),
    ]
  ),
  Instruction("BRK", "force break", "interrupt; push PC+2; push PSR", "nvxBdIzc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x00, 7, 0),
    ]
  ),
  Instruction("BVC", "branch on overflow clear", "branch on V = 0", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0x50, 2, 2),
    ]
  ),
  Instruction("BVS", "branch on overflow set", "branch on V = 1", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("REL"), 0x70, 2, 2),
    ]
  ),
  Instruction("CLC", "clear carry flag", "C <- 0", "nvxbdizC",
    [
      Variant( addrmode_find_by_name("IMP"), 0x18, 2, 0),
    ]
  ),
  Instruction("CLD", "clear decimal flag", "D <- 0", "nvxbDizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xD8, 2, 0),
    ]
  ),
  Instruction("CLI", "clear interrupt disable flag", "I <- 0 (enabled)", "nvxbdIzc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x58, 2, 0),
    ]
  ),
  Instruction("CLV", "clear overflow flag", "V <- 0", "nVxbdizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xB8, 2, 0),
    ]
  ),
  Instruction("CMP", "compare memory with accumulator", "A - M", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("IMM"), 0xC9, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xC5, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0xD5, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0xCD, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0xDD, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0xD9, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0xC1, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0xD1, 5, 1),
    ]
  ),
  Instruction("CPX", "compare memory and index X", "X - M", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("IMM"), 0xE0, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xE4, 3, 0),
      Variant( addrmode_find_by_name("ABS"), 0xEC, 4, 0),
    ]
  ),
  Instruction("CPY", "compare memory and index Y", "Y - M", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("IMM"), 0xC0, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xC4, 3, 0),
      Variant( addrmode_find_by_name("ABS"), 0xCC, 4, 0),
    ]
  ),
  Instruction("DEC", "decrement memory by one", "M <- M - 1", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("ZPG"), 0xC6, 5, 0),
      Variant( addrmode_find_by_name("ZPX"), 0xD6, 6, 0),
      Variant( addrmode_find_by_name("ABS"), 0xCE, 6, 0),
      Variant( addrmode_find_by_name("ABX"), 0xDE, 7, 0),
    ]
  ),
  Instruction("DEX", "decrement index X by one", "X <- X - 1", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xCA, 2, 0),
    ]
  ),
  Instruction("DEY", "decrement index Y by one", "Y <- Y - 1", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x88, 2, 0),
    ]
  ),
  Instruction("EOR", "EOR (exclusive-or) memory with accumulator", "A <- A EOR M", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMM"), 0x49, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x45, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x55, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x4D, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0x5D, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0x59, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0x41, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0x51, 5, 1),
    ]
  ),
  Instruction("INC", "increment memory by one", "M <- M + 1", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("ZPG"), 0xE6, 5, 0),
      Variant( addrmode_find_by_name("ZPX"), 0xF6, 6, 0),
      Variant( addrmode_find_by_name("ABS"), 0xEE, 6, 0),
      Variant( addrmode_find_by_name("ABX"), 0xFE, 7, 0),
    ]
  ),
  Instruction("INX", "increment index X by one", "X <- X + 1", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xE8, 2, 0),
    ]
  ),
  Instruction("INY", "increment index Y by one", "Y <- Y + 1", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xC8, 2, 0),
    ]
  ),
  Instruction("JMP", "jump to new location", "PCL <- (PC+1); PCH <- (PC+2)", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("ABS"), 0x4C, 3, 0),
      Variant( addrmode_find_by_name("IND"), 0x6C, 5, 0),
    ]
  ),
  Instruction("JSR", "jump to new location saving return address", "push (PC+2); PCL <- (PC+1); PCH <- (PC+2)", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("ABS"), 0x20, 6, 0),
    ]
  ),
  Instruction("LDA", "load accumulator with memory", "A <- M", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMM"), 0xA9, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xA5, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0xB5, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0xAD, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0xBD, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0xB9, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0xA1, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0xB1, 5, 1),
    ]
  ),
  Instruction("LDX", "load index X with memory", "X <- M", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMM"), 0xA2, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xA6, 3, 0),
      Variant( addrmode_find_by_name("ZPY"), 0xB6, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0xAE, 4, 0),
      Variant( addrmode_find_by_name("ABY"), 0xBE, 4, 1),
    ]
  ),
  Instruction("LDY", "load index Y with memory", "Y <- M", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMM"), 0xA0, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xA4, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0xB4, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0xAC, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0xBC, 4, 1),
    ]
  ),
  Instruction("LSR", "logic shift one bit right (memory or accumulator)", "0 -> [76543210] -> C", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("ACC"), 0x4A, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x46, 5, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x56, 6, 0),
      Variant( addrmode_find_by_name("ABS"), 0x4E, 6, 0),
      Variant( addrmode_find_by_name("ABX"), 0x5E, 7, 0),
    ]
  ),
  Instruction("NOP", "no operation", "skip", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xEA, 2, 0),
    ]
  ),
  Instruction("ORA", "OR memory with accumulator", "A <- A OR M", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMM"), 0x09, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x05, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x15, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x0D, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0x1D, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0x19, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0x01, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0x11, 5, 1),
    ]
  ),
  Instruction("PHA", "push accumulator on stack", "push A", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x48, 3, 0),
    ]
  ),
  Instruction("PHP", "push processor status register on stack", "push PSR", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x08, 3, 0),
    ]
  ),
  Instruction("PLA", "pull accumulator from stack", "pull A", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x68, 4, 0),
    ]
  ),
  Instruction("PLP", "pull processor status register from stack", "pull PSR", "NVxbDIZC",
    [
      Variant( addrmode_find_by_name("IMP"), 0x28, 4, 0),
    ]
  ),
  Instruction("ROL", "rotate one bit left (memory or accumulator)", "C <- [76543210] <- C", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("ACC"), 0x2A, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x26, 5, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x36, 6, 0),
      Variant( addrmode_find_by_name("ABS"), 0x2E, 6, 0),
      Variant( addrmode_find_by_name("ABX"), 0x3E, 7, 0),
    ]
  ),
  Instruction("ROR", "rotate one bit right (memory or accumulator)", "C -> [76543210] -> C", "NvxbdiZC",
    [
      Variant( addrmode_find_by_name("ACC"), 0x6A, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0x66, 5, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x76, 6, 0),
      Variant( addrmode_find_by_name("ABS"), 0x6E, 6, 0),
      Variant( addrmode_find_by_name("ABX"), 0x7E, 7, 0),
    ]
  ),
  Instruction("RTI", "return from interrupt", "pull PSR; pull PCL; pull PCH", "NVxbDIZC",
    [
      Variant( addrmode_find_by_name("IMP"), 0x40, 6, 0),
    ]
  ),
  Instruction("RTS", "return from subroutine", "pull PCL; pull PCH; PC <- PC+1", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x60, 6, 0),
    ]
  ),
  Instruction("SBC", "subtract memory from accumulator with borrow", "A <- A - M - C", "NVxbdiZC",
    [
      Variant( addrmode_find_by_name("IMM"), 0xE9, 2, 0),
      Variant( addrmode_find_by_name("ZPG"), 0xE5, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0xF5, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0xED, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0xFD, 4, 1),
      Variant( addrmode_find_by_name("ABY"), 0xF9, 4, 1),
      Variant( addrmode_find_by_name("ZXI"), 0xE1, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0xF1, 5, 1),
    ]
  ),
  Instruction("SEC", "set carry flag", "C <- 1", "nvxbdizC",
    [
      Variant( addrmode_find_by_name("IMP"), 0x38, 2, 0),
    ]
  ),
  Instruction("SED", "set decimal flag", "D <- 1", "nvxbDizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xF8, 2, 0),
    ]
  ),
  Instruction("SEI", "set interrupt disable flag", "I <- 1 (disabled)", "nvxbdIzc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x78, 2, 0),
    ]
  ),
  Instruction("STA", "store accumulator in memory", "M <- A", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("ZPG"), 0x85, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x95, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x8D, 4, 0),
      Variant( addrmode_find_by_name("ABX"), 0x9D, 5, 0),
      Variant( addrmode_find_by_name("ABY"), 0x99, 5, 0),
      Variant( addrmode_find_by_name("ZXI"), 0x81, 6, 0),
      Variant( addrmode_find_by_name("ZIY"), 0x91, 6, 0),
    ]
  ),
  Instruction("STX", "store index X in memory", "M <- X", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("ZPG"), 0x86, 3, 0),
      Variant( addrmode_find_by_name("ZPY"), 0x96, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x8E, 4, 0),
    ]
  ),
  Instruction("STY", "store index Y in memory", "M <- Y", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("ZPG"), 0x84, 3, 0),
      Variant( addrmode_find_by_name("ZPX"), 0x94, 4, 0),
      Variant( addrmode_find_by_name("ABS"), 0x8C, 4, 0),
    ]
  ),
  Instruction("TAX", "transfer accumulator to index X", "X <- A", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xAA, 2, 0),
    ]
  ),
  Instruction("TAY", "transfer accumulator to index Y", "Y <- A", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xA8, 2, 0),
    ]
  ),
  Instruction("TSX", "transfer stack pointer to index X", "X <- SP", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0xBA, 2, 0),
    ]
  ),
  Instruction("TXA", "transfer index X to accumulator", "A <- X", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x8A, 2, 0),
    ]
  ),
  Instruction("TXS", "transfer index X to stack register", "SP <- X", "nvxbdizc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x9A, 2, 0),
    ]
  ),
  Instruction("TYA", "transfer index Y to accumulator", "A <- Y", "NvxbdiZc",
    [
      Variant( addrmode_find_by_name("IMP"), 0x98, 2, 0),
    ]
  ),
]

# Make sure instructions are sorted
instructions.sort(key=lambda ins:ins.iname)
assert(instructions[0].iname=="0Ei")
# Post-processing: set the iix field of all instructions 
for iix,ins in enumerate(instructions) :
  ins.iix= iix
# check that there are no duplicate names in instructions and addressingmodes
for ins in instructions :
  for amode in addrmodes :
    if ins.iname==amode.aname : print("ERROR: duplicate name ", ins.iname)

def variant_find_by_name(instruction,aname) :
  for var in instruction.vars :
    if var.amode.aname==aname : 
      return var
  return None

# Print addressing modes ##############################################

def print_amodes() :
  print("Addressing modes (amodes)")
  print( f"+{'-'*3}+{'-'*5}+{'-'*35}+{'-'*10}+{'-'*3}+" )
  print( f"|{'aix':3}|{'aname':4}|{'description':35}|{'syntax':10}|{'len':3}|" )
  print( f"+{'-'*3}+{'-'*5}+{'-'*35}+{'-'*10}+{'-'*3}+" )
  for ams in addrmodes[1:] : # skip addrmode-0
    print( f"|{ams.aix:3}| {ams.aname} |{ams.desc:35}|{ams.syntax:10}|{ams.bytes:3}|" )
  print( f"+{'-'*3}+{'-'*5}+{'-'*35}+{'-'*10}+{'-'*3}+" )
  
# Print instructions ##################################################

def print_insts():
  datasheet_order= ["IMM", "ABS", "ZPG", "ACC", "IMP", "INX", "INY", "ZPX", "ABX", "ABY", "REL", "IND", "ZPY"]
  print("Instructions (insts)")
  print("+---+-----+" + "------+"*len(datasheet_order) )
  print("|iix|iname", end="|")
  for aname in datasheet_order :
    am= addrmode_find_by_name(aname)
    print( f"{am.aname}  {am.bytes}", end="|" )
  print()
  for ix,ins in enumerate(instructions[1:]) : # skip instruction-0
    if ix%8==0: print("+---+-----+" + "------+"*len(datasheet_order) )
    print( f"|{ins.iix:3}| {ins.iname} ", end="|")
    for aname in datasheet_order :
      var= variant_find_by_name(ins,aname)
      if var:
        print( f"{var.opcode:02X} {var.cycles}+{var.xcycles}", end="|")
      else :
        print( f"      ", end="|")
    print()
  print("+---+-----+" + "------+"*len(datasheet_order) )

# Print opcode matrix #################################################

def print_opcodes() :
  # Generate matrix of opcodes
  opcodes=["       "]*256
  for ins in instructions :
    for var in ins.vars :
      opcodes[var.opcode]= ins.iname+"/"+var.amode.aname
  # Print the matrix
  print("16x16 Opcode matrix (opcodes)")
  print("+--" + "+-------"*16 + "+" )
  print("|  ", end="|")
  for i in range(16) : print( f"  0{i:X}   ", end="|" )
  print()
  for opcode , iname_aname in enumerate(opcodes):
    if opcode%(4*16)==0 : print("+--" + "+-------"*16 + "+" )
    if opcode%16==0 : print( f"|{opcode:02X}", end="|")
    print( f"{iname_aname}", end="|")
    if opcode%16==15: print()
  print("+--" + "+-------"*16 + "+" )

# Print C data structures #############################################

def print_cpp_header() :
  print("// isa.cpp - 6502 instruction set architecture")
  print(f"// This file is generated by {sys.argv[0]} V{version} on", datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") )
  print()
  print()
  print("#include <Arduino.h>")
  print("#include <avr/pgmspace.h>")
  print("#include <ctype.h>")
  print("#include <string.h>")
  print("#include \"isa.h\"")
  print()
  print()
  print("// Reading from flash (PROGMEM) needs special instructions")
  print("// https://www.nongnu.org/avr-libc/user-manual/group__avr__pgmspace.html")
  print("// Recall that F(xxx) puts literal xxx in PROGMEM and makes it printable.")
  print("// The macro f(xxx) assumes pointer xxx is already in PROGMEM but it makes it printable.")
  print("#ifndef f")
  print("#define f(s) ((__FlashStringHelper *)(s))")
  print("#endif")
  print("// Recall pgm_read_word(addr) is needed to read a word (pointer) from address addr in progmem")
  print("// Recall pgm_read_byte(addr) is needed to read a byte (uint8/char) from address addr in progmem")
  print()
  print()

def print_cpp_addrmodes() :
  print("// ADDRMODES #####################################################")
  print()
  print()
  print("// All addressing mode strings are mapped to PROGRMEM")
  for am in addrmodes : # Also addrmode-0 (to trap errors in c)
    print( f"const char ISA_AIX_{am.aname}_aname [] /*{am.aix:2}*/ PROGMEM = \"{am.aname}\";")
    print( f"const char ISA_AIX_{am.aname}_desc  [] /*{am.aix:2}*/ PROGMEM = \"{am.desc}\";")
    print( f"const char ISA_AIX_{am.aname}_syntax[] /*{am.aix:2}*/ PROGMEM = \"{am.syntax}\";")
  print()
  print("// This structure stores the data for one addressing mode definition");
  print("// (since it will be mapped to PROGMEM, all fields are const)");
  print("typedef struct isa_addrmode_s {")  
  print("  const char * const aname;")
  print("  uint8_t      const bytes;")
  print("  const char * const desc;")
  print("  const char * const syntax;")
  print("} isa_addrmode_t;")
  print()
  print("// The table storing the attributes of all addressing modes (in PROGMEM)");
  print("const isa_addrmode_t isa_addrmodes[] PROGMEM = {")
  for am in addrmodes :
    print( f"  /*{am.aix:2}*/ {{ ISA_AIX_{am.aname}_aname, {am.bytes}, ISA_AIX_{am.aname}_desc, ISA_AIX_{am.aname}_syntax }},")
  print("};")
  print()
  print("const char * isa_addrmode_aname ( int aix ) { return (const char *)pgm_read_word(&isa_addrmodes[aix].aname ); }")
  print("uint8_t      isa_addrmode_bytes ( int aix ) { return (uint8_t)pgm_read_byte(&isa_addrmodes[aix].bytes ); }")
  print("const char * isa_addrmode_desc  ( int aix ) { return (const char *)pgm_read_word(&isa_addrmodes[aix].desc  ); }")
  print("const char * isa_addrmode_syntax( int aix ) { return (const char *)pgm_read_word(&isa_addrmodes[aix].syntax); }")
  print()
  print("// Prints the addressing mode name (eg ABS) associated with `aix` to `str`. ")
  print("// This function writes at most `size` bytes to `str`.")
  print("// If the name is shorter than `minlen`, spaces will be appended until name plus spaces equals `minlen`.")
  print("// When size>0, a terminating zero will be added;")
  print("// even if the name is longer than `size` (the operand string will be truncated).")
  print("// Returns the number of characters that would be written if size were high enough (excluding the terminating 0).")
  print("// In other words, when returnvalue<size the complete name (with terminating 0) is written.")
  print("#define _s(p) (uint8_t)pgm_read_byte(p)")
  print("int isa_snprint_aname(char * str, int size, int minlen, int aix) {")
  print("  const char * name= isa_addrmode_aname(aix);")
  print("  int len=0; // number of chars printed to `str`")
  print("  // Copy name")
  print("  char c;")
  print("  while( c=_s(name), c!=0 ) { len++; name++; if(size>0) {size--; *str++=(size==0)?'\\0':c; } } ")
  print("  // Write padding")
  print("  while( len<minlen ) {")
  print("    if( size>0 ) { size--; *str++= (size==0)?'\\0':' ';  }")
  print("    len++;")
  print("  }  ")
  print("  // Write terminating 0")
  print("  if( size>0 ) { size--; *str++='\\0'; }")
  print("  // len++; // snprintf doesn't count the terminating 0")
  print("  // Returns wish length")
  print("  return len;")
  print("}")
  print() 
  print("int isa_addrmode_find(const char * aname) {")
  print("  if( aname==0 ) return 0;")
  print("  int lo= ISA_AIX_FIRST;")
  print("  int hi= ISA_AIX_LAST-1;")
  print("  while( lo<=hi ) {")
  print("    int mid= (lo+hi)/2;")
  print("    int cmp= strcasecmp_P(aname,isa_addrmode_aname(mid));")
  print("    if( cmp==0 ) return mid;")
  print("    if( cmp>0 ) lo=mid+1; else hi=mid-1;")
  print("  }")
  print("  return 0; // (0 is not a used addrmode index)")
  print("}")
  print()
  print("// Prints the 'operand-string' to `str`. ")
  print("// The operand-string is the operand `op` formated according to addressing mode `aix`.")
  print("// For example if `op` is '$1234', and `iax` is ISA_AIX_IND (indirect addressing), ")
  print("// the operand-string '($1234)' will be printed to `str`.")
  print("// If `op` is 0 a default string will be supplied (HHLL, LL, NN) depending on addressing mode")
  print("//")
  print("// This function writes at most `size` bytes to `str`.")
  print("// When size>0, a terminating zero will be added;")
  print("// even if the operand-string is longer than `size` (the operand string will be truncated).")
  print("// Returns the number of characters that would be written if size were high enough (excluding the terminating 0).")
  print("// In other words, when returnvalue<size the complete `fsx`-string (with padding _and_ terminating 0) is written.")
  print("#define _s(p) (uint8_t)pgm_read_byte(p)")
  print("int isa_snprint_op(char * str, int size, int aix, const char * op) {")
  print("  const char * fmt= isa_addrmode_syntax(aix);")
  print("  int len=0; // number of chars printed to `str`")
  print("  char c; // temp variable holding `*fmt`, we use this because fmt is a pointer to PROGMEM. Hence the _s() macro")
  print("  // Part 1: skip OPC (do _not_ copy from fmt to str)")
  print("  while( c=_s(fmt), c=='O'||c=='P'||c=='C'||c==' '  ) fmt++;")
  print("  // Part 2: write prefix of the operand (do copy from fmt to buf)")
  print("  while( c=_s(fmt), c!='H'&&c!='L'&&c!='N'&&c!=0 ) { len++; fmt++; if(size>0) {size--; *str++=(size==0)?'\\0':c; } } // copy fmt")
  print("  // Part 3: write op (replace HLN chars by op, unless op==0)")
  print("  int HLN= c!=0;")
  print("  if( op ) {")
  print("    while( c=_s(fmt), c=='H'||c=='L'||c=='N' ) fmt++; // skip HLN")
  print("    if( HLN ) while( *op!=0 ) { len++; c=*op++; if(size>0) {size--; *str++=(size==0)?'\\0':c; } } // copy op (if there was HLN)")
  print("  } else {")
  print("    while( c=_s(fmt), c=='H'||c=='L'||c=='N' ) { len++; fmt++; if(size>0) {size--; *str++=(size==0)?'\\0':c; } }  // copy fmt")
  print("  }")
  print("  // Part 4: write the postfix (do copy from fmt to buf)")
  print("  while( c=_s(fmt), c!=0 ) { len++; fmt++; if(size>0) {size--; *str++=(size==0)?'\\0':c; } } // copy fmt")
  print("  // Part 5: write terminating 0")
  print("  if( size>0 ) { size--; *str++='\\0'; }")
  print("  // len++; // snprintf doesn't count the terminating 0")
  print("  // Returns wish length")
  print("  return len;")
  print("}")
  print()
  print("// Returns the aix matching the addressing mode syntax of operand `ops`.")
  print("// If no addressing mode syntax matches 0 is returned.")
  print("// If there is a match, `ops` is changed to only contain the operand.")
  print("// \"($3,X)\" -> \"$3\"")
  print("// \"(VEC8)\" -> \"VEC8\"")
  print("// \"#>AAP\" -> \">AAP\"")
  print("int isa_parse(char * ops) {")
  print("  char c;")
  print("")
  print("  char * str0= ops;")
  print("  // No OPC here")
  print("  char * pre0= str0;")
  print("  // Skip all pre chars")
  print("  char * op0= pre0;")
  print("  while( c=*op0, c=='#'||c=='('||c=='+'||c=='*' ) op0++;")
  print("  if( op0==pre0 && (*op0=='A'||*op0=='a') && *(op0+1)==0 ) op0++; // special case ACC addressing")
  print("  int pre0len= op0-pre0;")
  print("  // Skip all operand chars")
  print("  char * post0= op0;")
  print("  while( c=*post0, c!=')'&&c!=','&&c!=0 ) post0++;")
  print("  int op0len= post0-op0;")
  print("  // Search for end of string")
  print("  char * end0= post0;")
  print("  while( c=*end0, c!=0 ) end0++;")
  print("  int post0len= end0-post0;")
  print("  // debug")
  print("  //Serial.println(\"0123456789\");")
  print("  //Serial.println(str0);")
  print("  //Serial.print(\"pre0=\"    ); Serial.print(pre0-str0); Serial.print(\", \");")
  print("  //Serial.print(\"op0=\"     ); Serial.print(op0-str0 ); Serial.print(\", \");")
  print("  //Serial.print(\"post0=\"   ); Serial.print(post0-str0); Serial.print(\", \");")
  print("  //Serial.print(\"end0=\"    ); Serial.print(end0-str0 ); Serial.print(\", \");")
  print("  //Serial.print(\"pre0len=\" ); Serial.print(pre0len); Serial.print(\", \");")
  print("  //Serial.print(\"op0len=\"  ); Serial.print(op0len); Serial.print(\", \");")
  print("  //Serial.print(\"post0len=\"); Serial.print(post0len); Serial.println();")
  print("  // Search")
  print("  for( int aix=ISA_AIX_FIRST; aix<ISA_AIX_LAST; aix++)  {")
  print("    const char * str1= isa_addrmode_syntax(aix); // in PROGMEM!")
  print("    // Strip OPC, to find the start of pre-string")
  print("    const char * pre1= str1;")
  print("    while( c=_s(pre1), c=='O'||c=='P'||c=='C'||c==' ' ) pre1++;")
  print("    // Skip all pre chars")
  print("    const char * op1= pre1;")
  print("    while( c=_s(op1), c=='#'||c=='('||c=='+'||c=='*' ) op1++;")
  print("    if( op1==pre1 && _s(op1)=='A' && _s(op1+1)==0 ) op1++; // special case ACC addressing")
  print("    int pre1len= op1-pre1;")
  print("    // Skip all operand chars")
  print("    const char * post1= op1;")
  print("    while( c=_s(post1), c!=')'&&c!=','&&c!=0 ) post1++;")
  print("    int op1len= post1-op1;")
  print("    // Search for end of string")
  print("    const char * end1= post1;")
  print("    while( c=_s(end1), c!=0 ) end1++;")
  print("    int post1len= end1-post1;")
  print("    // debug")
  print("    //Serial.println(\"0123456789\");")
  print("    //Serial.println(f(str1));")
  print("    //Serial.print(\"pre1=\"    ); Serial.print(pre1-str1); Serial.print(\", \");")
  print("    //Serial.print(\"op1=\"     ); Serial.print(op1-str1 ); Serial.print(\", \");")
  print("    //Serial.print(\"post1=\"   ); Serial.print(post1-str1); Serial.print(\", \");")
  print("    //Serial.print(\"end1=\"    ); Serial.print(end1-str1 ); Serial.print(\", \");")
  print("    //Serial.print(\"pre1len=\" ); Serial.print(pre1len); Serial.print(\", \");")
  print("    //Serial.print(\"op1len=\"  ); Serial.print(op1len); Serial.print(\", \");")
  print("    //Serial.print(\"post1len=\"); Serial.print(post1len); Serial.println();")
  print("    // Do thestrings match?")
  print("    int prematch= pre0len==pre1len && strncasecmp_P(pre0,pre1,pre0len)==0;")
  print("    int opmatch= (op0len>0) == (op1len>0);")
  print("    int postmatch= post0len==post1len && strncasecmp_P(post0,post1,post0len)==0;")
  print("    if( prematch && opmatch && postmatch ) {")
  print("      memmove(ops,op0,op0len);")
  print("      ops[op0len]=0;")
  print("      return aix;")
  print("    }")
  print("  }")
  print("  return 0; // The error aix")
  print("}")
  print()
  print()


def print_cpp_instructions() :
  print("// INSTRUCTIONS ##################################################")
  print()
  print()
  print("// All instruction strings are mapped to PROGMEM")
  for ins in instructions :
    print( f"const char ISA_IIX_{ins.iname}_iname [] /*{ins.iix:2}*/ PROGMEM = \"{ins.iname}\";")
    print( f"const char ISA_IIX_{ins.iname}_desc  [] /*{ins.iix:2}*/ PROGMEM = \"{ins.desc}\";")
    print( f"const char ISA_IIX_{ins.iname}_help  [] /*{ins.iix:2}*/ PROGMEM = \"{ins.help}\";")
    print( f"const char ISA_IIX_{ins.iname}_flags [] /*{ins.iix:2}*/ PROGMEM = \"{ins.flags}\";")
  print()
  print("// This structure stores the data for one instruction (e.g. the LDA)");
  print("// (since it will be mapped to PROGMEM, all fields are const)");
  print("typedef struct isa_instruction_s {")  
  print("  const char * const iname;")
  print("  const char * const desc;")
  print("  const char * const help;")
  print("  const char * const flags;")
  print(f"  const uint8_t opcodes[{len(addrmodes)}]; // for each addrmode, the opcode")
  print("} isa_instruction_t;")
  print()
  print("// Opcode 0xBB is not in use in the 6502. We use it in instructions.opcodes to signal the addrmode does not exist for that instruction");
  print("#define ISA_OPCODE_INVALID 0xBB")
  print()
  print("// The table storing all attributes of instructions (in PROGMEM)");
  print("const isa_instruction_t isa_instructions[] PROGMEM = {")
  for ins in instructions :
    opcodes = [0xBB] * (len(addrmodes)) 
    for var in ins.vars :
      opcodes[var.amode.aix]= var.opcode 
    opcodes_s = "{" + ",".join([ f"0x{o:02x}" for o in opcodes ]) + "}"
    print( f"  /*{ins.iix:2}*/ {{ ISA_IIX_{ins.iname}_iname, ISA_IIX_{ins.iname}_desc, ISA_IIX_{ins.iname}_help, ISA_IIX_{ins.iname}_flags, {opcodes_s} }},")
  print("};")
  print()
  print("const char *  isa_instruction_iname  ( int iix )          { return (const char *)pgm_read_word(&isa_instructions[iix].iname ); }")
  print("const char *  isa_instruction_desc   ( int iix )          { return (const char *)pgm_read_word(&isa_instructions[iix].desc  ); }")
  print("const char *  isa_instruction_help   ( int iix )          { return (const char *)pgm_read_word(&isa_instructions[iix].help  ); }")
  print("const char *  isa_instruction_flags  ( int iix )          { return (const char *)pgm_read_word(&isa_instructions[iix].flags ); }")
  print("uint8_t       isa_instruction_opcodes( int iix, int aix ) { return (uint8_t     )pgm_read_byte(&isa_instructions[iix].opcodes[aix] ); }")
  print()
  print("// Prints the addressing mode name (eg ABS) associated with `aix` to `str`. ")
  print("// This function writes at most `size` bytes to `str`.")
  print("// If the name is shorter than `minlen`, spaces will be appended until name plus spaces equals `minlen`.")
  print("// When size>0, a terminating zero will be added;")
  print("// even if the name is longer than `size` (the operand string will be truncated).")
  print("// Returns the number of characters that would be written if size were high enough (excluding the terminating 0).")
  print("// In other words, when returnvalue<size the complete name (with terminating 0) is written.")
  print("#define _s(p) (uint8_t)pgm_read_byte(p)")
  print("int isa_snprint_iname(char * str, int size, int minlen, int iix) {")
  print("  const char * name= isa_instruction_iname(iix);")
  print("  int len=0; // number of chars printed to `str`")
  print("  // Copy name")
  print("  char c;")
  print("  while( c=_s(name), c!=0 ) { len++; name++; if(size>0) {size--; *str++=(size==0)?'\\0':c; } } ")
  print("  // Write padding")
  print("  while( len<minlen ) {")
  print("    if( size>0 ) { size--; *str++= (size==0)?'\\0':' ';  }")
  print("    len++;")
  print("  }  ")
  print("  // Write terminating 0")
  print("  if( size>0 ) { size--; *str++='\\0'; }")
  print("  // len++; // snprintf doesn't count the terminating 0")
  print("  // Returns wish length")
  print("  return len;")
  print("}")
  print()
  print("int isa_instruction_find(const char * iname) {")
  print("  if( iname==0 ) return 0;")
  print("  int lo= ISA_IIX_FIRST;")
  print("  int hi= ISA_IIX_LAST-1;")
  print("  while( lo<=hi ) {")
  print("    int mid= (lo+hi)/2;")
  print("    int cmp= strcasecmp_P(iname,isa_instruction_iname(mid));")
  print("    if( cmp==0 ) return mid;")
  print("    if( cmp>0 ) lo=mid+1; else hi=mid-1;")
  print("  }")
  print("  return 0; // (0 is not a used instruction index)")
  print("}")
  print()
  print()


def print_cpp_opcodes() :
  print("// OPCODES #####################################################")
  print()
  print()
  vars= [None]*256
  for ins in instructions :
    for var in ins.vars :
      vars[var.opcode]= var
  print("// This structure stores the data for one opcode (e.g. 0xAD or LDA.ABS)");
  print("// (since it will be mapped to PROGMEM, all fields are const)");
  print("typedef struct isa_opcode_s {")  
  print("  uint8_t      const iix; // index into isa_instructions[]")
  print("  uint8_t      const aix; // index into isa_addrmodes[]")
  print("  uint8_t      const cycles;")
  print("  uint8_t      const xcycles;")
  print("} opcode_t;")
  print()
  print("// The table storing all attributes of opcodes (in PROGMEM)");
  print("const opcode_t isa_opcodes[] PROGMEM = {")
  for ix,var in enumerate(vars) :
    if var :
      print( f"  /*{ix:02x}*/ {{ ISA_IIX_{var.ins.iname}, ISA_AIX_{var.amode.aname}, {var.cycles}, {var.xcycles} }},")
    else :
      print( f"  /*{ix:02x}*/ {{ 0, 0, 0, 0 }},")
  print("};")
  print()
  print("uint8_t isa_opcode_iix    ( uint8_t opcode ) { return (uint8_t)pgm_read_byte(&isa_opcodes[opcode].iix    ); }")
  print("uint8_t isa_opcode_aix    ( uint8_t opcode ) { return (uint8_t)pgm_read_byte(&isa_opcodes[opcode].aix    ); }")
  print("uint8_t isa_opcode_cycles ( uint8_t opcode ) { return (uint8_t)pgm_read_byte(&isa_opcodes[opcode].cycles ); }")
  print("uint8_t isa_opcode_xcycles( uint8_t opcode ) { return (uint8_t)pgm_read_byte(&isa_opcodes[opcode].xcycles); }")
  print()
  print()
  
def print_cpp_footer() :
  pass

def print_cpp() :
  print_cpp_header()
  print_cpp_addrmodes()
  print_cpp_instructions()
  print_cpp_opcodes()
  print_cpp_footer()

# Print H file content ################################################

def print_h_header() :
  print("// isa.h - 6502 instruction set architecture")
  print(f"// This file is generated by {sys.argv[0]} V{version} on", datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") )
  print("#ifndef __ISA_H__")
  print("#define __ISA_H__")
  print()
  print()
  print("#include \"stdint.h\"")
  print()
  print()
  print(f"#define ISA_VERSION {version}")
  print()
  print()
  print("// WARNING")
  print("// The data structure describing the addressing modes, instructions, and opcodes are stored in PROGMEM.")
  print("// This means you need special (code memory) instructions to read them.")
  print("// The access methods in this module take care of this for a part.")
  print("// However, when the returned value is a string (a pointer to characters), the characters are also in PROGMEM.")
  print("// Use these pointers with the f() macro, the %S print format (capital S!), or the xxx_P() functions. ")
  print("//   Serial.print( f(isa_addrmode_aname(4)) );")
  print("//   sprintf(buf, \"out[%S]\",isa_addrmode_aname(4))")
  print("//   if( strcmp_P(\"ABS\",isa_addrmode_aname(4))==0 ) {}")
  print()
  print()
  print("// Recall that F(xxx) puts literal xxx in PROGMEM _and_ makes it printable.")
  print("// It uses PSTR() to map it to progmem, and f() to make it printable. Basically F(x) is f(PSTR(x))")
  print("// For some reason, Arduino defines PSTR() and F(), but not f().")
  print("// The macro f(xxx) assumes xxx is already a pointer to chars in PROGMEM' it only makes it printable.")
  print("#ifndef f")
  print("#define f(s) ((__FlashStringHelper *)(s))")
  print("#endif")
  print()
  print()

def print_h_addrmodes() :
  print(" // AddrMode ===========================================================")
  print(" // Objects containing an address mode definitions - what does ABS mean")
  print(" // The definitions are available through indexes.")
  print()
  print("// The enumeration of all addressing modes");
  for am in addrmodes :
    print( f"#define ISA_AIX_{am.aname}   {am.aix:2} // {am.syntax}{' // trap uninitialized variables' if am.aix==0 else ''}")
  print( f"#define ISA_AIX_FIRST  1 // for iteration: first")
  print( f"#define ISA_AIX_LAST  {len(addrmodes)} // for iteration: (one after) last")
  print()
  print("/*PROGMEM*/ const char * isa_addrmode_aname ( int aix );              // A three letter name for the addressing mode (e.g. ABS)")
  print("            uint8_t      isa_addrmode_bytes ( int aix );              // Number of bytes this addressing mode takes (opcode plus 0, 1, or 2 bytes for operand)")
  print("/*PROGMEM*/ const char * isa_addrmode_desc  ( int aix );              // Human readable description of the addressing mode")
  print("/*PROGMEM*/ const char * isa_addrmode_syntax( int aix );              // Notation in assembly language")
  print("            int          isa_addrmode_find  (const char * aname);     // Returns aix for `aname` or 0 if not found")
  print("            int          isa_snprint_aname  (char * str, int size, int minlen, int aix); // Prints the addressing mode name (eg ABS) associated with `aix` to `str`.")
  print("            int          isa_snprint_op     (char * str, int size, int aix, const char * op); // Prints the operand-string (the operand `op` formated according to addressing mode `aix`) to `str`.")
  print("            int          isa_parse          (char * ops);             // ops is an operand string. Returns the addressing mode that maps the syntax of ops, and isolates the operand in ops")
  print()
  print()

def print_h_instructions() :
  print("// Instruction ========================================================")
  print("// Objects containing an instruction definitions - what does LDA mean (and which addressing mode variants does it have)")
  print("// The definitions are available through indexes.")
  print()
  print("// The enumeration of all instructions")
  for ins in instructions :
    print( f"#define ISA_IIX_{ins.iname}   {ins.iix:2} {' // trap uninitialized variables' if ins.iix==0 else ''}")
  print( f"#define ISA_IIX_FIRST  1 // for iteration: first")
  print( f"#define ISA_IIX_LAST  {len(instructions)} // for iteration: (one after) last")
  print()
  print("// Opcode 0xBB is not in use in the 6502. We use it in instruction.opcodes to signal the addrmode does not exist for that instruction")
  print("#define ISA_OPCODE_INVALID 0xBB")
  print()
  print("/*PROGMEM*/ const char * isa_instruction_iname  ( int iix );          // The instruction name (e.g. LDA)")
  print("/*PROGMEM*/ const char * isa_instruction_desc   ( int iix );          // The description of the instruction")
  print("/*PROGMEM*/ const char * isa_instruction_help   ( int iix );          // The detailed description of the instruction")
  print("/*PROGMEM*/ const char * isa_instruction_flags  ( int iix );          // The (program status) flags the instruction updates")
  print("            uint8_t      isa_instruction_opcodes( int iix, int aix ); // For each addrmode, the actual opcode")
  print("            int          isa_snprint_iname      (char * str, int size, int minlen, int iix); // Prints the instruction name (eg LDA) associated with `iix` to `str`.")
  print("            int          isa_instruction_find   (const char * iname); // Returns iix for `iname` or 0 if not found")
  print()
  print()

def print_h_opcodes() :
  print("// Opcode =============================================================")
  print("// Objects containing an opcode definitions - how many cycles does LDA.ABS need")
  print("// The definitions are available through indexes - the actual 6502 opcodes.")
  print("// Opcode that are not in use have the fields 0")
  print()
  print("uint8_t isa_opcode_iix    ( uint8_t opcode ); // Index into isa_instruction_xxx[]")
  print("uint8_t isa_opcode_aix    ( uint8_t opcode ); // Index into isa_addrmode_xxx[]")
  print("uint8_t isa_opcode_cycles ( uint8_t opcode ); // The (minimal) number of cycles to execute this instruction variant (0..)")
  print("uint8_t isa_opcode_xcycles( uint8_t opcode ); // The worst case additional number of cycles to execute this instruction variant (0..)")
  print("")
  print("")

def print_h_footer() :
  print("#endif")

def print_h() :
  print_h_header()
  print_h_addrmodes()
  print_h_instructions()
  print_h_opcodes()
  print_h_footer()

def print_test() :
  for ins in instructions :
    print(f'  # test instruction {ins.iname} - GENERATED BY isa6502.py')
    print(f'  def test_asm_{ins.iname}(self) :')
    print(f'    self.cmd.exec("asm 200", "> ") # Start streaming')
    expect=""
    num= 0
    addr= 0x200
    for am in addrmodes :
      var= variant_find_by_name(ins,am.aname)
      if var:
        # Generate code line
        if am.bytes==1: code= f'{addrmode_op_subst(am,"?")}'
        elif am.bytes==2: code= f'{addrmode_op_subst(am,"A8")}'
        elif am.bytes==3: code= f'{addrmode_op_subst(am,"A8B9")}'
        else : code= f'OOPS'
        code= " "+code if code else ""
        code= f"{ins.iname}"+code
        # Generate expected answer
        if am.bytes==1  : ans= f'     '
        elif am.bytes==2: ans= f'A8   '      
        elif am.bytes==3: ans= f'B9 A8'      
        else : code= f'OOPS'
        # Help for REL
        relhelp= f" ({addr+2+(0xA8-256):04X})" if am.aname=="REL" else ""
        ans= f"{addr:04X} {var.opcode:02X} "+ans+" "+code+relhelp+"\\r\\n"
        # Print
        print( f'    self.cmd.exec("{code}","> ") # {am.aname}')
        expect+= ans
        num+= 1
        addr+= am.bytes
    print( f'    self.cmd.exec("") # Stop streaming')
    print( f'    r= self.cmd.exec("dasm 0200 {num:02X}")')
    print( f'    self.assertEqual("{expect}",r)')
    print()


  
def help() :
  print("SYNTAX 6502ins <table>")
  print("where <table> is one of")
  print("  help   - prints this help")
  print("  amodes - prints the addressing modes")
  print("  insts  - prints alphabetical list of instructions, a column per address mode")
  print("  opcodes- prints the 16x16 opcode matrix")
  print("  cpp    - prints source file for c module")
  print("  h      - prints header file for c module")
  print("  test   - prints python file for testing")
  
def main() :
  if len(sys.argv)==1:
    print("Welcome to 6502ins")
    print()
    print("Missing table name, try 'help'")
    exit(1)
  if len(sys.argv)>2:
    print("Welcome to 6502ins")
    print()
    print("Specify one table name, try 'help'")
    exit(1)
  if sys.argv[1]=="help" :
    print("Welcome to 6502ins")
    print()
    help()
  elif sys.argv[1]=="amodes" :
    print_amodes()
  elif sys.argv[1]=="insts" :
    print_insts()
  elif sys.argv[1]=="opcodes" :
    print_opcodes()
  elif sys.argv[1]=="cpp" :
    print_cpp()
  elif sys.argv[1]=="h" :
    print_h()
  elif sys.argv[1]=="test" :
    print_test()
  else :
    print("Welcome to 6502ins")
    print()
    print("Unknown table name, try 'help'")
    exit(1)

main()


