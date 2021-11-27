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

    size_t size() const;
    std::string toString() const;
};


#endif //__ASSEMBLY_H__
