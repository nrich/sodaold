#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <cstdint>
#include <map>
#include <memory>

struct Function {
    std::string name;
    std::string end;
    std::vector<std::string> params;

    Function(const std::string &name, std::vector<std::string> params) {
        this->name = name;
        this->params = params;
    }
};

struct Struct {
    std::string name;
    std::map<std::string, uint32_t> slots;
    size_t size() const {
        return slots.size();
    }

    Struct(const std::string &name, std::vector<std::string> slotlist) : name(name) {
        for (auto slot : slotlist) {
            slots[slot] = slots.size();
        }
    }
};

class Environment {
    private:
        std::map<const std::string, uint32_t> vars;
        std::map<const std::string, Function> functions;
        std::map<const std::string, Struct> structs;
        std::shared_ptr<Environment> parent;
        const int32_t offset;
    public:
        Environment(int32_t offset) : parent(NULL), offset(offset) {
        }

        Environment(std::shared_ptr<Environment> parent) : parent(parent), offset(1) {
        }

        void defineStruct(const std::string &name, std::vector<std::string> slotlist) {
            auto _struct = Struct(name, slotlist);
            structs.insert(std::make_pair(name, _struct));
        }

        void defineFunction(const std::string &name, std::vector<std::string> params) {
            auto function = Function(name, params);
            functions.insert(std::make_pair(name, function));
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
                return found->second;
            } else {
                if (parent) {
                    return parent->get(name);
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


        bool isGlobal(const std::string &name) {
            auto found = vars.find(name);

            if (found != vars.end() && parent) {
                return false;
            }

            return true;
        }

        int32_t create(const std::string &name, size_t count=1) {
            auto existing = vars.find(name);
            if (existing != vars.end()) {
                return existing->second;
            }

            int32_t next = Offset() + vars.size();
            for (size_t i = 0; i  < count; i++) {
                vars.insert(std::make_pair(name + std::string(i, ' '), next+i));
            }
            return next;
        }

        std::shared_ptr<Environment> Parent() const {
            return parent;
        }
};

#endif //__ENVIRONMENT_H__
