#include "Binary.h"

void Binary::addByte(uint8_t b) {
    code.push_back(b);
}

void Binary::addShort(int16_t s) {
    uint8_t *bytes = (uint8_t *)&s;

    addByte(bytes[0]);
    addByte(bytes[1]);
}

void Binary::addFloat(float f) {
    uint8_t *bytes = (uint8_t *)&f;

    addByte(bytes[0]);
    addByte(bytes[1]);
    addByte(bytes[2]);
    addByte(bytes[3]);
}

void Binary::addPointer(uint32_t p) {
    uint8_t *bytes = (uint8_t *)&p;

    addByte(bytes[0]);
    addByte(bytes[1]);

    if (cpu == 32) {
        addByte(bytes[2]);
        addByte((uint8_t)(bytes[3] & 0x07));
    } else {
        addByte((uint8_t)(bytes[2] & 0x7F));
        addByte(0);
    }
}

void Binary::addValue32(uint32_t v) {
    uint8_t *bytes = (uint8_t *)&v;

    addByte(bytes[0]);
    addByte(bytes[1]);
    addByte(bytes[2]);
    addByte(bytes[3]);
}

void Binary::addValue64(uint64_t v) {
    uint8_t *bytes = (uint8_t *)&v;

    addByte(bytes[0]);
    addByte(bytes[1]);
    addByte(bytes[2]);
    addByte(bytes[3]);

    addByte(bytes[4]);
    addByte(bytes[5]);
    addByte(bytes[6]);
    addByte(bytes[7]);
}

void Binary::addSyscall(SysCall syscall) {
    uint8_t *bytes = (uint8_t *)&syscall;

    addByte(bytes[0]);
    addByte(bytes[1]);
}

uint32_t Binary::add(OpCode opcode) {
    uint32_t pos = code.size();

    addByte((uint8_t)opcode);

    return pos;
}

uint32_t Binary::addByte(OpCode opcode, uint8_t b) {
    uint32_t pos = code.size();

    addByte((uint8_t)opcode);
    addByte(b);

    return pos;
}

uint32_t Binary::addShort(OpCode opcode, int16_t s) {
    uint32_t pos = code.size();

    addByte((uint8_t)opcode);
    addShort(s);

    return pos;
}

uint32_t Binary::addFloat(OpCode opcode, float f) {
    uint32_t pos = code.size();

    addByte((uint8_t)opcode);
    addFloat(f);

    return pos;
}

uint32_t Binary::addString(OpCode opcode, const std::string &str) {
    uint32_t pos = code.size();

    addByte((uint8_t)opcode);
    for (const auto c : str)
        addByte((uint8_t)c);
    addByte((uint8_t)0);

    return pos;
}

uint32_t Binary::addPointer(OpCode opcode, uint32_t p) {
    uint32_t pos = code.size();

    code.push_back((uint8_t)opcode);
    addPointer(p);

    return pos;
}

uint32_t Binary::addValue32(OpCode opcode, uint32_t v) {
    uint32_t pos = code.size();

    code.push_back((uint8_t)opcode);
    addValue32(v);

    return pos;
}

uint32_t Binary::addValue64(OpCode opcode, uint64_t v) {
    uint32_t pos = code.size();

    code.push_back((uint8_t)opcode);
    addValue64(v);

    return pos;
}

uint32_t Binary::addSyscall(OpCode opcode, SysCall syscall, RuntimeValue rtarg) {
    uint32_t pos = code.size();

    code.push_back((uint8_t)opcode);
    addSyscall(syscall);
    addShort((int16_t)rtarg);

    return pos;
}

void Binary::updateShort(uint32_t pos, int16_t s) {
    uint8_t *bytes = (uint8_t *)&s;

    code[pos+0] = bytes[0];
    code[pos+1] = bytes[1];
}

std::vector<uint8_t> Binary::translate(const std::vector<AsmToken> &tokens) {
    std::map<std::string, uint32_t> labels;
    std::map<uint32_t, std::string> jumps;

    for (auto token : tokens) {
        uint32_t pos = 0;

        auto argtype = OpCodeDefinition[OpCodeAsString(token.opcode)].second;

        if (token.isNone()) {
            if (argtype == ArgType::LABEL) {
                pos = addShort(token.opcode, 0);
            } else {
                pos = add(token.opcode);
            }
        } else if (token.isShort()) {
            pos = addShort(token.opcode, std::get<int16_t>(*token.arg));
        } else if (token.isFloat()) {
            pos = addFloat(token.opcode, std::get<float>(*token.arg));
        } else if (token.isPointer()) {
            pos = addPointer(token.opcode, (uint32_t)std::get<int32_t>(*token.arg));
        } else if (token.isValue()) {
            if (cpu == 32) {
                pos = addValue64(token.opcode, std::get<uint64_t>(*token.arg));
            } else {
                pos = addValue32(token.opcode, std::get<uint32_t>(*token.arg));
            }
        } else if (token.isString()) {
            pos = addString(token.opcode, std::get<std::string>(*token.arg));
        } else if (token.isSysCall()) {
            auto syscall = std::get<std::pair<SysCall, RuntimeValue>>(*token.arg);
            pos = addSyscall(token.opcode, syscall.first, syscall.second);
        }

        if (token.label.size()) {
            if (argtype == ArgType::LABEL) {
                jumps[pos] = token.label;
            } else {
                labels[token.label] = pos;
            }
        }
    }

    for (auto jump : jumps) {
        const uint32_t pos = jump.first;
        const std::string label = jump.second;

        auto dst = labels.find(label);

        if (dst == labels.end()) {
            std::cerr << "Unknown label " << label << std::endl;
            exit(-1);
        }

        updateShort(pos+1, dst->second);
    }

    return code;
}
