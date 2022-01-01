#include "Assembly.h"

#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>

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
        syscallname = "PALETTE";
    } else if (syscall == SysCall::COLOUR) {
        syscallname = "COLOUR";
    } else if (syscall == SysCall::CURSOR) {
        syscallname = "CURSOR";
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
    } else if (syscall == SysCall::MOUSE) {
        syscallname = "MOUSE";
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
        if (OpCodeDefinition[OpCodeAsString(opcode)].second == ArgType::LABEL) {
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

std::vector<AsmToken> optimise(const int cpu, const std::vector<AsmToken> &asmTokens) {
    std::vector<AsmToken> output;

    size_t current = 0;

    auto NOP = AsmToken(OpCode::NOP);
    auto MOVCA = AsmToken(OpCode::MOVCA);
    auto MOVCB = AsmToken(OpCode::MOVCB);
    auto MOVCIDX = AsmToken(OpCode::MOVIDX);

    while (current < asmTokens.size()) {
        auto asmToken = asmTokens[current++];
        if (asmToken.isNone()) {
            auto next = asmTokens[current++];

            if (asmToken.opcode == OpCode::PUSHC && next.opcode == OpCode::POPC) {
                output.push_back(NOP);
                output.push_back(NOP);
            } else if (asmToken.opcode == OpCode::PUSHC && next.opcode == OpCode::POPA) {
                output.push_back(MOVCA);
                output.push_back(NOP);
            } else if (asmToken.opcode == OpCode::PUSHC && next.opcode == OpCode::POPB) {
                output.push_back(MOVCB);
                output.push_back(NOP);
            } else if (asmToken.opcode == OpCode::PUSHC && next.opcode == OpCode::POPIDX) {
                output.push_back(MOVCIDX);
                output.push_back(NOP);
            } else {
                output.push_back(asmToken);
                output.push_back(next);
            }
        } else {
            output.push_back(asmToken);
        }
    }

    return output;
}
