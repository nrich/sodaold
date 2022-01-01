#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <iostream>

enum class SimpleType {
    NONE,
    UNDEFINED,
    SCALAR
};

typedef std::variant<struct Struct, struct Array, struct String, SimpleType> ValueType;

struct String {
    std::string literal;
    size_t allocated;

    String() : literal(""), allocated(0) {
    }

    String(const std::string &value) : literal(value), allocated(value.size()+1) {
    }

    String(size_t allocated) : literal(""), allocated(allocated) {
    }

    bool operator==(const String &rhs) const {
        return true;
    }

    bool operator!=(const String &rhs) const {
        return false;
    }
};

struct Array {
    std::shared_ptr<ValueType> type;
    size_t length;
    size_t offset;

    Array(ValueType type, size_t length, size_t offset);

    bool operator==(const Array &rhs) const;
    bool operator!=(const Array &rhs) const;

    ValueType getType() const;
    ValueType getStoredType() const;

    size_t size() const;
};

struct Struct {
    std::string name;
    std::vector<std::pair<std::string, ValueType>> slots;
    size_t size() const {
        return slots.size();
    }

    Struct(const std::string &name, std::vector<std::pair<std::string, ValueType>> slots) : name(name), slots(slots) {
    }

    bool operator==(const Struct &rhs) const;

    bool operator!=(const Struct &rhs) const;

    ValueType getType(const std::string &slot) const;

    uint32_t getOffset(const std::string &slot) const;
};

struct Function {
    const std::string name;
    const std::vector<std::pair<std::string, ValueType>> params;
    ValueType returnType;

    Function(const std::string &name, std::vector<std::pair<std::string, ValueType>> params, ValueType returnType) : name(name), params(params), returnType(returnType) {
    }
};

class Environment {
    private:
        std::map<const std::string, std::pair<uint32_t, ValueType>> vars;
        std::map<const std::string, Function> functions;
        std::map<const std::string, Struct> structs;
        std::shared_ptr<Environment> parent;
        const int32_t offset;

        const std::string functionName;

        int localBlocks = 0;
        int32_t stringTableOffset = 256;
        Environment(int32_t offset) : parent(NULL), offset(offset), functionName("") {
        }

        Environment(std::shared_ptr<Environment> parent, const std::string &functionName) : parent(parent), offset(0), functionName(functionName) {
        }

        Environment(std::shared_ptr<Environment> parent, int32_t offset, const std::string &functionName) : parent(parent), offset(offset), functionName(functionName) {
        }

    public:
        static std::shared_ptr<Environment> createGlobal(int32_t offset) {
            return std::shared_ptr<Environment>(new Environment(offset));
        }

        std::shared_ptr<Environment> beginScope(const std::string &functionName, std::shared_ptr<Environment> parent) {
            return std::shared_ptr<Environment>(new Environment(parent, functionName));
        }

        std::shared_ptr<Environment> beginScope(std::shared_ptr<Environment> parent) {
            return std::shared_ptr<Environment>(new Environment(parent, parent->Offset()+parent->size()+parent->localBlocks, parent->functionName));
        }

        std::shared_ptr<Environment> endScope() {
            auto localBlock = std::to_string(parent->localBlocks);
            parent->localBlocks += this->size();
            return parent;
        }

        Struct defineStruct(const std::string &name, std::vector<std::pair<std::string, ValueType>> slotlist) {
            auto _struct = Struct(name, slotlist);
            structs.insert(std::make_pair(name, _struct));
            return _struct;
        }

        Function defineFunction(const std::string &name, std::vector<std::pair<std::string, ValueType>> params, ValueType returnType) {
            auto function = Function(name, params, returnType);
            functions.emplace(name, function);
            return function;
        }

        void updateStruct(const std::string &name, const Struct &_struct) {
            structs.erase(name);
            structs.emplace(name, _struct);
        }

        void updateFunction(const std::string &name, const Function &function) {
            functions.erase(name);
            functions.emplace(name, function);
        }

        Struct getStruct(const std::string &name) const {
            auto found = structs.find(name);
            if (found != structs.end()) {
                return found->second;
            } else {
                if (parent) {
                    return parent->getStruct(name);
                } else {
                    throw std::invalid_argument("Undefined struct `" + name + "'");
                }
            }
        }

        Function getFunction(const std::string &name) const {
            auto found = functions.find(name);
            if (found != functions.end()) {
                return found->second;
            } else {
                if (parent) {
                    return parent->getFunction(name);
                } else {
                    throw std::invalid_argument("Undefined function `" + name + "'");
                }
            }
        }

        const size_t Offset() const {
            return offset;
        }

        const size_t size() const {
            return vars.size();
        }

        uint32_t get(const std::string &name) const {
            auto found = vars.find(name);

            if (found != vars.end()) {
                return found->second.first;
            } else {
                if (parent) {
                    return parent->get(name);
                } else {
                    throw std::invalid_argument("Unknown variable `" + name + "'");
                }
            }
        }

        ValueType getType(const std::string &name) const {
            auto found = vars.find(name);

            if (found != vars.end()) {
                return found->second.second;
            } else {
                if (parent) {
                    return parent->getType(name);
                } else {
                    throw std::invalid_argument("Unknown variable `" + name + "'");
                }
            }
        }


        bool isStruct(const std::string &name) {
            auto found = structs.find(name);

            if (found != structs.end()) {
                return true;
            } else if (parent) {
                return parent->isStruct(name);
            }

            return false;
        }

        bool isFunction(const std::string &name) {
            auto found = functions.find(name);

            if (found != functions.end()) {
                return true;
            } else if (parent) {
                return parent->isFunction(name);
            }

            return false;
        }

        bool isVariable(const std::string &name) const {
            auto found = vars.find(name);

            if (found != vars.end()) {
                return true;
            }

            if (parent) {
                return parent->isVariable(name);
            }

            return false;
        }


        bool isGlobal(const std::string &name) {
            auto found = vars.find(name);

            if (found != vars.end()) {
                return !inFunction();
            }

            if (parent) {
                return parent->isGlobal(name);
            } else {
                if (found == vars.end())
                    throw std::invalid_argument("Unknown variable `" + name + "'");
                return true;
            }
        }

        int32_t create(const std::string &name, ValueType type, size_t count=1) {
            auto existing = vars.find(name);
            if (existing != vars.end()) {
                return existing->second.first;
            }

            int32_t next = Offset() + vars.size() + localBlocks;
            for (size_t i = 0; i  < count; i++) {
                vars.insert(std::make_pair(name + std::string(i, ' '), std::make_pair(next+i, type)));
            }

            //std::cerr << name << "@" << next << std::endl;

            return next;
        }

        int32_t defineString(const std::string &value) {
            if (parent) {
                return parent->defineString(value);
            } else {
                //int32_t next = Offset() + vars.size() + localBlocks;
                //localBlocks += value.size() + 1;
                int32_t next = stringTableOffset;
                stringTableOffset += value.size() + 1;
                return next;
            }
       }

        uint32_t set(const std::string &name, ValueType type) {
            auto found = vars.find(name);

            if (found != vars.end()) {
                //vars[name] = std::make_pair(found->second.first, type);
                found->second.second = type;
                return found->second.first;
            } else {
                if (parent) {
                    return parent->get(name);
                } else {
                    throw std::invalid_argument("Unknown variable `" + name + "'");
                }
            }
        }

        std::shared_ptr<Environment> Parent() const {
            return parent;
        }


        bool inFunction() const {
            return functionName.size() > 0;
        }
};

std::string ValueTypeToString(ValueType type);

#endif //__ENVIRONMENT_H__
