#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <cstdint>
#include <map>
#include <memory>

class Environment {
    private:
        std::map<const std::string, uint32_t> vars;
        std::shared_ptr<Environment> parent;
        const int32_t offset;
    public:
        Environment(int32_t offset) : parent(NULL), offset(offset) {
        }

        Environment(std::shared_ptr<Environment> parent) : parent(parent), offset(1) {
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
