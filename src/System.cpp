#include "System.h"

std::string OpCodeAsString(OpCode opcode) {
    switch(opcode) {
        case OpCode::NOP: return "NOP";
        case OpCode::HALT: return "HALT";
        case OpCode::SETA: return "SETA";
        case OpCode::SETB: return "SETB";
        case OpCode::SETC: return "SETC";
        case OpCode::LOADA: return "LOADA";
        case OpCode::LOADB: return "LOADB";
        case OpCode::LOADC: return "LOADC";
        case OpCode::STOREA: return "STOREA";
        case OpCode::STOREB: return "STOREB";
        case OpCode::STOREC: return "STOREC";
        case OpCode::READA: return "READA";
        case OpCode::READB: return "READB";
        case OpCode::READC: return "READC";
        case OpCode::WRITEA: return "WRITEA";
        case OpCode::WRITEB: return "WRITEB";
        case OpCode::WRITEC: return "WRITEC";
        case OpCode::PUSHA: return "PUSHA";
        case OpCode::PUSHB: return "PUSHB";
        case OpCode::PUSHC: return "PUSHC";
        case OpCode::POPA: return "POPA";
        case OpCode::POPB: return "POPB";
        case OpCode::POPC: return "POPC";
        case OpCode::MOVCA: return "MOVCA";
        case OpCode::MOVCB: return "MOVCB";
        case OpCode::MOVCIDX: return "MOVCIDX";
        case OpCode::INCA: return "INCA";
        case OpCode::INCB: return "INCB";
        case OpCode::INCC: return "INCC";
        case OpCode::IDXA: return "IDXA";
        case OpCode::IDXB: return "IDXB";
        case OpCode::IDXC: return "IDXC";
        case OpCode::WRITEAX: return "WRITEAX";
        case OpCode::WRITEBX: return "WRITEBX";
        case OpCode::WRITECX: return "WRITECX";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::IDIV: return "IDIV";
        case OpCode::MOD: return "MOD";
        case OpCode::POW: return "POW";
        case OpCode::EXP: return "EXP";
        case OpCode::LSHIFT: return "LSHIFT";
        case OpCode::RSHIFT: return "RSHIFT";
        case OpCode::BNOT: return "BNOT";
        case OpCode::BAND: return "BAND";
        case OpCode::BOR: return "BOR";
        case OpCode::XOR: return "XOR";
        case OpCode::ATAN: return "ATAN";
        case OpCode::COS: return "COS";
        case OpCode::LOG: return "LOG";
        case OpCode::SIN: return "SIN";
        case OpCode::SQR: return "SQR";
        case OpCode::TAN: return "TAN";
        case OpCode::RND: return "RND";
        case OpCode::SEED: return "SEED";
        case OpCode::FLT: return "FLT";
        case OpCode::INT: return "INT";
        case OpCode::PTR: return "PTR";
        case OpCode::STR: return "STR";
        case OpCode::VSTR: return "VSTR";
        case OpCode::AND: return "AND";
        case OpCode::OR: return "OR";
        case OpCode::NOT: return "NOT";
        case OpCode::EQ: return "EQ";
        case OpCode::NE: return "NE";
        case OpCode::GT: return "GT";
        case OpCode::GE: return "GE";
        case OpCode::LT: return "LT";
        case OpCode::LE: return "LE";
        case OpCode::CMP: return "CMP";
        case OpCode::SETIDX: return "SETIDX";
        case OpCode::MOVIDX: return "MOVIDX";
        case OpCode::LOADIDX: return "LOADIDX";
        case OpCode::STOREIDX: return "STOREIDX";
        case OpCode::INCIDX: return "INCIDX";
        case OpCode::SAVEIDX: return "SAVEIDX";
        case OpCode::PUSHIDX: return "PUSHIDX";
        case OpCode::POPIDX: return "POPIDX";
        case OpCode::JMP: return "JMP";
        case OpCode::JMPEZ: return "JMPEZ";
        case OpCode::JMPNZ: return "JMPNZ";
        case OpCode::IDATA: return "IDATA";
        case OpCode::FDATA: return "FDATA";
        case OpCode::PDATA: return "PDATA";
        case OpCode::SDATA: return "SDATA";
        case OpCode::SYSCALL: return "SYSCALL";
        case OpCode::CALL: return "CALL";
        case OpCode::RETURN: return "RETURN";
        case OpCode::IRQ: return "IRQ";
        case OpCode::ALLOC: return "ALLOC";
        case OpCode::CALLOC: return "CALLOC";
        case OpCode::FREE: return "FREE";
        case OpCode::FREEIDX: return "FREEIDX";
        case OpCode::COPY: return "COPY";
        case OpCode::YIELD: return "YIELD";
        case OpCode::TRACE: return "TRACE";
        default: return "????";
    }
}

std::map<std::string, std::pair<OpCode, ArgType>> OpCodeDefinition = {
    {"NOP", {OpCode::NOP, ArgType::NONE}},

    {"HALT", {OpCode::HALT, ArgType::NONE}},

    {"SETA", {OpCode::SETA, ArgType::VALUE}},
    {"SETB", {OpCode::SETB, ArgType::VALUE}},
    {"SETC", {OpCode::SETC, ArgType::VALUE}},

    {"LOADA", {OpCode::LOADA, ArgType::POINTER}},
    {"LOADB", {OpCode::LOADB, ArgType::POINTER}},
    {"LOADC", {OpCode::LOADC, ArgType::POINTER}},

    {"STOREA", {OpCode::STOREA, ArgType::POINTER}},
    {"STOREB", {OpCode::STOREB, ArgType::POINTER}},
    {"STOREC", {OpCode::STOREC, ArgType::POINTER}},

    {"READA", {OpCode::READA, ArgType::VALUE}},
    {"READB", {OpCode::READB, ArgType::VALUE}},
    {"READC", {OpCode::READC, ArgType::VALUE}},

    {"WRITEA", {OpCode::WRITEA, ArgType::VALUE}},
    {"WRITEB", {OpCode::WRITEB, ArgType::VALUE}},
    {"WRITEC", {OpCode::WRITEC, ArgType::VALUE}},

    {"PUSHA", {OpCode::PUSHA, ArgType::NONE}},
    {"PUSHB", {OpCode::PUSHB, ArgType::NONE}},
    {"PUSHC", {OpCode::PUSHC, ArgType::NONE}},

    {"POPA", {OpCode::POPA, ArgType::NONE}},
    {"POPB", {OpCode::POPB, ArgType::NONE}},
    {"POPC", {OpCode::POPC, ArgType::NONE}},

    {"MOVCA", {OpCode::MOVCA, ArgType::NONE}},
    {"MOVCB", {OpCode::MOVCB, ArgType::NONE}},
    {"MOVCIDX", {OpCode::MOVCIDX, ArgType::NONE}},

    {"INCA", {OpCode::INCA, ArgType::VALUE}},
    {"INCB", {OpCode::INCB, ArgType::VALUE}},
    {"INCC", {OpCode::INCC, ArgType::VALUE}},

    {"IDXA", {OpCode::IDXA, ArgType::NONE}},
    {"IDXB", {OpCode::IDXB, ArgType::NONE}},
    {"IDXC", {OpCode::IDXC, ArgType::NONE}},

    {"WRITEAX", {OpCode::WRITEAX, ArgType::NONE}},
    {"WRITEBX", {OpCode::WRITEBX, ArgType::NONE}},
    {"WRITECX", {OpCode::WRITECX, ArgType::NONE}},

    {"ADD", {OpCode::ADD, ArgType::NONE}},
    {"SUB", {OpCode::SUB, ArgType::NONE}},
    {"MUL", {OpCode::MUL, ArgType::NONE}},
    {"DIV", {OpCode::DIV, ArgType::NONE}},
    {"IDIV", {OpCode::IDIV, ArgType::NONE}},
    {"MOD", {OpCode::MOD, ArgType::NONE}},
    {"POW", {OpCode::POW, ArgType::NONE}},
    {"EXP", {OpCode::EXP, ArgType::NONE}},

    {"LSHIFT", {OpCode::LSHIFT, ArgType::NONE}},
    {"RSHIFT", {OpCode::RSHIFT, ArgType::NONE}},
    {"BNOT", {OpCode::BNOT, ArgType::NONE}},
    {"BAND", {OpCode::BAND, ArgType::NONE}},
    {"BOR", {OpCode::BOR, ArgType::NONE}},
    {"XOR", {OpCode::XOR, ArgType::NONE}},

    {"ATAN", {OpCode::ATAN, ArgType::NONE}},
    {"COS", {OpCode::COS, ArgType::NONE}},
    {"LOG", {OpCode::LOG, ArgType::NONE}},
    {"SIN", {OpCode::SIN, ArgType::NONE}},
    {"SQR", {OpCode::SQR, ArgType::NONE}},
    {"TAN", {OpCode::TAN, ArgType::NONE}},

    {"RND", {OpCode::RND, ArgType::NONE}},
    {"SEED", {OpCode::SEED, ArgType::NONE}},

    {"FLT", {OpCode::FLT, ArgType::NONE}},
    {"INT", {OpCode::INT, ArgType::NONE}},
    {"PTR", {OpCode::PTR, ArgType::NONE}},
    {"STR", {OpCode::STR, ArgType::NONE}},
    {"VSTR", {OpCode::VSTR, ArgType::NONE}},

    {"AND", {OpCode::AND, ArgType::NONE}},
    {"OR", {OpCode::OR, ArgType::NONE}},
    {"NOT", {OpCode::NOT, ArgType::NONE}},

    {"EQ", {OpCode::EQ, ArgType::NONE}},
    {"NE", {OpCode::NE, ArgType::NONE}},
    {"GT", {OpCode::GT, ArgType::NONE}},
    {"GE", {OpCode::GE, ArgType::NONE}},
    {"LT", {OpCode::LT, ArgType::NONE}},
    {"LE", {OpCode::LE, ArgType::NONE}},
    {"CMP", {OpCode::CMP, ArgType::NONE}},

    {"SETIDX", {OpCode::SETIDX, ArgType::POINTER}},
    {"MOVIDX", {OpCode::MOVIDX, ArgType::VALUE}},
    {"LOADIDX", {OpCode::LOADIDX, ArgType::POINTER}},
    {"STOREIDX", {OpCode::STOREIDX, ArgType::VALUE}},
    {"INCIDX", {OpCode::INCIDX, ArgType::VALUE}},
    {"SAVEIDX", {OpCode::SAVEIDX, ArgType::POINTER}},
    {"PUSHIDX", {OpCode::PUSHIDX, ArgType::NONE}},
    {"POPIDX", {OpCode::POPIDX, ArgType::NONE}},

    {"JMP", {OpCode::JMP, ArgType::LABEL}},
    {"JMPEZ", {OpCode::JMPEZ, ArgType::LABEL}},
    {"JMPNZ", {OpCode::JMPNZ, ArgType::LABEL}},

    {"IDATA", {OpCode::IDATA, ArgType::INT}},
    {"FDATA", {OpCode::FDATA, ArgType::FLOAT}},
    {"PDATA", {OpCode::PDATA, ArgType::POINTER}},
    {"SDATA", {OpCode::SDATA, ArgType::STRING}},

    {"SYSCALL", {OpCode::SYSCALL, ArgType::SYSCALL}},

    {"CALL", {OpCode::CALL, ArgType::LABEL}},
    {"RETURN", {OpCode::RETURN, ArgType::NONE}},

    {"IRQ", {OpCode::IRQ, ArgType::INT}},

    {"ALLOC", {OpCode::ALLOC, ArgType::INT}},
    {"CALLOC", {OpCode::CALLOC, ArgType::NONE}},

    {"FREE", {OpCode::FREE, ArgType::POINTER}},
    {"FREEIDX", {OpCode::FREEIDX, ArgType::NONE}},

    {"COPY", {OpCode::COPY, ArgType::NONE}},

    {"YIELD", {OpCode::YIELD, ArgType::NONE}},

    {"TRACE", {OpCode::TRACE, ArgType::INT}}
};


