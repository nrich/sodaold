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

static std::string function(const Token &token) {
    if (token.type != TokenType::FUNCTION && token.type != TokenType::IDENTIFIER) {
        error("Function name expected");
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
                argcount++;

                if (type == None)
                    error(std::string("Function `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                check(tokens[current++], TokenType::COMMA, "`,' expected");
                auto type = expression(cpu, asmTokens, tokens, 0);
                argcount++;

                if (type == None)
                    error(std::string("Function `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != function->params.size()) {
                error(std::string("Function `") + name + "' expected " + std::to_string(function->params.size()) + " arguments, got " + std::to_string(argcount));
            }

            add(asmTokens, OpCode::CALL, str_toupper(name));

            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX), str_toupper(name) + "_RETURN");
            add(asmTokens, OpCode::IDXA);
            add(asmTokens, OpCode::PUSHA);
            add(asmTokens, OpCode::POPIDX);
            addPointer(asmTokens, OpCode::SAVEIDX, env->get(FRAME_INDEX));

            return function->returnType;
        } else if (env->isStruct(token.str)) {
            auto name = token.str;
            auto _struct = env->getStruct(name);
            auto slots = _struct->slots.size();

            current++;
            check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

            addShort(asmTokens, OpCode::ALLOC, slots);
            add(asmTokens, OpCode::PUSHIDX);

            size_t argcount = 0;
            if (tokens[current].type != TokenType::RIGHT_PAREN) {
                auto type = expression(cpu, asmTokens, tokens, 0);
                argcount++;

                if (type == None)
                    error(std::string("Struct `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));

                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::WRITECX);
                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                } else {
                    addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
                }
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                check(tokens[current++], TokenType::COMMA, "`,' expected");
                auto type = expression(cpu, asmTokens, tokens, 0);
                argcount++;

                if (type == None)
                    error(std::string("Struct `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount));

                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::WRITECX);
                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                } else {
                    addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
                }
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != slots) {
                error(std::string("Struct `") + name + "' expected " + std::to_string(slots) + " arguments, got " + std::to_string(argcount));
            }

            return VariableType(*_struct);
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
            add(asmTokens, OpCode::IDXB);
            add(asmTokens, OpCode::PUSHB);
            add(asmTokens, OpCode::POPIDX);
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
            addValue16(asmTokens, OpCode::SETA, Int16AsValue(_struct->size()));
        } else {
            addValue32(asmTokens, OpCode::SETA, Int32AsValue(_struct->size()));
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
            addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(offset));
        }

        add(asmTokens, OpCode::IDXC);
        add(asmTokens, OpCode::PUSHC);

        return _struct.getType(property);
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

        if (env->inFunction()) {
            env->create(name, Undefined);
        } else {
            env->create(name, Undefined);
        }
    } else if (tokens[current].type == TokenType::ASSIGN) {
        current++;

        if (env->inFunction()) {

            add(asmTokens, OpCode::PUSHIDX);
            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));
            add(asmTokens, OpCode::IDXB);
            add(asmTokens, OpCode::PUSHB);

            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + name + "'");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);

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

static VariableType statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::CONST) {
        define_const(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::AUTO) {
        define_variable(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::RETURN) {
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

            add(asmTokens, OpCode::PUSHIDX);
            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));
            add(asmTokens, OpCode::IDXB);
            add(asmTokens, OpCode::PUSHB);

            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(env->set(varname, type)));
            }

            add(asmTokens, OpCode::WRITECX);
            add(asmTokens, OpCode::POPIDX);

            //return type;
        }
    } else {
        auto type = expression(cpu, asmTokens, tokens);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        //return type;
    }
    return None;
}

static void define_function(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    std::vector<std::string> params;

    check(tokens[current++], TokenType::DEF, "`def' expected");
    auto name = function(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    if (tokens[current].type != TokenType::RIGHT_PAREN) {
        auto param = identifier(tokens[current++]);
        params.push_back(param);
    }

    while (tokens[current].type != TokenType::RIGHT_PAREN) {
        check(tokens[current++], TokenType::COMMA, "`,' expected");
        auto param = identifier(tokens[current++]);
        params.push_back(param);
    }
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    add(asmTokens, OpCode::JMP, str_toupper(name) + "_END");
    add(asmTokens, OpCode::NOP, str_toupper(name));
    addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX));
    if (cpu == 16) {
        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
    } else {
        addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
    }

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
        env->create(rargs[i], None);
    }

    addPointer(asmTokens, OpCode::LOADC, env->get(FRAME_INDEX));
    add(asmTokens, OpCode::WRITECX);

    addPointer(asmTokens, OpCode::SAVEIDX, env->get(FRAME_INDEX));

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

    add(asmTokens, OpCode::POPC);
    add(asmTokens, OpCode::RETURN);
    add(asmTokens, OpCode::NOP, str_toupper(name) + "_END");
}


static void define_struct(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    std::vector<std::string> slots;
    check(tokens[current++], TokenType::STRUCT, "`struct' expected");
    auto name = identifier(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");
    check(tokens[current++], TokenType::SLOT, "`slot' expected");
    auto slot = identifier(tokens[current++]);
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    slots.push_back(slot);
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        check(tokens[current++], TokenType::SLOT, "`slot' expected");
        auto slot = identifier(tokens[current++]);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        slots.push_back(slot);
    }
    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

    env->defineStruct(name, slots);
}

std::vector<AsmToken> compile(const int cpu, const std::vector<Token> &tokens) {
    std::vector<AsmToken> asmTokens;

    asmTokens.push_back(AsmToken(OpCode::NOP));

    env = std::make_shared<Environment>(0);

    env->create(FRAME_INDEX, Scalar);

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

    asmTokens.insert(asmTokens.begin(), AsmToken(OpCode::STOREC, (int32_t)env->get(FRAME_INDEX)));
    asmTokens.insert(asmTokens.begin(), AsmToken(OpCode::SETC, (int32_t)(env->Offset() + env->size())));

    return asmTokens;
}
