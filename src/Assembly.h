#ifndef __ASSEMBLY_H__
#define __ASSEMBLY_H__

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <iostream>

#include <optional>
#include <variant>

#include "System.h"

enum class ArgType {
    NONE,
    INT,
    FLOAT,
    POINTER,
    STRING,
    VALUE,
    SYSCALL,
    LABEL,
    COUNT
};

struct AsmToken {
    OpCode opcode;
    std::optional<std::variant<int16_t, float, int32_t, uint32_t, uint64_t, std::string, std::pair<SysCall, RuntimeValue>>> arg;
    std::string label;

    AsmToken(OpCode opcode) : opcode(opcode), label("") {
        arg = std::nullopt;
    }

    AsmToken(OpCode opcode, int16_t i) : opcode(opcode), arg(i) {
    }

    AsmToken(OpCode opcode, float f) : opcode(opcode), arg(f) {
    }

    AsmToken(OpCode opcode, int32_t p) : opcode(opcode), arg(p) {
    }

    AsmToken(OpCode opcode, uint32_t v) : opcode(opcode), arg(v) {
    }

    AsmToken(OpCode opcode, uint64_t v) : opcode(opcode), arg(v) {
    }

    AsmToken(OpCode opcode, const std::string &str) : opcode(opcode), arg(str) {
    }

    AsmToken(OpCode opcode, std::pair<SysCall, RuntimeValue> syscall) : opcode(opcode), arg(syscall) {
    }

    bool iSNone() const {
        return arg == std::nullopt;
    }

    bool iSShort() const {
        return std::holds_alternative<int16_t>(*arg);
    }

    bool iSFloat() const {
        return std::holds_alternative<float>(*arg);
    }

    bool iSPointer() const {
        return std::holds_alternative<int32_t>(*arg);
    }

    bool iSValue() const {
        return std::holds_alternative<uint32_t>(*arg) || std::holds_alternative<uint64_t>(*arg);
    }

    bool iSString() const {
        return std::holds_alternative<std::string>(*arg);
    }

    bool iSSysCall() const {
        return std::holds_alternative<std::pair<SysCall, RuntimeValue>>(*arg);
    }

    size_t size() const;
    std::string toString() const;
};

std::vector<AsmToken> optimise(const int cpu, const std::vector<AsmToken> &asmTokens);

#endif //__ASSEMBLY_H__
