#ifndef __BINARY_H__
#define __BINARY_H__

#include "Assembly.h"


class Binary {
    std::vector<uint8_t> code;
    const int cpu;

    void addByte(uint8_t b);
    void addShort(int16_t s);
    void addFloat(float f);
    void addPointer(uint32_t p);
    void addValue32(uint32_t v);
    void addValue64(uint64_t v);
    void addSyscall(SysCall syscall);

    uint32_t add(OpCode opcode);
    uint32_t addByte(OpCode opcode, uint8_t b);
    uint32_t addShort(OpCode opcode, int16_t s);
    uint32_t addFloat(OpCode opcode, float f);
    uint32_t addString(OpCode opcode, const std::string &str);
    uint32_t addPointer(OpCode opcode, uint32_t p);
    uint32_t addValue32(OpCode opcode, uint32_t v);
    uint32_t addValue64(OpCode opcode, uint64_t v);
    uint32_t addSyscall(OpCode opcode, SysCall syscall, RuntimeValue rtarg);

    void updateShort(uint32_t pos, int16_t s);
public:
    Binary(int cpu) : cpu(cpu) {
    }

    std::vector<uint8_t> translate(const std::vector<AsmToken> &tokens);
};

#endif //__BINARY_H__
