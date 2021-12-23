#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <string>
#include <cstdint>

enum class OpCode {
    NOP = 0,

    HALT,

    SETA,
    SETB,
    SETC,

    LOADA,
    LOADB,
    LOADC,

    STOREA,
    STOREB,
    STOREC,

    READA,
    READB,
    READC,

    WRITEA,
    WRITEB,
    WRITEC,

    PUSHA,
    PUSHB,
    PUSHC,

    POPA,
    POPB,
    POPC,

    MOVCA,
    MOVCB,
    MOVCIDX,

    INCA,
    INCB,
    INCC,

    IDXA,
    IDXB,
    IDXC,

    WRITEAX,
    WRITEBX,
    WRITECX,

    ADD,
    SUB,
    MUL,
    DIV,
    IDIV,
    MOD,
    EXP,

    LSHIFT,
    RSHIFT,
    BNOT,
    BAND,
    BOR,
    XOR,

    ATAN,
    COS,
    LOG,
    SIN,
    SQR,
    TAN,

    RND,
    SEED,

    FLT,
    INT,
    PTR,
    STR,
    VSTR,

    AND,
    OR,
    NOT,

    EQ,
    NE,
    GT,
    GE,
    LT,
    LE,
    CMP,

    SETIDX,
    MOVIDX,
    LOADIDX,
    STOREIDX,
    INCIDX,
    SAVEIDX,
    PUSHIDX,
    POPIDX,

    JMP,
    JMPEZ,
    JMPNZ,

    IDATA,
    FDATA,
    PDATA,
    SDATA,

    SYSCALL,

    CALL,
    RETURN,

    IRQ,

    ALLOC,
    CALLOC,

    FREE,
    FREEC,

    YIELD,

    TRACE,

    COUNT
};

enum class RuntimeValue {
    NONE = 0,
    A,
    B,
    C,
    IDX,
    PC,
    COUNT
};

enum class SysCall {
    CLS = 0,
    WRITE,
    READ,
    READKEY,
    KEYSET,
    PALETTE,
    COLOUR,
    CURSOR,
    DRAW,
    DRAWLINE,
    DRAWBOX,
    BLIT,
    SOUND,
    VOICE,
    COUNT
};

std::string OpCodeAsString(OpCode opcode);

#endif //__SYSTEM_H__
