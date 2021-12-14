#include <vector>

#include "Environment.h"

Array::Array(VariableType type, size_t length, size_t offset) : type(std::shared_ptr<VariableType>(new VariableType(type))), length(length), offset(offset) {
}

bool Array::operator==(const Array &rhs) const {
    return *type == *rhs.type && length == rhs.length;
}

bool Array::operator!=(const Array &rhs) const {
    return *type != *rhs.type || length != rhs.length;
}

VariableType Array::getType() const {
    return *type;
}

bool Struct::operator==(const Struct &rhs) const {
    return name == rhs.name;
}

bool Struct::operator!=(const Struct &rhs) const {
    return name != rhs.name;
}

VariableType Struct::getType(const std::string &slot) const {
    for (const auto &s : slots) {
        if (s.first == slot)
            return s.second;
    }

    throw std::invalid_argument("Undefined slot `" + slot + "' in struct `" + name + "'");
    return SimpleType::NONE;
}

uint32_t Struct::getOffset(const std::string &slot) const {
    uint32_t i = 0;
    for (const auto &s : slots) {
        if (s.first == slot)
            return i;
        i++;
    }

    throw std::invalid_argument("Undefined slot `" + slot + "' in struct `" + name + "'");
    return 0;
}

