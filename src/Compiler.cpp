#include "Compiler.h"

#include <sstream>
#include <stack>
#include <algorithm>

#include "Environment.h"

#define FRAME_INDEX "FRAME"

static size_t current = 0;

static std::shared_ptr<Environment> env;

static void expression(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp);

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

static void builtin(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current];

    check(tokens[current+1], TokenType::LEFT_PAREN, "`(' expected");

    current += 2;

    if (token.str == "make") {
        expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        add(asmTokens, OpCode::CALLOC);
    } else if (token.str == "puts") {
        expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::WRITE, RuntimeValue::C);
    } else {
        error(std::string("Unknown function `") + token.str + std::string("'"));
    }
}

static void TokenAsValue(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current];

    if (token.type == TokenType::STRING) {
        addShort(asmTokens, OpCode::ALLOC, token.str.size()+1);
        addString(asmTokens, OpCode::SDATA, token.str);
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::INTEGER) {
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETC, Int16AsValue((int16_t)std::stoi(token.str)));
        } else {
            addValue32(asmTokens, OpCode::SETC, Int32AsValue((int32_t)std::stoi(token.str)));
        }
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::REAL) {
        addFloat(asmTokens, OpCode::SETC, std::stof(token.str));
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::BUILTIN) {
        builtin(cpu, asmTokens, tokens);
    } else if (token.type == TokenType::FUNCTION) {
    } else if (token.type == TokenType::IDENTIFIER) {
        if (env->isFunction(token.str)) {
            auto name = token.str;
            current++;

            check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

            if (tokens[current].type != TokenType::RIGHT_PAREN) {
                expression(cpu, asmTokens, tokens, 0);
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                check(tokens[current++], TokenType::COMMA, "`,' expected");
                expression(cpu, asmTokens, tokens, 0);
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
            add(asmTokens, OpCode::CALL, str_toupper(name));

            addPointer(asmTokens, OpCode::LOADIDX, env->get(FRAME_INDEX), str_toupper(name) + "_RETURN");
            add(asmTokens, OpCode::IDXA);
            add(asmTokens, OpCode::PUSHA);
            add(asmTokens, OpCode::POPIDX);
            addPointer(asmTokens, OpCode::SAVEIDX, env->get(FRAME_INDEX));
        } else if (env->isGlobal(token.str)) {
            addPointer(asmTokens, OpCode::LOADC, env->get(token.str));
            add(asmTokens, OpCode::PUSHC);
        } else {
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
        }
    } else {
        error(std::string("value expected, got `") + token.str + "'");
    }
}

static void prefix(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp) {
    if (tokens[current].type == TokenType::LEFT_PAREN) {
        current++;
        expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
    } else if (tokens[current].type == TokenType::NOT) {
        current++;
        prefix(cpu, asmTokens, tokens, rbp);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::NOT);
        add(asmTokens, OpCode::PUSHC);
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
    } else if (tokens[current].type == TokenType::PLUS) {
        current++;
        prefix(cpu, asmTokens, tokens, rbp);
    } else if (tokens[current].type == TokenType::MINUS) {
        current++;
        prefix(cpu, asmTokens, tokens, rbp);
        add(asmTokens, OpCode::POPB);
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETA, Int16AsValue(0));
        } else {
            addValue32(asmTokens, OpCode::SETA, Int32AsValue(0));
        }
        add(asmTokens, OpCode::SUB);
        add(asmTokens, OpCode::PUSHC);
    } else {
        TokenAsValue(cpu, asmTokens, tokens);
    }
}

static void Op(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current++];

    if (token.type == TokenType::STAR) {
        expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::MUL);
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::SLASH) {
        expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::DIV);
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::PLUS) {
        expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::ADD);
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::MINUS) {
        expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::SUB);
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::PERCENT) {
        expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::MOD);
        add(asmTokens, OpCode::PUSHC);
    } else if (token.type == TokenType::ACCESSOR) {
        auto property = identifier(tokens[current++]);
        add(asmTokens, OpCode::POPIDX);
    } else {
        error(std::string("op expected, got `") + token.str + "'");
    }
}

static void expression(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp=0) {
    if (tokens.size() == 0) {
        error("Expression expected");
    }

    prefix(cpu, asmTokens, tokens, rbp);
    auto token = tokens[++current];

    while (rbp < token.lbp) {
        Op(cpu, asmTokens, tokens);
        token = tokens[current];
    }
}

static void define_const(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {

}

static void define_variable(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    check(tokens[current++], TokenType::AUTO, "`auto' expected");
    auto name = identifier(tokens[current++]);

    if (tokens[current].type == TokenType::SEMICOLON) {
        current++;

        addPointer(asmTokens, OpCode::LOADIDX, env->create(name));
    } else if (tokens[current].type == TokenType::ASSIGN) {
        current++;

        expression(cpu, asmTokens, tokens);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        add(asmTokens, OpCode::POPC);
        addPointer(asmTokens, OpCode::STOREC, env->create(name));
    } else {

    }
}

static void statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::CONST) {
        define_const(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::AUTO) {
        define_variable(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::RETURN) {
        current++;
        if (tokens[current].type != TokenType::SEMICOLON) {
            expression(cpu, asmTokens, tokens);
        } else {
            add(asmTokens, OpCode::POPC);
        }
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        add(asmTokens, OpCode::RETURN);
    } else {
        expression(cpu, asmTokens, tokens);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    }
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
        env->create(rargs[i]);
    }

    addPointer(asmTokens, OpCode::LOADC, env->get(FRAME_INDEX));
    add(asmTokens, OpCode::WRITECX);

    addPointer(asmTokens, OpCode::SAVEIDX, env->get(FRAME_INDEX));

    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");

    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        statement(cpu, asmTokens, tokens);
    }

    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");

    env = env->Parent();

    env->defineFunction(name, params);

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

    env->create(FRAME_INDEX);

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

    asmTokens.insert(asmTokens.begin(), AsmToken(OpCode::STOREC, env->create(FRAME_INDEX)));
    asmTokens.insert(asmTokens.begin(), AsmToken(OpCode::SETC, (int32_t)(env->Offset() + env->size())));

    return asmTokens;
}
