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

static const int32_t StackFrameSize = 32;

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

    if (token.str == "abs") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `abs': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPA);
        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETB, Int16AsValue(0));
        } else {
            addValue32(asmTokens, OpCode::SETB, Int32AsValue(0));
        }
        add(asmTokens, OpCode::CMP);
        add(asmTokens, OpCode::MOVCB);
        add(asmTokens, OpCode::MUL);
        add(asmTokens, OpCode::PUSHC);

        return Scalar;
    } else if (token.str == "atan") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `atan': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::ATAN);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "cls") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::CLS, RuntimeValue::NONE);
        return None;
    } else if (token.str == "cos") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `cos': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::COS);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "drawbox") {
        const int DrawBoxArgs = 6;
        const std::string DrawBoxIndex = " DRAWBOX";

        add(asmTokens, OpCode::PUSHIDX);

        if (env->inFunction()) {
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(env->create(DrawBoxIndex, Scalar, DrawBoxArgs)));
            } else {
                addValue32(asmTokens, OpCode::MOVIDX, Int32AsValue(env->create(DrawBoxIndex, Scalar, DrawBoxArgs)));
            }
        } else {
            addPointer(asmTokens, OpCode::SETIDX, env->create(DrawBoxIndex, Scalar, DrawBoxArgs));
        }

        add(asmTokens, OpCode::PUSHIDX);

        for (int i = 0; i < DrawBoxArgs; i++) {
            add(asmTokens, OpCode::PUSHIDX);
            auto type = expression(cpu, asmTokens, tokens, 0);
            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
            }

            if (i != (DrawBoxArgs-1))
                check(tokens[current++], TokenType::COMMA, "`,' expected");
        }

        add(asmTokens, OpCode::POPIDX);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::DRAWBOX, RuntimeValue::IDX);

        add(asmTokens, OpCode::POPIDX);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        return None;
    } else if (token.str == "drawline") {
        const int DrawLineArgs = 5;
        const std::string DrawLineIndex = " DRAWLINE";

        add(asmTokens, OpCode::PUSHIDX);

        if (env->inFunction()) {
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(env->create(DrawLineIndex, Scalar, DrawLineArgs)));
            } else {
                addValue32(asmTokens, OpCode::MOVIDX, Int32AsValue(env->create(DrawLineIndex, Scalar, DrawLineArgs)));
            }
        } else {
            addPointer(asmTokens, OpCode::SETIDX, env->create(DrawLineIndex, Scalar, DrawLineArgs));
        }

        add(asmTokens, OpCode::PUSHIDX);

        for (int i = 0; i < DrawLineArgs; i++) {
            add(asmTokens, OpCode::PUSHIDX);
            auto type = expression(cpu, asmTokens, tokens, 0);
            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
            }

            if (i != (DrawLineArgs-1))
                check(tokens[current++], TokenType::COMMA, "`,' expected");
        }

        add(asmTokens, OpCode::POPIDX);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::DRAWLINE, RuntimeValue::IDX);

        add(asmTokens, OpCode::POPIDX);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        return None;
    } else if (token.str == "drawpixel") {
        auto x_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto y_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto c_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (x_type == None || x_type == Undefined)
            error("Function `sdrawpixel': Cannot assign a void value to parameter 1");
        if (y_type == None || y_type == Undefined)
            error("Function `drawpixel': Cannot assign a void value to parameter 2");
        if (c_type == None || c_type == Undefined)
            error("Function `drawpixel': Cannot assign a void value to parameter 3");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::DRAW, RuntimeValue::NONE);
        return None;
    } else if (token.str == "float") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `float': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::FLT);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "int") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `int': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::INT);
        add(asmTokens, OpCode::PUSHC);
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
    } else if (token.str == "keypressed") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `keypressed': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::KEYSET, RuntimeValue::C);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "log") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `log': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::LOG);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "make") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error("Function `make': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::CALLOC);
        return Scalar;
    } else if (token.str == "max") {
        static int MAXs = 1;
        int _max = MAXs++;

        auto left_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto right_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error("Function `max': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error("Function `max': Cannot assign a void value to parameter 2");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::GT);

        add(asmTokens, OpCode::JMPNZ, "MAX_" + std::to_string(_max) + "_TRUE");
        add(asmTokens, OpCode::PUSHB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::PUSHA, "MAX_" + std::to_string(_max) + "_TRUE");
        return Scalar;
    } else if (token.str == "min") {
        static int MINs = 1;
        int _min = MINs++;

        auto left_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto right_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error("Function `min': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error("Function `min': Cannot assign a void value to parameter 2");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LT);

        add(asmTokens, OpCode::JMPNZ, "MIN_" + std::to_string(_min) + "_TRUE");
        add(asmTokens, OpCode::PUSHB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::PUSHA, "MIN_" + std::to_string(_min) + "_TRUE");
        return Scalar;
    } else if (token.str == "puts") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error("Function `puts': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::WRITE, RuntimeValue::C);
        return None;
    } else if (token.str == "rand") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETC, Int16AsValue(1));
        } else {
            addValue32(asmTokens, OpCode::SETC, Int32AsValue(1));
        }

        add(asmTokens, OpCode::RND);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "setpalette") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `setpalette': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::PALETTE, RuntimeValue::C);
        return None;
    } else if (token.str == "sin") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `sin': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::SIN);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "sound") {
        auto frequency_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto duration_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto voice_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (frequency_type == None || frequency_type == Undefined)
            error("Function `sound': Cannot assign a void value to parameter 1");
        if (duration_type == None || duration_type == Undefined)
            error("Function `sound': Cannot assign a void value to parameter 2");
        if (voice_type == None || voice_type == Undefined)
            error("Function `sound': Cannot assign a void value to parameter 3");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::SOUND, RuntimeValue::NONE);

        return None;
    } else if (token.str == "sqrt") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `sqrt': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::SQR);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "srand") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `srand': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::SEED);
        return None;
    } else if (token.str == "tan") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error("Function `tan': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::TAN);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.str == "voice") {
        const int VoiceArgs = 6;
        const std::string VoiceIndex = " VOICE";

        add(asmTokens, OpCode::PUSHIDX);

        if (env->inFunction()) {
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(env->create(VoiceIndex, Scalar, VoiceArgs)));
            } else {
                addValue32(asmTokens, OpCode::MOVIDX, Int32AsValue(env->create(VoiceIndex, Scalar, VoiceArgs)));
            }
        } else {
            addPointer(asmTokens, OpCode::SETIDX, env->create(VoiceIndex, Scalar, VoiceArgs));
        }

        add(asmTokens, OpCode::PUSHIDX);

        for (int i = 0; i < VoiceArgs; i++) {
            add(asmTokens, OpCode::PUSHIDX);
            auto type = expression(cpu, asmTokens, tokens, 0);
            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(1));
            }

            check(tokens[current++], TokenType::COMMA, "`,' expected");
        }

        expression(cpu, asmTokens, tokens, 0);
        add(asmTokens, OpCode::POPC);

        add(asmTokens, OpCode::POPIDX);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::VOICE, RuntimeValue::C);

        add(asmTokens, OpCode::POPIDX);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        return None;
    } else if (token.str == "vsync") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        add(asmTokens, OpCode::YIELD);
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
            auto params = function.params.size();
            current++;

            size_t argcount = 0;
            check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

            if (tokens[current].type != TokenType::RIGHT_PAREN) {
                if (argcount >= params)
                    error("Function `" + name + "': Too many arguments, expected " + (params ? std::to_string(params) : "none"));

                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(std::string("Function `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                auto param = function.params[argcount];
                auto paramType = param.second;

                if (paramType != Scalar && paramType != type) {
                    if (std::holds_alternative<Array>(paramType)) {
                        error(std::string("Function `") + name + "': Expected array for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<Struct>(paramType)) {
                        auto expected = std::get<Struct>(paramType);
                        error(std::string("Function `") + name + "': Expected struct type " + expected.name + " for parameter " + std::to_string(argcount+1));
                    }
                }

                argcount++;
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                if (argcount >= params)
                    error("Function `" + name + "': Too many arguments, expected " + (params ? std::to_string(params) : "none"));

                check(tokens[current++], TokenType::COMMA, "`,' expected");

                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(std::string("Function `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                auto param = function.params[argcount];
                auto paramType = param.second;

                if (paramType != Scalar && paramType != type) {
                    if (std::holds_alternative<Array>(paramType)) {
                        error(std::string("Function `") + name + "': Expected array for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<Struct>(paramType)) {
                        auto expected = std::get<Struct>(paramType);
                        error(std::string("Function `") + name + "': Expected struct type " + expected.name + " for parameter " + std::to_string(argcount+1));
                    }
                }

                argcount++;
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != function.params.size()) {
                error(std::string("Function `") + name + "' expected " + std::to_string(function.params.size()) + " arguments, got " + std::to_string(argcount));
            }

            add(asmTokens, OpCode::CALL, str_toupper(name));

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
                if (argcount >= slots)
                    error("Struct `" + name + "': Too many arguments, expected " + (slots ? std::to_string(slots) : "none"));

                auto param = _struct.slots[argcount];
                auto paramType = param.second;

                add(asmTokens, OpCode::PUSHIDX);
                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(std::string("Struct `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                if (paramType != Scalar && paramType != type) {
                    if (std::holds_alternative<Array>(paramType)) {
                        error(std::string("Struct `") + name + "': Expected array for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<Struct>(paramType)) {
                        auto expected = std::get<Struct>(paramType);
                        error(std::string("Struct `") + name + "': Expected struct type " + expected.name + " for parameter " + std::to_string(argcount+1));
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
                if (argcount >= slots)
                    error("Struct `" + name + "': Too many arguments, expected " + (slots ? std::to_string(slots) : "none"));

                check(tokens[current++], TokenType::COMMA, "`,' expected");

                auto param = _struct.slots[argcount];
                auto paramType = param.second;

                add(asmTokens, OpCode::PUSHIDX);
                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(std::string("Struct `") + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                if (paramType != Scalar && paramType != type) {
                    if (std::holds_alternative<Array>(paramType)) {
                        error(std::string("Struct `") + name + "': Expected array for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<Struct>(paramType)) {
                        auto expected = std::get<Struct>(paramType);
                        error(std::string("Struct `") + name + "': Expected struct type " + expected.name + " for parameter " + std::to_string(argcount+1));
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

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READC, Int16AsValue(env->get(token.str)));
            } else {
                addValue32(asmTokens, OpCode::READC, Int32AsValue(env->get(token.str)));
            }

            add(asmTokens, OpCode::PUSHC);
            return type;
        }
    } else {
        error(std::string("value expected, got `") + token.str + "'");
    }
    return None;
}

static Array parseArray(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    check(tokens[current++], TokenType::LEFT_BRACKET, "`[' expected");

    if (tokens[current].type == TokenType::LEFT_BRACKET) {
        auto array = parseArray(cpu, asmTokens, tokens);

        current++;

        int length = 1;
        while (tokens[current].type != TokenType::RIGHT_BRACKET) {
            check(tokens[current++], TokenType::COMMA, "`,' expected");

            if (parseArray(cpu, asmTokens, tokens) != array)
                error("Array mismatch");

            current++;
            length++;
        }

        check(tokens[current], TokenType::RIGHT_BRACKET, "`]' expected");

        return Array(array, length, array.offset*array.length);
    } else {
        auto type = expression(cpu, asmTokens, tokens, 0);
        int length = 1;

        while (tokens[current].type != TokenType::RIGHT_BRACKET) {
            check(tokens[current++], TokenType::COMMA, "`,' expected");

            if (expression(cpu, asmTokens, tokens, 0) != type)
                error("Array mismatch");

            length++;
        }

        check(tokens[current], TokenType::RIGHT_BRACKET, "`]' expected");

        return Array(type, length, 1);
    }
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
    } else if (tokens[current].type == TokenType::INCREMENT) {
        current++;
        auto name = identifier(tokens[current]);

        if (!env->isVariable(name))
            error("Variable expected");

        auto type = prefix(cpu, asmTokens, tokens, rbp);

        if (type != Scalar)
            error("Scalar expected");

        add(asmTokens, OpCode::POPC);

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));
        } else {
            addValue32(asmTokens, OpCode::INCC, Int32AsValue(1));
        }

        if (env->inFunction()) {
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->get(name)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->get(name)));
            }
        } else {
            addPointer(asmTokens, OpCode::STOREC, env->get(name));
        }

        add(asmTokens, OpCode::PUSHC);

        return type;
    } else if (tokens[current].type == TokenType::DECREMENT) {
        current++;
        auto name = identifier(tokens[current]);

        if (!env->isVariable(name))
            error("Variable expected");

        auto type = prefix(cpu, asmTokens, tokens, rbp);

        if (type != Scalar)
            error("Scalar expected");

        add(asmTokens, OpCode::POPC);

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));
        } else {
            addValue32(asmTokens, OpCode::INCC, Int32AsValue(-1));
        }

        if (env->inFunction()) {
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->get(name)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->get(name)));
            }
        } else {
            addPointer(asmTokens, OpCode::STOREC, env->get(name));
        }

        add(asmTokens, OpCode::PUSHC);

        return type;

    } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
        auto array = parseArray(cpu, asmTokens, tokens);

        addShort(asmTokens, OpCode::ALLOC, array.size());

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(array.size()));
        } else {
            addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(array.size()));
        }

        for (size_t i = 0; i < array.size(); i++) {
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(-1));
            } else {
                addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(-1));
            }

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::WRITECX);
        }

        add(asmTokens, OpCode::PUSHIDX);

        return array;
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
    } else if (token.type == TokenType::BACKSLASH) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::IDIV);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::LEFT_BRACKET) {
        auto varType = lType;

        if (!std::holds_alternative<Array>(varType))
            error("Array expected");

        auto array = std::get<Array>(varType);

        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETB, Int16AsValue(array.offset));
        } else {
            addValue32(asmTokens, OpCode::SETB, Int32AsValue(array.offset));
        }

        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::MUL);
        add(asmTokens, OpCode::PUSHC);

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::ADD);
        add(asmTokens, OpCode::PUSHC);

        if (!std::holds_alternative<Array>(array.getType())) {
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::IDXC);
            add(asmTokens, OpCode::PUSHC);
        }

        return array.getType();
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
    } else if (token.type == TokenType::EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::EQ);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::NOT_EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::NE);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::LESS) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LT);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::LESS_EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LE);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::GREATER) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::GT);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::GREATER_EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::GE);
        add(asmTokens, OpCode::PUSHC);
        return Scalar;
    } else if (token.type == TokenType::AND) {
        static int ANDs = 1;
        int _and = ANDs++;

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::JMPEZ, "AND_" + std::to_string(_and) + "_FALSE");
        add(asmTokens, OpCode::PUSHC);

        auto type = expression(cpu, asmTokens, tokens, token.lbp);

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHC, "AND_" + std::to_string(_and) + "_FALSE");

        return type;
    } else if (token.type == TokenType::OR) {
        static int ORs = 1;

        int _or = ORs++;

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::JMPNZ, "OR_" + std::to_string(_or) + "_TRUE");

        auto type = expression(cpu, asmTokens, tokens, token.lbp);

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHC, "OR_" + std::to_string(_or) + "_TRUE");

        return type;
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

        std::stack<int> dimensions;
        dimensions.push(size);

        while (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;

            check(tokens[current], TokenType::INTEGER, "integer expected");

            auto dim = std::stoi(tokens[current++].str);
            dimensions.push(dim);

            size *= dim;

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
        }

        VariableType type = Scalar;
        if (tokens[current].type == TokenType::COLON) {
            current++;
            auto type_name = identifier(tokens[current++]);

            if (!env->isStruct(type_name))
                error(type_name + " does not name a struct");

            type = env->getStruct(type_name);
        }

        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

        int offset = 1;
        while (dimensions.size()) {
            auto dim = dimensions.top();
            type = Array(type, dim, offset);
            dimensions.pop();
            offset *= dim;
        }

        if (env->inFunction()) {
            addShort(asmTokens, OpCode::ALLOC, size);
            add(asmTokens, OpCode::PUSHIDX);
            add(asmTokens, OpCode::POPC);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->create(name, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->create(name, type)));
            }
        } else {
            addShort(asmTokens, OpCode::ALLOC, size);
            addPointer(asmTokens, OpCode::SAVEIDX, env->create(name, type));
        }
    } else if (tokens[current].type == TokenType::ASSIGN) {
        current++;

        if (env->inFunction()) {
            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + name + "'");

            add(asmTokens, OpCode::POPC);
            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->create(name, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->create(name, type)));
            }
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
static VariableType declaration(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens);

static void if_statment(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    static int IFs = 1;
    int _if = IFs++;

    check(tokens[current++], TokenType::IF, "`if' expected");
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    auto type = expression(cpu, asmTokens, tokens);
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    add(asmTokens, OpCode::POPC);
    add(asmTokens, OpCode::JMPEZ, "IF_" + std::to_string(_if) + "_FALSE");

    declaration(cpu, asmTokens, tokens);

    if (tokens[current].type == TokenType::ELSE) {
        add(asmTokens, OpCode::JMP, "IF_" + std::to_string(_if) + "_TRUE");
        add(asmTokens, OpCode::NOP, "IF_" + std::to_string(_if) + "_FALSE");
        current++;
        declaration(cpu, asmTokens, tokens);
        add(asmTokens, OpCode::NOP, "IF_" + std::to_string(_if) + "_TRUE");
    } else {
        add(asmTokens, OpCode::NOP, "IF_" + std::to_string(_if) + "_FALSE");
    }
}

static std::string LOOP_BREAK = "";
static std::string LOOP_CONTINUE = "";

static void while_statment(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    static int WHILEs = 1;
    int _while = WHILEs++;

    check(tokens[current++], TokenType::WHILE, "`while' expected");
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    add(asmTokens, OpCode::NOP, "WHILE_" + std::to_string(_while) + "_CHECK");
    auto type = expression(cpu, asmTokens, tokens);
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    add(asmTokens, OpCode::POPC);
    add(asmTokens, OpCode::JMPEZ, "WHILE_" + std::to_string(_while) + "_FALSE");

    auto old_break = LOOP_BREAK;
    auto old_continue = LOOP_CONTINUE;

    LOOP_BREAK = "WHILE_" + std::to_string(_while) + "_FALSE";
    LOOP_CONTINUE = "WHILE_" + std::to_string(_while) + "_CHECK";

    declaration(cpu, asmTokens, tokens);

    LOOP_BREAK = old_break;
    LOOP_CONTINUE = old_continue;

    add(asmTokens, OpCode::JMP, "WHILE_" + std::to_string(_while) + "_CHECK");

    add(asmTokens, OpCode::NOP, "WHILE_" + std::to_string(_while) + "_FALSE");
}

static void for_statment(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    static int FORs = 1;
    int _for = FORs++;

    check(tokens[current++], TokenType::FOR, "`for' expected");
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    if (tokens[current].type == TokenType::SEMICOLON) {
        current++;
        add(asmTokens, OpCode::NOP);
    } else {
        declaration(cpu, asmTokens, tokens);
    }

    add(asmTokens, OpCode::NOP, "FOR_" + std::to_string(_for) + "_CHECK");
    expression(cpu, asmTokens, tokens);
    add(asmTokens, OpCode::POPC);
    add(asmTokens, OpCode::JMPEZ, "FOR_" + std::to_string(_for) + "_FALSE");
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    add(asmTokens, OpCode::JMP, "FOR_" + std::to_string(_for) + "_BODY");

    add(asmTokens, OpCode::NOP, "FOR_" + std::to_string(_for) + "_POST");
    statement(cpu, asmTokens, tokens);
    add(asmTokens, OpCode::JMP, "FOR_" + std::to_string(_for) + "_CHECK");
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    auto old_break = LOOP_BREAK;
    auto old_continue = LOOP_CONTINUE;

    LOOP_BREAK = "FOR_" + std::to_string(_for) + "_FALSE";
    LOOP_CONTINUE = "FOR_" + std::to_string(_for) + "_CHECK";

    add(asmTokens, OpCode::NOP, "FOR_" + std::to_string(_for) + "_BODY");
    declaration(cpu, asmTokens, tokens);

    LOOP_BREAK = old_break;
    LOOP_CONTINUE = old_continue;

    add(asmTokens, OpCode::JMP, "FOR_" + std::to_string(_for) + "_POST");

    add(asmTokens, OpCode::NOP, "FOR_" + std::to_string(_for) + "_FALSE");
}

static VariableType parseIndexStatement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, VariableType containerType) {
    if (tokens[current].type == TokenType::LEFT_BRACKET) {
        current++;

        if (!std::holds_alternative<Array>(containerType))
            error("Array expected");

        auto array = std::get<Array>(containerType);
        auto subType = array.getType();

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::SETB, Int16AsValue(array.offset));
        } else {
            addValue32(asmTokens, OpCode::SETB, Int32AsValue(array.offset));
        }

        expression(cpu, asmTokens, tokens);

        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::MUL);
        add(asmTokens, OpCode::PUSHC);

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::ADD);

        add(asmTokens, OpCode::PUSHC);

        check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

        if (tokens[current].type != TokenType::ASSIGN) {
            return parseIndexStatement(cpu, asmTokens, tokens, subType);
        }

        return subType;
    } else if (tokens[current].type == TokenType::ACCESSOR) {
        current++;

        if (!(std::holds_alternative<Struct>(containerType)))
            error("Struct instance expected");

        auto _struct = std::get<Struct>(containerType);

        auto property = identifier(tokens[current++]);

        auto offset = _struct.getOffset(property);
        auto subType = _struct.getType(property);

        add(asmTokens, OpCode::POPC);

        if (cpu == 16) {
            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(offset));
        } else {
            addValue32(asmTokens, OpCode::INCIDX, Int32AsValue(offset));
        }

        add(asmTokens, OpCode::IDXC);

        if (tokens[current].type != TokenType::ASSIGN) {
            return parseIndexStatement(cpu, asmTokens, tokens, subType);
        }

        return subType;
    }

    return Scalar;
}

static VariableType statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::ASSIGN) {
       auto varname = tokens[current].str; 

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            add(asmTokens, OpCode::POPC);
            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            add(asmTokens, OpCode::POPC);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::PLUS_ASSIGN) {
       auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            addPointer(asmTokens, OpCode::LOADA, env->get(varname));
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::ADD);

            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));
            } else {
                addValue32(asmTokens, OpCode::READA, Int32AsValue(env->get(varname)));
            }

            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::ADD);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::MINUS_ASSIGN) {
       auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            addPointer(asmTokens, OpCode::LOADA, env->get(varname));
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::SUB);

            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));
            } else {
                addValue32(asmTokens, OpCode::READA, Int32AsValue(env->get(varname)));
            }

            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::SUB);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::STAR_ASSIGN) {
       auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            addPointer(asmTokens, OpCode::LOADA, env->get(varname));
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::MUL);

            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));
            } else {
                addValue32(asmTokens, OpCode::READA, Int32AsValue(env->get(varname)));
            }
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::MUL);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::SLASH_ASSIGN) {
       auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            addPointer(asmTokens, OpCode::LOADA, env->get(varname));
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::DIV);

            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));
            } else {
                addValue32(asmTokens, OpCode::READA, Int32AsValue(env->get(varname)));
            }
            add(asmTokens, OpCode::IDXA);

            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::DIV);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::PERCENT_ASSIGN) {
       auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            addPointer(asmTokens, OpCode::LOADA, env->get(varname));
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::MOD);

            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));
            } else {
                addValue32(asmTokens, OpCode::READA, Int32AsValue(env->get(varname)));
            }
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::MOD);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::BACKSLASH_ASSIGN) {
       auto varname = tokens[current].str;

        if (env->isFunction(varname)) {
            error("Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error("Cannot reassign struct");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            addPointer(asmTokens, OpCode::LOADA, env->get(varname));
            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::IDIV);

            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to variable `") + varname + "'");

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));
            } else {
                addValue32(asmTokens, OpCode::READA, Int32AsValue(env->get(varname)));
            }

            add(asmTokens, OpCode::POPB);

            add(asmTokens, OpCode::IDIV);

            if (cpu == 16) {
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
            } else {
                addValue32(asmTokens, OpCode::WRITEC, Int32AsValue(env->set(varname, type)));
            }
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::LEFT_BRACKET) {
        auto varname = tokens[current++].str;

        if (env->isFunction(varname)) {
            error("Cannot index function");
        } else if (env->isStruct(varname)) {
            error("Cannot index struct type");
        } else {
            auto varType = env->getType(varname);

            if (env->isGlobal(varname)) {
                addPointer(asmTokens, OpCode::LOADC, env->get(varname));
                add(asmTokens, OpCode::PUSHC);
            } else {
                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::READC, Int16AsValue(env->get(varname)));
                } else {
                    addValue32(asmTokens, OpCode::READC, Int32AsValue(env->get(varname)));
                }
                add(asmTokens, OpCode::PUSHC);
            }

            auto ltype = parseIndexStatement(cpu, asmTokens, tokens, varType);

            check(tokens[current++], TokenType::ASSIGN, "`=' expected!");

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(std::string("Cannot assign a void value to array"));

            if (type != ltype)
                error("Array type mismatch");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::ACCESSOR) {
        auto varname = tokens[current++].str;

        if (env->isFunction(varname)) {
            error("Cannot index function");
        } else if (env->isStruct(varname)) {
            error("Cannot index struct type");
        } else {
            auto varType = env->getType(varname);

            if (env->isGlobal(varname)) {
                addPointer(asmTokens, OpCode::LOADC, env->get(varname));
                add(asmTokens, OpCode::PUSHC);
            } else {
                if (cpu == 16) {
                    addValue16(asmTokens, OpCode::READC, Int16AsValue(env->get(varname)));
                } else {
                    addValue32(asmTokens, OpCode::READC, Int32AsValue(env->get(varname)));
                }
                add(asmTokens, OpCode::PUSHC);
            }

            auto ltype = parseIndexStatement(cpu, asmTokens, tokens, varType); 

            check(tokens[current++], TokenType::ASSIGN, "`=' expected");

            auto type = expression(cpu, asmTokens, tokens);
            //check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error("Cannot assign a void value to property");

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::WRITECX);
        }
    } else {
        auto type = expression(cpu, asmTokens, tokens);
        //check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        //return type;
    }
    return None;
}


static VariableType declaration(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::CONST) {
        define_const(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::AUTO) {
        define_variable(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::IF) {
        if_statment(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::WHILE) {
        while_statment(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::FOR) {
        for_statment(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::BREAK) {
        current++;
        if (LOOP_BREAK.size() == 0)
            error("Cannot break when not in loop");
        add(asmTokens, OpCode::JMP, LOOP_BREAK);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    } else if (tokens[current].type == TokenType::CONTINUE) {
        current++;
        if (LOOP_CONTINUE.size() == 0)
            error("Cannot continue when not in loop");
        add(asmTokens, OpCode::JMP, LOOP_CONTINUE);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    } else if (tokens[current].type == TokenType::LEFT_BRACE) {
        env = env->beginScope(env);
        current++;
        while (tokens[current].type != TokenType::RIGHT_BRACE) {
            auto type = declaration(cpu, asmTokens, tokens);
        }
        check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");
        env = env->endScope();
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
    } else {
        statement(cpu, asmTokens, tokens);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
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
        } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;

            std::stack<int> dimensions;
            dimensions.push(0);

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            while (tokens[current].type == TokenType::LEFT_BRACKET) {
                current++;
                check(tokens[current], TokenType::INTEGER, "integer expected");

                auto dim = std::stoi(tokens[current++].str);
                dimensions.push(dim);

                check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
            }

            VariableType type = Scalar;
            if (tokens[current].type == TokenType::COLON) {
                current++;
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(type_name + " does not name a struct");

                type = env->getStruct(type_name);
            }

            int offset = 1;
            while (dimensions.size()) {
                auto dim = dimensions.top();
                type = Array(type, dim, offset);
                dimensions.pop();
                offset *= dim;
            }

            params.push_back(std::make_pair(param, type));
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
        } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;

            std::stack<int> dimensions;
            dimensions.push(0);

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            while (tokens[current].type == TokenType::LEFT_BRACKET) {
                current++;
                check(tokens[current], TokenType::INTEGER, "integer expected");

                auto dim = std::stoi(tokens[current++].str);
                dimensions.push(dim);

                check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
            }

            VariableType type = Scalar;
            if (tokens[current].type == TokenType::COLON) {
                current++;
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(type_name + " does not name a struct");

                type = env->getStruct(type_name);
            }

            int offset = 1;
            while (dimensions.size()) {
                auto dim = dimensions.top();
                type = Array(type, dim, offset);
                dimensions.pop();
                offset *= dim;
            }

            params.push_back(std::make_pair(param, type));
        } else {
            params.push_back(std::make_pair(param, Scalar));
        }
    }
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    auto function = env->defineFunction(name, params, Undefined);

    add(asmTokens, OpCode::JMP, str_toupper(name) + "_END");
    add(asmTokens, OpCode::NOP, str_toupper(name));
    env = env->beginScope(name, env);

    auto rargs = params;
    std::reverse(rargs.begin(), rargs.end());

    if (cpu == 16) {
        addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(0));
    } else {
        addValue32(asmTokens, OpCode::MOVIDX, Int32AsValue(0));
    }

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
        auto newtype = declaration(cpu, asmTokens, tokens);

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

    env = env->endScope();

    function.returnType = type;
    env->updateFunction(name, function);

    add(asmTokens, OpCode::RETURN);
    add(asmTokens, OpCode::NOP, str_toupper(name) + "_END");
}

static Struct define_struct(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    std::vector<std::pair<std::string, VariableType>> slots;

    check(tokens[current++], TokenType::STRUCT, "`struct' expected");
    auto name = identifier(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");
    check(tokens[current++], TokenType::SLOT, "`slot' expected");
    auto slot = identifier(tokens[current++]);

    VariableType type = Scalar;
    if (tokens[current].type == TokenType::COLON) {
        current++;

        auto type_name = identifier(tokens[current++]);

        if (!env->isStruct(type_name))
            error(type_name + " does not name a struct");

        type = env->getStruct(type_name);
    } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
        current++;

        std::stack<int> dimensions;

        check(tokens[current], TokenType::INTEGER, "integer expected");
        auto dim = std::stoi(tokens[current++].str);
        dimensions.push(dim);

        check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

        while (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;
            check(tokens[current], TokenType::INTEGER, "integer expected");

            auto dim = std::stoi(tokens[current++].str);
            dimensions.push(dim);

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
        }

        if (tokens[current].type == TokenType::COLON) {
            current++;
            auto type_name = identifier(tokens[current++]);

            if (!env->isStruct(type_name))
                error(type_name + " does not name a struct");

            type = env->getStruct(type_name);
        }

        int offset = 1;
        while (dimensions.size()) {
            auto dim = dimensions.top();
            type = Array(type, dim, offset);
            dimensions.pop();
            offset *= dim;
        }
    }

    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    slots.push_back(std::make_pair(slot, type));
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        check(tokens[current++], TokenType::SLOT, "`slot' expected");
        auto slot = identifier(tokens[current++]);

        VariableType type = Scalar;

        if (tokens[current].type == TokenType::COLON) {
            current++;

            auto type_name = identifier(tokens[current++]);

            if (!env->isStruct(type_name))
                    error(type_name + " does not name a struct");

            type = env->getStruct(type_name);
        } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;

            std::stack<int> dimensions;

            check(tokens[current], TokenType::INTEGER, "integer expected");
            auto dim = std::stoi(tokens[current++].str);
            dimensions.push(dim);

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            while (tokens[current].type == TokenType::LEFT_BRACKET) {
                current++;
                check(tokens[current], TokenType::INTEGER, "integer expected");

                auto dim = std::stoi(tokens[current++].str);
                dimensions.push(dim);

                check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
            }

            if (tokens[current].type == TokenType::COLON) {
                current++;
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(type_name + " does not name a struct");

                type = env->getStruct(type_name);
            }

            int offset = 1;
            while (dimensions.size()) {
                auto dim = dimensions.top();
                type = Array(type, dim, offset);
                dimensions.pop();
                offset *= dim;
            }
        }

        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
        slots.push_back(std::make_pair(slot, type));
    }
    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

    return env->defineStruct(name, slots);
}

std::vector<AsmToken> compile(const int cpu, const std::vector<Token> &tokens) {
    std::vector<AsmToken> asmTokens;

    asmTokens.push_back(AsmToken(OpCode::NOP));

    env = Environment::createGlobal(0);

    addPointer(asmTokens, OpCode::SETC, 0);

    while (current < tokens.size()) {
        auto token = tokens[current];

        if (token.type == TokenType::EOL) {
            break;
        } else if (token.type == TokenType::DEF) {
            define_function(cpu, asmTokens, tokens);
        } else if (token.type == TokenType::STRUCT) {
            define_struct(cpu, asmTokens, tokens);
        } else {
            declaration(cpu, asmTokens, tokens);
        }
    }

    return asmTokens;
}
