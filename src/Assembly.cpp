#include "Assembly.h"

#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>

std::map<std::string, std::pair<OpCode, ArgType>> def = {
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
    {"EXP", {OpCode::EXP, ArgType::NONE}},

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

    {"YIELD", {OpCode::YIELD, ArgType::NONE}},

    {"TRACE", {OpCode::TRACE, ArgType::INT}}
};

static std::pair<std::string, std::string> getSysCall(SysCall syscall, RuntimeValue rt) {
    std::string syscallname;
    std::string rtname;

    if (syscall == SysCall::CLS) {
        syscallname = "CLS";
    } else if (syscall == SysCall::WRITE) {
        syscallname = "WRITE";
    } else if (syscall == SysCall::READ) {
        syscallname = "READ";
    } else if (syscall == SysCall::READKEY) {
        syscallname = "READKEY";
    } else if (syscall == SysCall::KEYSET) {
        syscallname = "KEYSET";
    } else if (syscall == SysCall::PALETTE) {
        syscallname = "COLOUR";
    } else if (syscall == SysCall::COLOUR) {
        syscallname = "PALETTE";
    } else if (syscall == SysCall::DRAW) {
        syscallname = "DRAW";
    } else if (syscall == SysCall::DRAWLINE) {
        syscallname = "DRAWLINE";
    } else if (syscall == SysCall::DRAWBOX) {
        syscallname = "DRAWBOX";
    } else if (syscall == SysCall::BLIT) {
        syscallname = "BLIT";
    } else if (syscall == SysCall::SOUND) {
        syscallname = "SOUND";
    } else if (syscall == SysCall::VOICE) {
        syscallname = "VOICE";
    } else {
        throw std::domain_error("Unknown SysCall");
    }

    if (rt == RuntimeValue::NONE) {
        rtname = "NONE";
    } else if (rt == RuntimeValue::A) {
        rtname = "A";
    } else if (rt == RuntimeValue::B) {
        rtname = "B";
    } else if (rt == RuntimeValue::C) {
        rtname = "C";
    } else if (rt == RuntimeValue::IDX) {
        rtname = "IDX";
    } else {
        throw std::domain_error("Unknown RuntimeValue");
    }

    return std::pair<std::string, std::string>(syscallname, rtname);
}


size_t AsmToken::size() const {
    if (arg != std::nullopt) {
        if (std::holds_alternative<int16_t>(*arg)) {
            return 1+sizeof(int16_t);
        } else if (std::holds_alternative<float>(*arg)) {
            return 1+sizeof(float);
        } else if (std::holds_alternative<int32_t>(*arg)) {
            return 1+sizeof(int32_t);
        } else if (std::holds_alternative<uint32_t>(*arg)) {
            return 1+sizeof(uint32_t);
        } else if (std::holds_alternative<uint64_t>(*arg)) {
            return 1+sizeof(uint64_t);
        } else if (std::holds_alternative<std::string>(*arg)) {
            auto value = std::get<std::string>(*arg);
            return 1 + value.size();
        } else if (std::holds_alternative<std::pair<SysCall, RuntimeValue>>(*arg)) {
            return 1+sizeof(int16_t)+sizeof(int16_t);
        } else {
            std::cerr << "Error in size " << (int)opcode << std::endl;
        }
    }
    return 1;
}

std::string AsmToken::toString() const {
    std::ostringstream s;

    if (label.size()) {
        if (def[OpCodeAsString(opcode)].second == ArgType::LABEL) {
            s << OpCodeAsString(opcode) << " " << label;
        } else {
            s << label << ":" << std::endl;
            s << OpCodeAsString(opcode);
        }
    } else {
        s << OpCodeAsString(opcode);
    }

    if (arg != std::nullopt) {
        s << " ";

        if (std::holds_alternative<int16_t>(*arg)) {
            auto value = std::get<int16_t>(*arg);
            s << value;
        } else if (std::holds_alternative<float>(*arg)) {
            auto value = std::get<float>(*arg);
            s << std::to_string(value);
        } else if (std::holds_alternative<int32_t>(*arg)) {
            auto value = std::get<int32_t>(*arg);
            s << "0x" << std::setfill('0') << std::setw(6) << std::uppercase << std::hex << (uint32_t)value;
        } else if (std::holds_alternative<uint32_t>(*arg)) {
            const uint32_t SIGN_BIT = 0x80000000;
            const uint32_t QNAN = 0x7F800000;

            auto value = std::get<uint32_t>(*arg);

            if (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT)) {
                s << "0x" << std::setfill('0') << std::setw(6) << std::uppercase << std::hex << (uint32_t)((~(QNAN | SIGN_BIT)) & value);
            } else if (((value & SIGN_BIT) != SIGN_BIT) && ((value & QNAN) == QNAN)) {
                int16_t i = (int16_t)(0xFFFF & value);
                s << std::to_string(i);
            } else {
                float f;
                std::memcpy(&f, &value, sizeof(float));
                s << std::to_string(f);
            }
        } else if (std::holds_alternative<uint64_t>(*arg)) {
            const uint64_t SIGN_BIT = 0x8000000000000000;
            const uint64_t QNAN = 0X7FFC000000000000;

            auto value = std::get<uint64_t>(*arg);

            if (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT)) {
                s << "0x" << std::setfill('0') << std::setw(6) << std::uppercase << std::hex << (uint32_t)((~(QNAN | SIGN_BIT)) & value);
            } else if (((value & SIGN_BIT) != SIGN_BIT) && ((value & QNAN) == QNAN)) {
                int32_t i = (int32_t)(0xFFFFFFFF & value);
                s << std::to_string(i);
            } else {
                double f;
                std::memcpy(&f, &value, sizeof(double));
                s << std::to_string(f);
            }

        } else if (std::holds_alternative<std::string>(*arg)) {
            auto value = std::get<std::string>(*arg);
            std::string res;

            for (const auto &c : value) {
                switch (c) {
                    case '\n':
                        res += '\\';
                        res += 'n';
                        break;
                    case '\t':
                        res += '\\';
                        res += 't';
                        break;
                    case '\\':
                        res += '\\';
                        res += '\\';
                        break;
                    case '"':
                        res += '\\';
                        res += '"';
                        break;
                    default:
                        res += c;
                }
            }

            s << "\"" << res << "\"";
        } else if (std::holds_alternative<std::pair<SysCall, RuntimeValue>>(*arg)) {
            auto value = std::get<std::pair<SysCall, RuntimeValue>>(*arg);
            auto syscall = getSysCall(value.first, value.second);
            s << syscall.first << " " << syscall.second;
        }
    }

    return s.str();
}
