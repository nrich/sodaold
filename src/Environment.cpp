#include <vector>
#include <sstream>

#include "Environment.h"

Array::Array(ValueType type, size_t length, size_t offset) : type(std::shared_ptr<ValueType>(new ValueType(type))), length(length), offset(offset) {
}

bool Array::operator==(const Array &rhs) const {
    if (!length || !rhs.length)
        return *type == *rhs.type;

    return *type == *rhs.type && length == rhs.length;
}

bool Array::operator!=(const Array &rhs) const {
    if (!length || !rhs.length)
        return *type != *rhs.type;

    return *type != *rhs.type || length != rhs.length;
}

ValueType Array::getType() const {
    return *type;
}

size_t Array::size() const {
    if (std::holds_alternative<Array>(*type)) {
        auto array = std::get<Array>(*type);

        return array.size() * length;
    } else {
        return length;
    }
}

ValueType Array::getStoredType() const {
    if (std::holds_alternative<Array>(*type)) {
        auto array = std::get<Array>(*type);
        return array.getStoredType();
    }

    return getType();
}

bool Struct::operator==(const Struct &rhs) const {
    return name == rhs.name;
}

bool Struct::operator!=(const Struct &rhs) const {
    return name != rhs.name;
}

ValueType Struct::getType(const std::string &slot) const {
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

std::string ValueTypeToString(ValueType type) {
    if (std::holds_alternative<Array>(type)) {
        std::ostringstream s;

        auto array = std::get<Array>(type);

        s << "[";

        s << array.length;

        auto type_name = ValueTypeToString(*array.type);

        s << ":" << type_name;

        s << "]";

        return s.str();
    } else if (std::holds_alternative<Struct>(type)) {
        std::ostringstream s;

        auto _struct = std::get<Struct>(type);

        s << "Struct " << _struct.name << "{";

        for (auto slot : _struct.slots) {
            s << "slot " << slot.first;

            auto type_name = ValueTypeToString(slot.second);

            if (type_name != "Scalar") {
                s << ": " << type_name;
            }

            s << ";";
        }

        s << "}";

        return s.str();
    } else if (std::holds_alternative<String>(type)) {
        auto _string = std::get<String>(type);

        return "String[" + std::to_string(_string.literal.size()) + "]";
    } else {
        std::ostringstream s;

        auto _simple = std::get<SimpleType>(type);

        switch (_simple) {
            case SimpleType::NONE:
                s << "None";
                break;
            case SimpleType::UNDEFINED:
                s << "Undefined";
                break;
            case SimpleType::POINTER:
                s << "Pointer";
                break;
            case SimpleType::FLOAT:
                s << "Float";
                break;
            case SimpleType::INTEGER:
                s << "Integer";
                break;
            case SimpleType::BYTE:
                s << "Byte";
                break;
        }

        return s.str();
    }
}
