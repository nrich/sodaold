#include "Compiler.h"

#include <sstream>
#include <stack>
#include <algorithm>

#include "Environment.h"

#define FRAME_INDEX "FRAME"

static size_t current = 0;

static std::shared_ptr<Environment> env;

static VariableType expression(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp);

static const VariableType None(SimpleType::NONE);
static const VariableType Undefined(SimpleType::UNDEFINED);
static const VariableType Scalar(SimpleType::SCALAR);

static const int32_t StackFrameSize = 16;

static std::string str_toupper(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return std::toupper(c); }
    );

    return s;
}

static std::string str_tolower(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return std::tolower(c); }
    );

    return s;
}

static void error(const std::string &err) {
    std::ostringstream s;
    s << "Error at position " << current << ": " << err;
    throw std::domain_error(s.str());
}

static std::string identifier(const Token &token) {
    if (token.type != TokenType::IDENTIFIER) {
        error("Identifier expected");
    }

    return token.str;
}

static void check(const Token &token, TokenType type, const std::string &err) {
    if (token.type != type)
        error(token.str + " " + err);
}

static void add(std::vector<AsmToken> &asmTokens, OpCode opcode, const std::string label="") {
    auto token = AsmToken(opcode);
    token.label = label;
    asmTokens.push_back(token);
}

static void addShort(std::vector<AsmToken> &asmTokens, OpCode opcode, int16_t v, const std::string label="") {
    auto token = AsmToken(opcode, v);
    token.label = label;
    asmTokens.push_back(token);
}

static void addPointer(std::vector<AsmToken> &asmTokens, OpCode opcode, int32_t v, const std::string label="") {
    auto token = AsmToken(opcode, v);
    token.label = label;
    asmTokens.push_back(token);
}

static void addString(std::vector<AsmToken> &asmTokens, OpCode opcode, const std::string &v, const std::string label="") {
    auto token = AsmToken(opcode,v );
    token.label = label;
    asmTokens.push_back(token);
}

static void addFloat(std::vector<AsmToken> &asmTokens, OpCode opcode, const float v, const std::string label="") {
    auto token = AsmToken(opcode, v);
    token.label = label;
    asmTokens.push_back(token);
}

static void addValue16(std::vector<AsmToken> &asmTokens, OpCode opcode, const uint32_t &v, const std::string label="") {
    auto token = AsmToken(opcode, v);
    token.label = label;
    asmTokens.push_back(token);
}

static void addValue32(std::vector<AsmToken> &asmTokens, OpCode opcode, const uint64_t &v, const std::string label="") {
    auto token = AsmToken(opcode, v);
    token.label = label;
    asmTokens.push_back(token);
}

static void addSyscall(std::vector<AsmToken> &asmTokens, OpCode opcode, SysCall syscall, RuntimeValue r, const std::string label="") {
    auto token = AsmToken(opcode, std::make_pair(syscall, r));
    token.label = label;
    asmTokens.push_back(token);
}

static uint32_t Int16AsValue(int16_t i) {
    const uint32_t QNAN = 0x7F800000;

    return (uint32_t)(QNAN|(uint16_t)i);

}

static uint64_t Int32AsValue(int32_t i) {
    const uint64_t QNAN = 0X7FFC000000000000;

    return (uint64_t)(QNAN|(uint32_t)i);
}

static VariableType builtin(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current];

    check(tokens[current+1], TokenType::LEFT_PAREN, "`(' expected");

    current += 2;

    if (token.str == "make") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error("Function `make': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::CALLOC);
        return Scalar;
    } else if (token.str == "getc") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::READKEY, RuntimeValue::C);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "gets") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error("Function `gets': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::CALLOC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::READ, RuntimeValue::IDX);
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPIDX);
        add(asmTokens, OpCode::PUSHC);

        return Scalar;
    } else if (token.str == "puts") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error("Function `puts': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::WRITE, RuntimeValue::C);
        return None;
    } else {
        error(std::string("Unknown function `") + token.str + std::string("'"));
    }
    return None;
}

static VariableType TokenAsValue(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current];

    if (token.type == TokenType::STRING) {
        addShort(asmTokens, OpCode::ALLOC, token.str.size()+1);
        addString(asmTokens, OpCode::SDATA, token.str);
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::INTEGER) {
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETC, Int16AsValue((int16_t)std::stoi(token.str)));
        } else {
            addValue32(asmTokens, OpCode::SETC, Int32AsValue((int32_t)std::stoi(token.str)));
        }
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::REAL) {
        addFloat(asmTokens, OpCode::SETC, std::stof(token.str));
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::BUILTIN) {
        return builtin(cpu, asmTokens, tokens);
    } else if (token.type == TokenType::FUNCTION) {
    } else if (token.type == TokenType::IDENTIFIER) {
        if (env->isFunction(token.str)) {
            auto name = token.str;
            auto function = env->getFunction(name);
            current++;

            size_t argcount = 0;
            check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

            if (tokens[current].type != TokenType::RIGHT_PAREN) {
                auto type = expression(cpu, asmTokens, tokens, 0);

                auto param = function.params[argcount];

                if (type == None)
                    error(std::string("Function `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));

                if (param.second != Scalar && param.second != type) {
                    auto expected = std::get<Struct>(param.second);

                    if (type == Scalar) {
                        error(std::string("Function `") + name + "': Expected struct type " + expected.name);
                    } else {
                        auto got = std::get<Struct>(type);
                        error(std::string("Function `") + name + "': Expected struct type " + expected.name + ", got " + got.name);
                    }
                }

                argcount++;
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                check(tokens[current++], TokenType::COMMA, "`,' expected");
                auto type = expression(cpu, asmTokens, tokens, 0);

                auto param = function.params[argcount];

                if (type == None)
                    error(std::string("Function `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));

                if (param.second != Scalar && param.second != type) {
                    auto expected = std::get<Struct>(param.second);

                    if (type == Scalar) {
                        error(std::string("Function `") + name + "': Expected struct type " + expected.name);
                    } else {
                        auto got = std::get<Struct>(type);
                        error(std::string("Function `") + name + "': Expected struct type " + expected.name + ", got " + got.name);
                    }
                }

                argcount++;
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != function.params.size()) {
                error(std::string("Function `") + name + "' expected " + std::to_string(function.params.size()) + " arguments, got " + std::to_string(argcount));
            }

            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(StackFrameSize));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(StackFrameSize));
            }

            addPointer(asmTokens, OpCode::SAVEIDX, env->get(FRAME_INDEX));

            add(asmTokens, OpCode::CALL, str_toupper(name));

            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX), str_toupper(name) + "_RETURN");
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(-StackFrameSize));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(-StackFrameSize));
            }

            addPointer(asmTokens, OpCode::SAVEIDX, env->get(FRAME_INDEX));

            return function.returnType;
        } else if (env->isStruct(token.str)) {
            auto name = token.str;
            auto _struct = env->getStruct(name);
            auto slots = _struct.slots.size();

            current++;
            check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

            addShort(asmTokens, OpCode::ALLOC, slots);
            add(asmTokens, OpCode::PUSHIDX);

            size_t argcount = 0;
            if (tokens[current].type != TokenType::RIGHT_PAREN) {
                auto param = _struct.slots[argcount];
                add(asmTokens, OpCode::PUSHIDX);
                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(std::string("Struct `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));

                if (param.second != Scalar && param.second != type) {
                    auto expected = std::get<Struct>(param.second);

                    if (type == Scalar) {
                        error(std::string("Struct `") + name + "': Expected struct type " + expected.name + " for position " + std::to_string(argcount));
                    } else {
                        auto got = std::get<Struct>(type);
                        error(std::string("Struct `") + name + "': Expected struct type " + expected.name + ", got " + got.name + " for position " + std::to_string(argcount));
                    }
                }


                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::WRITECX);

                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                } else {
                    addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
                }

                argcount++;
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                check(tokens[current++], TokenType::COMMA, "`,' expected");
                auto param = _struct.slots[argcount];
                add(asmTokens, OpCode::PUSHIDX);
                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(std::string("Struct `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));

                if (param.second != Scalar && param.second != type) {
                    auto expected = std::get<Struct>(param.second);

                    if (type == Scalar) {
                        error(std::string("Struct `") + name + "': Expected struct type " + expected.name + " for position " + std::to_string(argcount));
                    } else {
                        auto got = std::get<Struct>(type);
                        error(std::string("Struct `") + name + "': Expected struct type " + expected.name + ", got " + got.name + " for position " + std::to_string(argcount));
                    }
                }

                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::WRITECX);

                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                } else {
                    addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
                }

                argcount++;
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != slots) {
                error(std::string("Struct `") + name + "' expected " + std::to_string(slots) + " arguments, got " + std::to_string(argcount));
            }

            return VariableType(_struct);
        } else if (env->isGlobal(token.str)) {
            auto type = env->getType(token.str);

            if (type == Undefined)
                error(std::string("Variable `") + token.str + "' used before initialisation");

            addPointer(asmTokens, OpCode::LOADC, env->get(token.str));
            add(asmTokens, OpCode::PUSHC);
            return type;
        } else {
            auto type = env->getType(token.str);

            if (type == Undefined)
                error(std::string("Variable `") + token.str + "' used before initialisation");

            add(asmTokens, OpCode::PUSHIDX);
            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->get(token.str)));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->get(token.str)));
            }
            add(asmTokens, OpCode::IDXC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::PUSHC);
            return type;
        }
    } else {
        error(std::string("value expected, got `") + token.str + "'");
    }
    return None;
}

static VariableType prefix(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp) {
    if (tokens[current].type == TokenType::LEFT_PAREN) {
        current++;
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        return type;
    } else if (tokens[current].type == TokenType::NOT) {
        current++;
        auto type = prefix(cpu, asmTokens, tokens, rbp);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::NOT);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (tokens[current].type == TokenType::SIZEOF) {
        current++;
        auto name = tokens[current].str;
        auto _struct = env->getStruct(name);
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETA, Int16AsValue(_struct.size()));
        } else {
            addValue32(asmTokens, OpCode::SETA, Int32AsValue(_struct.size()));
        }
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (tokens[current].type == TokenType::PLUS) {
        current++;
        return prefix(cpu, asmTokens, tokens, rbp);
    } else if (tokens[current].type == TokenType::MINUS) {
        current++;
        auto type = prefix(cpu, asmTokens, tokens, rbp);
        add(asmTokens, OpCode::POPB);
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETA, Int16AsValue(0));
        } else {
            addValue32(asmTokens, OpCode::SETA, Int32AsValue(0));
        }
        add(asmTokens, OpCode::SUB);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else {
        return TokenAsValue(cpu, asmTokens, tokens);
    }
}

static VariableType Op(int cpu, std::vector<AsmToken> &asmTokens, const Token &lhs, const VariableType lType, const std::vector<Token> &tokens) {
    auto token = tokens[current++];

    if (token.type == TokenType::STAR) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::MUL);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::SLASH) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::DIV);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::PLUS) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::ADD);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::MINUS) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::SUB);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::PERCENT) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::MOD);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::LEFT_BRACKET) {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::ADD);
        add(asmTokens, OpCode::PUSHC);
        add(asmTokens, OpCode::POPIDX);
        add(asmTokens, OpCode::IDXC);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::ACCESSOR) {
        auto varType = lType;

        if (std::holds_alternative<SimpleType>(varType)) {
            error("Struct expected");
        }

        auto _struct = std::get<Struct>(varType);

        auto property = identifier(tokens[current++]);

        auto offset = _struct.getOffset(property);

        add(asmTokens, OpCode::POPIDX);

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(offset));
        } else {
            addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(offset), property);
        }

        add(asmTokens, OpCode::IDXC);
        add(asmTokens, OpCode::PUSHC);

        return _struct.getType(property);
    } else if (token.type == TokenType::AND) {
        static int ANDs = 1;
        int _and = ANDs++;

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::JMPEZ, "AND_" + std::to_string(_and) + "_FALSE");
        add(asmTokens, OpCode::PUSHC);

        auto type = expression(cpu, asmTokens, tokens, token.lbp);

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::AND);
        add(asmTokens, OpCode::PUSHC, "AND_" + std::to_string(_and) + "_FALSE");
    } else if (token.type == TokenType::OR) {
        static int ORs = 1;

        int _or = ORs++;

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::JMPNZ, "OR_" + std::to_string(_or) + "_TRUE");

        auto type = expression(cpu, asmTokens, tokens, token.lbp);

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHC, "OR_" + std::to_string(_or) + "_TRUE");

    } else {
        error(std::string("op expected, got `") + token.str + "'");
    }

    return None;
}

static VariableType expression(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp=0) {
    if (tokens.size() == 0) {
        error("Expression expected");
    }

    auto type = prefix(cpu, asmTokens, tokens, rbp);
    auto lhs = tokens[current];
    auto token = tokens[++current];

    while (rbp < token.lbp) {
        type = Op(cpu, asmTokens, lhs, type, tokens);
        token = tokens[current];
    }

    return type;
}

static void define_const(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {

}

static void define_variable(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    check(tokens[current++], TokenType::AUTO, "`auto' expected");
    auto name = identifier(tokens[current++]);

    if (tokens[current].type == TokenType::SEMICOLON) {
        current++;

        env->create(name, Undefined);
    } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
        current++;

        check(tokens[current], TokenType::INTEGER, "integer expected");
        auto size = std::stoi(tokens[current++].str);

        check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

        if (env->inFunction()) {
            add(asmTokens, OpCode::PUSHIDX);
            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->create(name, Scalar)));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->create(name, Scalar)));
            }

            add(asmTokens, OpCode::PUSHIDX);
            addShort(asmTokens, OpCode::ALLOC, size);
            add(asmTokens, OpCode::PUSHIDX);
            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);

            add(asmTokens, OpCode::WRITECX);
            add(asmTokens, OpCode::POPIDX);
        } else {
            addShort(asmTokens, OpCode::ALLOC, size);
            addPointer(asmTokens, OpCode::SAVEIDX, env->create(name, Scalar));
        }

        env->create(name, Scalar);
    } else if (tokens[current].type == TokenType::ASSIGN) {
        current++;

        if (env->inFunction()) {
            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + name + "'");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::PUSHIDX);
            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->create(name, type)));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->create(name, type)));
            }

            add(asmTokens, OpCode::WRITECX);
            add(asmTokens, OpCode::POPIDX);
        } else {
            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + name + "'");

            add(asmTokens, OpCode::POPC);
            addPointer(asmTokens, OpCode::STOREC, env->create(name, type));
        }
    } else {

    }
}

static VariableType statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens);
static void if_statment(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    static int IFs = 1;
    int _if = IFs++;

    check(tokens[current++], TokenType::IF, "`if' expected");
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    auto type = expression(cpu, asmTokens, tokens);
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");

    add(asmTokens, OpCode::POPC);
    add(asmTokens, OpCode::JMPEZ, "IF_" + std::to_string(_if) + "_FALSE");

    //VariableType type = SimpleType::NONE;
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        auto type = statement(cpu, asmTokens, tokens);
    }

    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");

    if (tokens[current].type == TokenType::ELSE) {
        add(asmTokens, OpCode::JMP, "IF_" + std::to_string(_if) + "_TRUE");
        add(asmTokens, OpCode::NOP, "IF_" + std::to_string(_if) + "_FALSE");
        current++;
        check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");

        while (tokens[current].type != TokenType::RIGHT_BRACE) {
            auto type = statement(cpu, asmTokens, tokens);
        }
        check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");
        add(asmTokens, OpCode::NOP, "IF_" + std::to_string(_if) + "_TRUE");
    } else {

        add(asmTokens, OpCode::NOP, "IF_" + std::to_string(_if) + "_FALSE");
    }
}


static void while_statment(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    static int WHILEs = 1;
    int _while = WHILEs++;

    check(tokens[current++], TokenType::WHILE, "`while' expected");
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    add(asmTokens, OpCode::NOP, "WHILE_" + std::to_string(_while) + "_CHECK");
    auto type = expression(cpu, asmTokens, tokens);
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");

    add(asmTokens, OpCode::POPC);
    add(asmTokens, OpCode::JMPEZ, "WHILE_" + std::to_string(_while) + "_FALSE");

    //VariableType type = SimpleType::NONE;
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        auto type = statement(cpu, asmTokens, tokens);
    }
    add(asmTokens, OpCode::JMP, "WHILE_" + std::to_string(_while) + "_CHECK");

    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");
    add(asmTokens, OpCode::NOP, "WHILE_" + std::to_string(_while) + "_FALSE");
}

static VariableType statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::CONST) {
        define_const(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::AUTO) {
        define_variable(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::IF) {
        if_statment(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::WHILE) {
        while_statment(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::RETURN) {
        if (!env->inFunction()) {
            error("Cannot return when not in function");
        }
        current++;
        VariableType type = None;

        if (tokens[current].type != TokenType::SEMICOLON) {
            type = expression(cpu, asmTokens, tokens);
        } else {
            add(asmTokens, OpCode::POPC);
        }
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        add(asmTokens, OpCode::RETURN);
        return type;
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::ASSIGN) {
       auto varname = tokens[current].str; 

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            add(asmTokens, OpCode::POPC);
            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
            //return type;
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::PUSHIDX);
            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->set(varname, type)));
            }

            add(asmTokens, OpCode::WRITECX);
            add(asmTokens, OpCode::POPIDX);

            //return type;
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::LEFT_BRACKET) {
        auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot index function");
        } else if (env->isStruct(varname)) {
            error("Cannot index struct type");
        } else {
            current += 2;

            expression(cpu, asmTokens, tokens);

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
            check(tokens[current++], TokenType::ASSIGN, "`=' expected");

            if (env->isGlobal(varname)) {
                addPointer(asmTokens, OpCode::LOADC, env->get(varname));
                add(asmTokens, OpCode::PUSHC);
            } else {
                add(asmTokens, OpCode::PUSHIDX);
                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->get(varname)));
                } else {
                    addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->get(varname)));
                }
                add(asmTokens, OpCode::IDXC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::PUSHC);
            }

            add(asmTokens, OpCode::POPB);
            add(asmTokens, OpCode::POPA);
            add(asmTokens, OpCode::ADD);

            add(asmTokens, OpCode::PUSHC);

            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to array"));

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::ACCESSOR) {
        auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot index function");
        } else if (env->isStruct(varname)) {
            error("Cannot index struct type");
        } else {
            auto varType = env->getType(varname);
            current += 2;
            auto property = identifier(tokens[current++]);

            if (env->isGlobal(varname)) {
                addPointer(asmTokens, OpCode::LOADC, env->get(varname));
                add(asmTokens, OpCode::PUSHC);
            } else {
                add(asmTokens, OpCode::PUSHIDX);
                addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));
                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->get(varname)));
                } else {
                    addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->get(varname)));
                }
                add(asmTokens, OpCode::IDXC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::PUSHC);
            }

            auto _struct = std::get<Struct>(varType);

            auto offset = _struct.getOffset(property);

            add(asmTokens, OpCode::POPIDX);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(offset));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(offset));
            }

            add(asmTokens, OpCode::IDXC);

            while (tokens[current].type != TokenType::ASSIGN) {
                if (tokens[current].type == TokenType::ACCESSOR) {
                    current++;

                    auto varType = _struct.getType(property);
                    auto property = identifier(tokens[current++]);

                    if (varType == Scalar)
                        break;

                    _struct = std::get<Struct>(varType);

                    auto offset = _struct.getOffset(property);

                    if (cpu == 16) {
                        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(offset));
                    } else {
                        addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(offset));
                    }

                    add(asmTokens, OpCode::IDXC);
                } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
                    current++;
                    add(asmTokens, OpCode::PUSHIDX);
                    expression(cpu, asmTokens, tokens);
                    check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

                    add(asmTokens, OpCode::POPB);
                    add(asmTokens, OpCode::POPA);
                    add(asmTokens, OpCode::ADD);
                    add(asmTokens, OpCode::PUSHC);
                    add(asmTokens, OpCode::POPIDX);
                } else {
                    error("Index operator expected");
                }
            }

            check(tokens[current++], TokenType::ASSIGN, "`=' expected");

            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to property `") + property + "'");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::WRITECX);
        }
    } else {
        auto type = expression(cpu, asmTokens, tokens);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        //return type;
    }
    return None;
}

static void define_function(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    std::vector<std::pair<std::string, VariableType>> params;

    check(tokens[current++], TokenType::DEF, "`def' expected");
    auto name = identifier(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    if (tokens[current].type != TokenType::RIGHT_PAREN) {
        auto param = identifier(tokens[current++]);
        if (tokens[current].type == TokenType::COLON) {
            current++;
            auto type_name = identifier(tokens[current++]);

            if (!env->isStruct(type_name))
                error(type_name + " does not name a struct");

            auto _struct = env->getStruct(type_name);

            params.push_back(std::make_pair(param, VariableType(_struct)));
        } else {
            params.push_back(std::make_pair(param, Scalar));
        }
    }

    while (tokens[current].type != TokenType::RIGHT_PAREN) {
        check(tokens[current++], TokenType::COMMA, "`,' expected");
        auto param = identifier(tokens[current++]);

        if (tokens[current].type == TokenType::COLON) {
            current++;
            auto type_name = identifier(tokens[current++]);

            if (!env->isStruct(type_name))
                error(type_name + " does not name a struct");

            auto _struct = env->getStruct(type_name);

            params.push_back(std::make_pair(param, VariableType(_struct)));
        } else {
            params.push_back(std::make_pair(param, Scalar));
        }
    }
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    add(asmTokens, OpCode::JMP, str_toupper(name) + "_END");
    add(asmTokens, OpCode::NOP, str_toupper(name));
    env = std::make_shared<Environment>(env);

    auto rargs = params;
    std::reverse(rargs.begin(), rargs.end());

    for (size_t i = 0; i< rargs.size(); i++) {
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::WRITECX);
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
        } else {
            addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
        }
        env->create(rargs[i].first, rargs[i].second);
    }

    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");

    VariableType type = SimpleType::NONE;
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        auto newtype = statement(cpu, asmTokens, tokens);

        if (std::holds_alternative<SimpleType>(type) && std::holds_alternative<SimpleType>(newtype)) {
            auto existing = std::get<SimpleType>(type);
            auto _new = std::get<SimpleType>(newtype);

            if (existing == SimpleType::NONE) {
                type = newtype;
            } else {
                if (existing != _new) {
                    error("Return type mismatch");
                }
            }
        } else if (std::holds_alternative<Struct>(type) && std::holds_alternative<Struct>(newtype)) {
            auto existing = std::get<Struct>(type);
            auto _new = std::get<Struct>(newtype);

            if (existing.name != _new.name) {
                error("Return type mismatch");
            }
        } else if (std::holds_alternative<SimpleType>(type) && std::holds_alternative<Struct>(newtype)) {
            auto existing = std::get<SimpleType>(type);
            if (existing == SimpleType::NONE) {
                type = newtype;
            } else {
                error("Return type mismatch");
            }
        } else {
            error("Return type mismatch");
        }
    }

    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");

    env = env->Parent();

    env->defineFunction(name, params, type);

    add(asmTokens, OpCode::RETURN);
    add(asmTokens, OpCode::NOP, str_toupper(name) + "_END");
}


static Struct define_struct(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, const std::string &prefix="") {
    std::vector<std::pair<std::string, VariableType>> slots;

    check(tokens[current++], TokenType::STRUCT, "`struct' expected");
    auto name = identifier(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");
    check(tokens[current++], TokenType::SLOT, "`slot' expected");
    auto slot = identifier(tokens[current++]);
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    slots.push_back(std::make_pair(slot, Scalar));
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        if (tokens[current].type == TokenType::SLOT) {
            current++;
            auto slot = identifier(tokens[current++]);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
            slots.push_back(std::make_pair(slot, Scalar));
        } else if (tokens[current].type == TokenType::STRUCT) {
            auto slot = identifier(tokens[current+1]);

            if (prefix.size()) {
                auto _struct = define_struct(cpu, asmTokens, tokens, prefix + "__" + name);
                slots.push_back(std::make_pair(slot, _struct));
            } else {
                auto _struct = define_struct(cpu, asmTokens, tokens, name);
                slots.push_back(std::make_pair(slot, _struct));
            }
        } else {
            error("`slot' or `struct' expected");
        }
    }
    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

    if (prefix.size()) {
        return env->defineStruct(prefix + "__" + name, slots);
    } else {
        return env->defineStruct(name, slots);
    }
}

std::vector<AsmToken> compile(const int cpu, const std::vector<Token> &tokens) {
    std::vector<AsmToken> asmTokens;

    asmTokens.push_back(AsmToken(OpCode::NOP));

    env = std::make_shared<Environment>(0);

    addPointer(asmTokens, OpCode::SETC, 0);
    addPointer(asmTokens, OpCode::STOREC, env->create(FRAME_INDEX, Scalar));

    while (current < tokens.size()) {
        auto token = tokens[current];

        if (token.type == TokenType::EOL) {
            break;
        } else if (token.type == TokenType::DEF) {
            define_function(cpu, asmTokens, tokens);
        } else if (token.type == TokenType::STRUCT) {
            define_struct(cpu, asmTokens, tokens);
        } else {
            statement(cpu, asmTokens, tokens);
        }
    }

    return asmTokens;
}
