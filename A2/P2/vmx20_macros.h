#include <stdint.h>

// instructions
#define INS_HALT    0x00
#define INS_LOAD    0x01
#define INS_STORE   0x02
#define INS_LDIMM   0x03
#define INS_LDADDR  0x04
#define INS_LDIND   0x05
#define INS_STIND   0x06
#define INS_ADDF    0x07
#define INS_SUBF    0x08
#define INS_DIVF    0x09
#define INS_MULF    0x0a
#define INS_ADDI    0x0b
#define INS_SUBI    0x0c
#define INS_DIVI    0x0d
#define INS_MULI    0x0e
#define INS_CALL    0x0f
#define INS_RET     0x10
#define INS_BLT     0x11
#define INS_BGT     0x12
#define INS_BEQ     0x13    
#define INS_JMP     0x14
#define INS_CMPXCHG 0x15
#define INS_GETPID  0x16
#define INS_GETPN   0x17
#define INS_PUSH    0x18
#define INS_POP     0x19
#define INS_NOP     0x20
#define INS_INVALID 0x21

// instruction formats
#define F_OP            1
#define F_ADDR          2
#define F_REG           3
#define F_REGCONST      4
#define F_REGADDR       5
#define F_REGREG        6
#define F_REGOFF        7
#define F_REGREGADDR    8
#define F_INVALID       9

#define EXTENDSIGN20(x) ((x) >> 19 ? (x) | 0xfff00000 : (x)) 
#define EXTENDSIGN16(x) ((x) >> 15 ? (x) | 0xffff0000 : (x))