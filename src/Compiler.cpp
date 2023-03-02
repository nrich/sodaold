#include "Compiler.h"

#include <sstream>
#include <stack>
#include <algorithm>
#include <numeric>

#include "Environment.h"

#define FRAME_INDEX "FRAME"

static size_t current = 0;

static std::shared_ptr<Environment> env;

static ValueType expression(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp);

static const ValueType None(SimpleType::NONE);
static const ValueType Undefined(SimpleType::UNDEFINED);
static const ValueType Any(SimpleType::ANY);
static const ValueType Pointer(SimpleType::POINTER);
static const ValueType Integer(SimpleType::INTEGER);
static const ValueType Float(SimpleType::FLOAT);
static const ValueType Byte(SimpleType::BYTE);

/*
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
*/


static std::string join(std::vector<std::string> &strings, std::string delim) {
    if (strings.empty()) {
        return std::string();
    }
 
    return std::accumulate(strings.begin() + 1, strings.end(), strings[0],
        [&delim](std::string x, std::string y) {
            return x + delim + y;
        }
    );
}

static void error(const Token &token, const std::string &err) {
    std::ostringstream s;
    s << "Error at line " << token.line << " position " << token.position << ": " << err;
    throw std::domain_error(s.str());
}

static void warning(const Token &token, const std::string &warn) {
    std::ostringstream s;
    s << "Warning at " << token.line << " position " << token.position << ": " << warn;
    //throw std::domain_error(s.str());
    std::cerr << s.str() << std::endl;
}


static std::string identifier(const Token &token) {
    if (token.type != TokenType::IDENTIFIER) {
        error(token ,"Identifier expected");
    }

    return token.str;
}

static void checkTypeOrAny(const Token &token, const ValueType &type, const ValueType &check) {
    if (type == Any) {
        warning(token, "Unbound value found where " + ValueTypeToString(check) + " value or typed variable expected");
    } else if (type != check) {
        error(token, ValueTypeToString(check) + " value or typed variable expected");
    }
}

static void checkTypeOrAny(const Token &token, const ValueType &type, const std::vector<ValueType> &checks) {
    std::vector<std::string> type_names;

    std::transform(checks.begin(), checks.end(), std::back_inserter(type_names),
        [](ValueType t) { return ValueTypeToString(t); });

    if (type == Any) {
        warning(token, "Unbound value found where " + join(type_names, " or ") + " value or typed variable expected");
    } else if (std::find(checks.begin(), checks.end(), type) == checks.end()) {
        error(token, join(type_names, " or ") + " value or typed variable expected");
    }
}


static void check(const Token &token, TokenType type, const std::string &err) {
    if (token.type != type)
        error(token, token.str + " " + err);
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

static void addSyscall(std::vector<AsmToken> &asmTokens, OpCode opcode, SysCall syscall, RuntimeValue r, const std::string label="") {
    auto token = AsmToken(opcode, std::make_pair(syscall, r));
    token.label = label;
    asmTokens.push_back(token);
}

static uint32_t Int16AsValue(int16_t i) {
    const uint32_t QNAN = 0x7F800000;

    return (uint32_t)(QNAN|(uint16_t)i);
}

static uint32_t ByteAsValue(int16_t i) {
    const uint32_t QNAN = 0x7F800000;
    const uint32_t BYTE_BIT = 0x00010000;

    return (uint32_t)(QNAN|BYTE_BIT|(uint16_t)i);
}

static ValueType builtin(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current];

    check(tokens[current+1], TokenType::LEFT_PAREN, "`(' expected");

    current += 2;

    if (token.str == "abs") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `abs': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPA);

        addValue16(asmTokens, OpCode::SETB, Int16AsValue(0));
        add(asmTokens, OpCode::CMP);
        add(asmTokens, OpCode::MOVCB);
        add(asmTokens, OpCode::MUL);
        add(asmTokens, OpCode::PUSHC);

        return type;
    } else if (token.str == "atan") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `atan': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::ATAN);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "chr") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        if (type == None || type == Undefined)
            error(token, "Function `chr': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::BYT);
        add(asmTokens, OpCode::PUSHC);
        return Byte;
    } else if (token.str == "clock") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::CLOCK, RuntimeValue::C);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.str == "cls") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::CLS, RuntimeValue::NONE);
        return None;
    } else if (token.str == "cos") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `cos': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::COS);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "drawbox") {
        const int DrawBoxArgs = 6;
        const std::string DrawBoxIndex = " DRAWBOX";

        add(asmTokens, OpCode::PUSHIDX);

        if (env->inFunction()) {
            addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(env->create(DrawBoxIndex, Integer, DrawBoxArgs)));
        } else {
            addPointer(asmTokens, OpCode::SETIDX, env->create(DrawBoxIndex, Pointer, DrawBoxArgs));
        }

        add(asmTokens, OpCode::PUSHIDX);

        for (int i = 0; i < DrawBoxArgs; i++) {
            add(asmTokens, OpCode::PUSHIDX);
            auto type = expression(cpu, asmTokens, tokens, 0);

            if (type == None || type == Undefined)
                error(token, "Function `drawbox': Cannot assign a void value to parameter " + std::to_string(i+1));

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);

            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

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
            addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(env->create(DrawLineIndex, Integer, DrawLineArgs)));
        } else {
            addPointer(asmTokens, OpCode::SETIDX, env->create(DrawLineIndex, Pointer, DrawLineArgs));
        }

        add(asmTokens, OpCode::PUSHIDX);

        for (int i = 0; i < DrawLineArgs; i++) {
            add(asmTokens, OpCode::PUSHIDX);
            auto type = expression(cpu, asmTokens, tokens, 0);

            if (type == None || type == Undefined)
                error(token, "Function `drawline': Cannot assign a void value to parameter " + std::to_string(i+1));

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);

            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

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
            error(token, "Function `drawpixel': Cannot assign a void value to parameter 1");
        if (y_type == None || y_type == Undefined)
            error(token, "Function `drawpixel': Cannot assign a void value to parameter 2");
        if (c_type == None || c_type == Undefined)
            error(token, "Function `drawpixel': Cannot assign a void value to parameter 3");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::DRAW, RuntimeValue::NONE);
        return None;
    } else if (token.str == "exp") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `exp': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::EXP);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "float") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `float': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::FLT);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "free") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        add(asmTokens, OpCode::POPIDX);

        if (type == None || type == Undefined) {
            error(token, "Function `free': Cannot assign a void value to parameter 1");
        } else if (std::holds_alternative<Struct>(type)) {
            auto _struct = std::get<Struct>(type);
        } else if (std::holds_alternative<Array>(type)) {
            auto array = std::get<Array>(type);
        } else if (std::holds_alternative<String>(type)) {
            auto _string = std::get<String>(type);
        } else {
            error(token, "Function `free': Cannot free a scalar value");
        }

        add(asmTokens, OpCode::FREEIDX);

        return None;
    } else if (token.str == "getc") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::READKEY, RuntimeValue::C);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.str == "gets") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error(token, "Function `gets': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::CALLOC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::READ, RuntimeValue::IDX);
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPIDX);
        add(asmTokens, OpCode::PUSHC);

        return String();
    } else if (token.str == "int") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `int': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::INT);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.str == "keypressed") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `keypressed': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::KEYSET, RuntimeValue::C);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.str == "log") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `log': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::LOG);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "malloc") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error(token, "Function `malloc': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::CALLOC);
        add(asmTokens, OpCode::PUSHIDX);
        return Undefined;
    } else if (token.str == "max") {
        static int MAXs = 1;
        int _max = MAXs++;

        auto left_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto right_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error(token, "Function `max': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error(token, "Function `max': Cannot assign a void value to parameter 2");
        if (left_type != right_type)
            error(token, "Function `max': parameter type mismatch");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::GT);

        add(asmTokens, OpCode::JMPNZ, "MAX_" + std::to_string(_max) + "_TRUE");
        add(asmTokens, OpCode::PUSHB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::PUSHA, "MAX_" + std::to_string(_max) + "_TRUE");
        return left_type;
    } else if (token.str == "min") {
        static int MINs = 1;
        int _min = MINs++;

        auto left_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto right_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error(token, "Function `min': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error(token, "Function `min': Cannot assign a void value to parameter 2");
        if (left_type != right_type)
            error(token, "Function `min': parameter type mismatch");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LT);

        add(asmTokens, OpCode::JMPNZ, "MIN_" + std::to_string(_min) + "_TRUE");
        add(asmTokens, OpCode::PUSHB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::PUSHA, "MIN_" + std::to_string(_min) + "_TRUE");
        return left_type;
    } else if (token.str == "mouse") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::MOUSE, RuntimeValue::NONE);

        addShort(asmTokens, OpCode::ALLOC, 3);
        add(asmTokens, OpCode::PUSHIDX);

        add(asmTokens, OpCode::WRITEAX);

        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

        add(asmTokens, OpCode::WRITEBX);
        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

        add(asmTokens, OpCode::WRITECX);

        add(asmTokens, OpCode::POPIDX);

        add(asmTokens, OpCode::PUSHIDX);

        return Array(Integer, 3, 1);
    } else if (token.str == "pow") {
        auto left_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto right_type = expression(cpu, asmTokens, tokens, 0);

        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error(token, "Function `pow': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error(token, "Function `pow': Cannot assign a void value to parameter 2");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::POW);

        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "puts") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None)
            error(token, "Function `puts': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::WRITE, RuntimeValue::C);
        return None;
    } else if (token.str == "rand") {
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        addValue16(asmTokens, OpCode::SETC, Int16AsValue(1));

        add(asmTokens, OpCode::RND);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "setcolours") {
        auto left_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");
        auto right_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error(token, "Function `setcolours': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error(token, "Function `setcolours': Cannot assign a void value to parameter 2");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::COLOUR, RuntimeValue::NONE);
        return None;
    } else if (token.str == "setcursor") {
        auto left_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");
        auto right_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (left_type == None || left_type == Undefined)
            error(token, "Function `setcursor': Cannot assign a void value to parameter 1");
        if (right_type == None || right_type == Undefined)
            error(token, "Function `setcursor': Cannot assign a void value to parameter 2");

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::CURSOR, RuntimeValue::NONE);
        return None;
    } else if (token.str == "setpalette") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `setpalette': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::PALETTE, RuntimeValue::C);
        return None;
    } else if (token.str == "sin") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `sin': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::SIN);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "sound") {
        auto frequency_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto duration_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto voice_type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (frequency_type == None || frequency_type == Undefined)
            error(token, "Function `sound': Cannot assign a void value to parameter 1");
        if (duration_type == None || duration_type == Undefined)
            error(token, "Function `sound': Cannot assign a void value to parameter 2");
        if (voice_type == None || voice_type == Undefined)
            error(token, "Function `sound': Cannot assign a void value to parameter 3");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        addSyscall(asmTokens, OpCode::SYSCALL, SysCall::SOUND, RuntimeValue::NONE);

        return None;
    } else if (token.str == "sqrt") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `sqrt': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::SQR);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "srand") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `srand': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::SEED);
        return None;
    } else if (token.str == "strcat") {
        auto ltype = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        static int STRCATs = 1;

        if (ltype == None || ltype == Undefined)
            error(token, "Function `strcat': Cannot assign a void value to parameter 1");

        if (std::holds_alternative<String>(ltype)) {
            auto _string = std::get<String>(ltype);

            add(asmTokens, OpCode::POPIDX);

            if (_string.literal.size()) {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(_string.literal.size()));
                add(asmTokens, OpCode::PUSHC);
                add(asmTokens, OpCode::PUSHIDX);
            } else {
                int _strcat = STRCATs++;

                add(asmTokens, OpCode::PUSHIDX);

                addValue16(asmTokens, OpCode::SETB, Int16AsValue(0));

                add(asmTokens, OpCode::IDXA, "STRCAT_" + std::to_string(_strcat) + "_CHECK");
                add(asmTokens, OpCode::CMP);
                add(asmTokens, OpCode::JMPEZ, "STRCAT_" + std::to_string(_strcat) + "_FALSE");
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                add(asmTokens, OpCode::JMP, "STRCAT_" + std::to_string(_strcat) + "_CHECK");

                add(asmTokens, OpCode::PUSHIDX, "STRCAT_" + std::to_string(_strcat) + "_FALSE");
                add(asmTokens, OpCode::POPA);
                add(asmTokens, OpCode::POPB);
                add(asmTokens, OpCode::SUB);

                add(asmTokens, OpCode::PUSHC);
                add(asmTokens, OpCode::PUSHB, "STRCAT_" + std::to_string(_strcat) + "_SAVEIDX");
            }
        } else {
            error(token, "Function `strcat': String value expected for parameter 1");
        }

        auto rtype = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (rtype == None || rtype == Undefined)
            error(tokens[current], "Function `strcat': Cannot assign a void value to parameter 2");

        if (std::holds_alternative<String>(rtype)) {
            auto _string = std::get<String>(rtype);

            add(asmTokens, OpCode::POPIDX);

            if (_string.literal.size()) {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(_string.literal.size()));
                add(asmTokens, OpCode::PUSHC);
                add(asmTokens, OpCode::PUSHIDX);
            } else {
                int _strcat = STRCATs++;

                add(asmTokens, OpCode::PUSHIDX);

                addValue16(asmTokens, OpCode::SETB, Int16AsValue(0));

                add(asmTokens, OpCode::IDXA, "STRCAT_" + std::to_string(_strcat) + "_CHECK");
                add(asmTokens, OpCode::CMP);
                add(asmTokens, OpCode::JMPEZ, "STRCAT_" + std::to_string(_strcat) + "_FALSE");
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                add(asmTokens, OpCode::JMP, "STRCAT_" + std::to_string(_strcat) + "_CHECK");

                add(asmTokens, OpCode::PUSHIDX, "STRCAT_" + std::to_string(_strcat) + "_FALSE");
                add(asmTokens, OpCode::POPA);
                add(asmTokens, OpCode::POPB);
                add(asmTokens, OpCode::SUB);

                add(asmTokens, OpCode::PUSHC);
                add(asmTokens, OpCode::PUSHB, "STRCAT_" + std::to_string(_strcat) + "_SAVEIDX");
            }
        } else {
            error(token, "Function `strcat': String value expected for parameter 2");
        }

        // A = LEN L, B = LEN R, C = L, IDX = R
        add(asmTokens, OpCode::POPIDX);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::POPA);

        // STACK = [L,R]
        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::PUSHC);

        add(asmTokens, OpCode::ADD);
        addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));

        add(asmTokens, OpCode::CALLOC);

        // A = LEN L, B = LEN R, C = LEN N, IDX = N

        add(asmTokens, OpCode::PUSHA);
        // STACK = [LEN L,L,R]

        add(asmTokens, OpCode::POPC);
        // A = LEN L, B = LEN R, C = LEN L, IDX = N
        // STACK = [L,R]

        add(asmTokens, OpCode::POPA);
        // A = L, B = LEN R, C = LEN L, IDX = N
        // STACK = [R]

        add(asmTokens, OpCode::PUSHB);
        // STACK = [LEN R,R]

        add(asmTokens, OpCode::PUSHA);
        // STACK = [L, LEN R,R]

        add(asmTokens, OpCode::POPB);
        // A = L, B = L, C = LEN L, IDX = N
        // STACK = [LEN R,R]

        add(asmTokens, OpCode::PUSHIDX);
        // STACK = [N,LEN R,R]

        add(asmTokens, OpCode::POPA);
        // A = N, B = L, C = LEN L, IDX = N
        // STACK = [LEN R,R]

        add(asmTokens, OpCode::COPY);

        add(asmTokens, OpCode::PUSHC);
        // STACK = [LEN L,LEN R,R]

        add(asmTokens, OpCode::POPB);
        // A = N, B = LEN L, C = LEN L, IDX = N
        // STACK = [LEN R,R]

        add(asmTokens, OpCode::ADD);
        // A = N, B = LEN L, C = N+L, IDX = N

        add(asmTokens, OpCode::PUSHC);
        // STACK = [N+L,LEN R,R]

        add(asmTokens, OpCode::POPA);
        // A = N+L, B = LEN L, C = N+L, IDX = N
        // STACK = [LEN R,R]

        add(asmTokens, OpCode::POPC);
        // A = N+L, B = LEN L, C = LEN R, IDX = N
        // STACK = [R]

        add(asmTokens, OpCode::POPB);
        // A = N+L, B = R, C = LEN R, IDX = N
        // STACK = []

        add(asmTokens, OpCode::COPY);

        add(asmTokens, OpCode::PUSHIDX);
        // STACK = [N]

        return String();
    } else if (token.str == "strcmp") {
        auto ltype = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        if (ltype == None || ltype == Undefined)
            error(token, "Function `strcmp': Cannot assign a void value to parameter 1");

        if (!std::holds_alternative<String>(ltype))
            error(token, "Function `strcmp': String value expected for parameter 1");

        auto rtype = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (rtype == None || rtype == Undefined)
            error(token, "Function `strcmp': Cannot assign a void value to parameter 2");

        if (!std::holds_alternative<String>(rtype))
            error(token, "Function `strcmp': String value expected for parameter 2");

        static int STRCMPs = 1;
        int _strcmp = STRCMPs++;

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::PUSHA);
        add(asmTokens, OpCode::POPIDX);

        add(asmTokens, OpCode::EQ);
        add(asmTokens, OpCode::NOT);
        add(asmTokens, OpCode::JMPEZ, "STRCMP_" + std::to_string(_strcmp) + "_DONE");

        add(asmTokens, OpCode::CMP, "STRCMP_" + std::to_string(_strcmp) + "_CMP");
        add(asmTokens, OpCode::JMPNZ, "STRCMP_" + std::to_string(_strcmp) + "_DONE");

        add(asmTokens, OpCode::IDXC);
        add(asmTokens, OpCode::JMPEZ, "STRCMP_" + std::to_string(_strcmp) + "_DONE");

        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
        addValue16(asmTokens, OpCode::INCA, Int16AsValue(1));
        addValue16(asmTokens, OpCode::INCB, Int16AsValue(1));

        add(asmTokens, OpCode::JMP, "STRCMP_" + std::to_string(_strcmp) + "_CMP");

        add(asmTokens, OpCode::PUSHC, "STRCMP_" + std::to_string(_strcmp) + "_DONE");
        return Integer;
    } else if (token.str == "strcpy") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `strcpy': Cannot assign a void value to parameter 1");

        if (std::holds_alternative<String>(type)) {
            auto _string = std::get<String>(type);

            add(asmTokens, OpCode::POPIDX);

            if (_string.literal.size()) {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(_string.literal.size()));
                add(asmTokens, OpCode::PUSHC);
            } else {
                static int STRCPYs = 1;
                int _strcpy = STRCPYs++;

                add(asmTokens, OpCode::PUSHIDX);

                addValue16(asmTokens, OpCode::SETB, Int16AsValue(0));

                add(asmTokens, OpCode::IDXA, "STRCPY_" + std::to_string(_strcpy) + "_CHECK");
                add(asmTokens, OpCode::CMP);
                add(asmTokens, OpCode::JMPEZ, "STRCPY_" + std::to_string(_strcpy) + "_FALSE");
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                add(asmTokens, OpCode::JMP, "STRCPY_" + std::to_string(_strcpy) + "_CHECK");

                add(asmTokens, OpCode::PUSHIDX, "STRCPY_" + std::to_string(_strcpy) + "_FALSE");
                add(asmTokens, OpCode::POPA);
                add(asmTokens, OpCode::POPB);
                add(asmTokens, OpCode::SUB);

                add(asmTokens, OpCode::PUSHC);
            }

            addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));

            add(asmTokens, OpCode::PUSHIDX);
            add(asmTokens, OpCode::POPB);
            add(asmTokens, OpCode::CALLOC);
            add(asmTokens, OpCode::PUSHIDX);
            add(asmTokens, OpCode::POPA);
            add(asmTokens, OpCode::POPC);

            add(asmTokens, OpCode::COPY);

            add(asmTokens, OpCode::PUSHIDX);
        } else {
            error(tokens[current], "Function `strcpy': String value expected for parameter 1");
        }

        return String();
    } else if (token.str == "strlen") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(token, "Function `strlen': Cannot assign a void value to parameter 1");

        if (std::holds_alternative<String>(type)) {
            auto _string = std::get<String>(type);

            add(asmTokens, OpCode::POPIDX);

            if (_string.literal.size()) {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(_string.literal.size()));
                add(asmTokens, OpCode::PUSHC);
            } else {
                static int STRLENs = 1;
                int _strlen = STRLENs++;

                add(asmTokens, OpCode::PUSHIDX);

                addValue16(asmTokens, OpCode::SETB, Int16AsValue(0));

                add(asmTokens, OpCode::IDXA, "STRLEN_" + std::to_string(_strlen) + "_CHECK");
                add(asmTokens, OpCode::CMP);
                add(asmTokens, OpCode::JMPEZ, "STRLEN_" + std::to_string(_strlen) + "_FALSE");
                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
                add(asmTokens, OpCode::JMP, "STRLEN_" + std::to_string(_strlen) + "_CHECK");

                add(asmTokens, OpCode::PUSHIDX, "STRLEN_" + std::to_string(_strlen) + "_FALSE");
                add(asmTokens, OpCode::POPA);
                add(asmTokens, OpCode::POPB);
                add(asmTokens, OpCode::SUB);

                add(asmTokens, OpCode::PUSHC);
            }
        } else {
            error(token, "Function `strlen': String value expected for parameter 1");
        }

        return Integer;
    } else if (token.str == "substr") {
        auto type = expression(cpu, asmTokens, tokens, 0);

        if (type == None || type == Undefined)
            error(token, "Function `substr': Cannot assign a void value to parameter 1");

        if (!std::holds_alternative<String>(type))
            error(tokens[current], "Function `substr': String value expected for parameter 1");

        check(tokens[current++], TokenType::COMMA, "`,' expected");

        auto begin = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        if (begin == None || begin == Undefined)
            error(token, "Function `substr': Cannot assign a void value to parameter 2");

        auto len = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (len == None || len == Undefined)
            error(token, "Function `strcmp': Cannot assign a void value to parameter 3");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::PUSHC);

        addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));

        add(asmTokens, OpCode::CALLOC);
        add(asmTokens, OpCode::POPC);

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);

        add(asmTokens, OpCode::PUSHC);
        add(asmTokens, OpCode::ADD);

        add(asmTokens, OpCode::PUSHC);
        add(asmTokens, OpCode::POPB);

        add(asmTokens, OpCode::PUSHIDX);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::POPC);

        add(asmTokens, OpCode::COPY);

        add(asmTokens, OpCode::PUSHIDX);

        return String();
    } else if (token.str == "tan") {
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

        if (type == None || type == Undefined)
            error(tokens[current], "Function `tan': Cannot assign a void value to parameter 1");

        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::TAN);
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.str == "voice") {
        const int VoiceArgs = 6;
        const std::string VoiceIndex = " VOICE";

        add(asmTokens, OpCode::PUSHIDX);

        if (env->inFunction()) {
            addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(env->create(VoiceIndex, Integer, VoiceArgs)));
        } else {
            addPointer(asmTokens, OpCode::SETIDX, env->create(VoiceIndex, Pointer, VoiceArgs));
        }

        add(asmTokens, OpCode::PUSHIDX);

        for (int i = 0; i < VoiceArgs; i++) {
            add(asmTokens, OpCode::PUSHIDX);
            auto type = expression(cpu, asmTokens, tokens, 0);
            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::WRITECX);

            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

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
        error(token, "Unknown function `" + token.str + "'");
    }
    return None;
}

static std::vector<std::pair<std::string, int32_t>> StringTable;

static ValueType TokenAsValue(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    auto token = tokens[current];

    if (token.type == TokenType::STRING) {
        auto ptr = env->defineString(token.str);

        StringTable.push_back(std::make_pair(token.str, ptr));

        addPointer(asmTokens, OpCode::SETIDX, ptr);
        addString(asmTokens, OpCode::SDATA, token.str);

        add(asmTokens, OpCode::PUSHIDX);

        return String(token.str);
    } else if (token.type == TokenType::CHARACTER) {
        addValue16(asmTokens, OpCode::SETC, ByteAsValue((int8_t)token.str[0]));
        add(asmTokens, OpCode::PUSHC);
        return Byte;
    } else if (token.type == TokenType::INTEGER) {
        auto integer_type = Integer;
        if (token.str.size() > 2 && (token.str[1] == 'x' || token.str[1] == 'X')) {
            int16_t value = (int16_t)std::stoi(token.str, nullptr, 16);
            if (value < 256) {
                addValue16(asmTokens, OpCode::SETC, ByteAsValue(value));
                integer_type = Byte;
            } else {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(value));
            }
        } else if (token.str.size() > 2 && token.str[1] == 'b') {
            int16_t value = (int16_t)std::stoi(token.str.substr(2), nullptr, 2);
            if (value < 256) {
                addValue16(asmTokens, OpCode::SETC, ByteAsValue(value));
                integer_type = Byte;
            } else {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(value));
            }
        } else {
            int16_t value = (int16_t)std::stoi(token.str);
            if (value < 256) {
                addValue16(asmTokens, OpCode::SETC, ByteAsValue(value));
                integer_type = Byte;
            } else {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(value));
            }
        }

        add(asmTokens, OpCode::PUSHC);
        return integer_type;
    } else if (token.type == TokenType::REAL) {
        addFloat(asmTokens, OpCode::SETC, std::stof(token.str));
        add(asmTokens, OpCode::PUSHC);
        return Float;
    } else if (token.type == TokenType::BUILTIN || token.type == TokenType::INT || token.type == TokenType::FLOAT) {
        return builtin(cpu, asmTokens, tokens);
    } else if (token.type == TokenType::FUNCTION) {
    } else if (token.type == TokenType::IDENTIFIER) {
        if (env->isFunction(token.str)) {
            auto name = token.str;
            auto function = env->getFunction(name);
            auto params = function.params.size();
            std::vector<std::pair<std::string, ValueType>> param_types;
            std::copy(function.params.begin(), function.params.end(), std::back_inserter(param_types));
            current++;

            size_t argcount = 0;
            check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

            if (tokens[current].type != TokenType::RIGHT_PAREN) {
                if (argcount >= params)
                    error(tokens[current], "Function `" + name + "': Too many arguments, expected " + (params ? std::to_string(params) : "none"));

                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(tokens[current], "Function `" + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                auto param = param_types[argcount];
                auto paramType = param.second;

                if (paramType != type && paramType != Any) {
                     error(tokens[current], "Function `" + name + "': Expected " + ValueTypeToString(paramType) + " for parameter " + std::to_string(argcount+1) + ", got " + ValueTypeToString(type));
                }

                param_types[argcount].second = type;

                argcount++;
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                if (argcount >= params)
                    error(tokens[current], "Function `" + name + "': Too many arguments, expected " + (params ? std::to_string(params) : "none"));

                check(tokens[current++], TokenType::COMMA, "`,' expected");

                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(tokens[current], "Function `" + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                auto param = param_types[argcount];
                auto paramType = param.second;

                if (paramType != type && paramType != Any) {
                     error(tokens[current], "Function `" + name + "': Expected " + ValueTypeToString(paramType) + " for parameter " + std::to_string(argcount+1) + ", got " + ValueTypeToString(type));
                }

                param_types[argcount].second = type;

                argcount++;
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != function.params.size()) {
                error(token, "Function `" + name + "' expected " + std::to_string(function.params.size()) + " arguments, got " + std::to_string(argcount));
            }

            add(asmTokens, OpCode::CALL, name);

            if (function.returnType == Any) {
                for (const auto &param : param_types) {
                    if (param.second != None && param.second != Undefined && param.second != Any) {
                        return param.second;
                    }
                }
            }

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
                    error(tokens[current], "Struct `" + name + "': Too many arguments, expected " + (slots ? std::to_string(slots) : "none"));

                auto param = _struct.slots[argcount];
                auto paramType = param.second;

                add(asmTokens, OpCode::PUSHIDX);
                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(tokens[current], "Struct `" + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                if (paramType != Float && paramType != Integer && paramType != type) {
                    if (std::holds_alternative<Array>(paramType)) {
                        error(tokens[current], "Struct `" + name + "': Expected array for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<String>(paramType)) {
                        error(tokens[current], "Struct `" + name + "': Expected string for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<Struct>(paramType)) {
                        auto expected = std::get<Struct>(paramType);
                        error(tokens[current], "Struct `" + name + "': Expected struct type " + expected.name + " for parameter " + std::to_string(argcount+1));
                    }
                }

                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::WRITECX);

                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

                argcount++;
            }

            while (tokens[current].type != TokenType::RIGHT_PAREN) {
                if (argcount >= slots)
                    error(tokens[current], "Struct `" + name + "': Too many arguments, expected " + (slots ? std::to_string(slots) : "none"));

                check(tokens[current++], TokenType::COMMA, "`,' expected");

                auto param = _struct.slots[argcount];
                auto paramType = param.second;

                add(asmTokens, OpCode::PUSHIDX);
                auto type = expression(cpu, asmTokens, tokens, 0);

                if (type == None)
                    error(tokens[current], "Struct `" + name + "': Cannot assign a void value to parameter " + std::to_string(argcount+1));

                if (paramType != Float && paramType != Integer && paramType != type) {
                    if (std::holds_alternative<Array>(paramType)) {
                        error(tokens[current], "Struct `" + name + "': Expected array for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<String>(paramType)) {
                        error(tokens[current], "Struct `" + name + "': Expected string for parameter " + std::to_string(argcount+1));
                    } else if (std::holds_alternative<Struct>(paramType)) {
                        auto expected = std::get<Struct>(paramType);
                        error(tokens[current], "Struct `" + name + "': Expected struct type " + expected.name + " for parameter " + std::to_string(argcount+1));
                    }
                }

                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::WRITECX);

                addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));

                argcount++;
            }
            check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");

            if (argcount != slots) {
                error(token, "Struct `" + name + "' expected " + std::to_string(slots) + " arguments, got " + std::to_string(argcount));
            }

            return ValueType(_struct);
        } else if (env->isGlobal(token.str)) {
            auto type = env->getType(token.str);

            if (type == Undefined)
                error(tokens[current], "Variable `" + token.str + "' used before initialisation");

            addPointer(asmTokens, OpCode::LOADC, env->get(token.str));
            add(asmTokens, OpCode::PUSHC);

            if (tokens[current+1].type == TokenType::DECREMENT) {
                current++;
                addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));
                addPointer(asmTokens, OpCode::STOREC, env->get(token.str));
            } else if (tokens[current+1].type == TokenType::INCREMENT) {
                current++;
                addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));
                addPointer(asmTokens, OpCode::STOREC, env->get(token.str));
            }

            return type;
        } else {
            auto type = env->getType(token.str);

            if (type == Undefined)
                error(tokens[current], "Variable `" + token.str + "' used before initialisation");

            addValue16(asmTokens, OpCode::READC, Int16AsValue(env->get(token.str)));

            add(asmTokens, OpCode::PUSHC);

            if (tokens[current+1].type == TokenType::DECREMENT) {
                current++;
                addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->get(token.str)));
           } else if (tokens[current+1].type == TokenType::INCREMENT) {
                current++;
                addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));
                addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->get(token.str)));
            }

            return type;
        }
    } else {
        error(tokens[current], "value expected, got `" + token.str + "'");
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
                error(tokens[current], "Array mismatch");

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
                error(tokens[current], "Array mismatch");

            length++;
        }

        check(tokens[current], TokenType::RIGHT_BRACKET, "`]' expected");

        return Array(type, length, 1);
    }
}

static ValueType prefix(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp) {
    if (tokens[current].type == TokenType::LEFT_PAREN) {
        current++;
        auto type = expression(cpu, asmTokens, tokens, 0);
        check(tokens[current], TokenType::RIGHT_PAREN, "`)' expected");
        return type;
    } else if (tokens[current].type == TokenType::LESS) {
        current++;

        ValueType type = Undefined;

        if (tokens[current].type == TokenType::INT) {
            current++;
            check(tokens[current++], TokenType::GREATER, "`>' expected");
            type = Integer;
        } else if (tokens[current].type == TokenType::FLOAT) {
            current++;
            check(tokens[current++], TokenType::GREATER, "`>' expected");

            type = Float;
        } else {
            auto name = identifier(tokens[current++]);
            auto _struct = env->getStruct(name);
            check(tokens[current++], TokenType::GREATER, "`>' expected");

            type = _struct;
        }

        int16_t size = 1;
        std::stack<int> dimensions;

        while (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;

            check(tokens[current], TokenType::INTEGER, "integer expected");

            auto dim = std::stoi(tokens[current++].str);
            dimensions.push(dim);

            size *= dim;

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");
        }

        int offset = 1;
        while (dimensions.size()) {
            auto dim = dimensions.top();
            type = Array(type, dim, offset);
            dimensions.pop();
            offset *= dim;
        }

        auto _type = prefix(cpu, asmTokens, tokens, rbp);

        return type;
    } else if (tokens[current].type == TokenType::NOT) {
        current++;
        auto type = prefix(cpu, asmTokens, tokens, rbp);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::NOT);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (tokens[current].type == TokenType::TILDE) {
        current++;
        auto type = prefix(cpu, asmTokens, tokens, rbp);
        checkTypeOrAny(tokens[current-1], type, Integer);
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::BNOT);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (tokens[current].type == TokenType::SIZEOF) {
        current++;
        auto name = tokens[current].str;

        if (env->isStruct(name)) {
            auto _struct = env->getStruct(name);
            addValue16(asmTokens, OpCode::SETC, Int16AsValue(_struct.size()));
        } else if (env->isFunction(name)) {
            error(tokens[current], "Cannot pass function to sizeof");
        } else {
            auto type = env->getType(name);

            if (std::holds_alternative<Struct>(type)) {
                auto _struct = std::get<Struct>(type);

                addValue16(asmTokens, OpCode::SETC, Int16AsValue(_struct.size()));
            } else if (std::holds_alternative<Array>(type)) {
                auto _array = std::get<Array>(type);

                int len = _array.length ? _array.length : 1;

                addValue16(asmTokens, OpCode::SETC, Int16AsValue(len));
            } else if (std::holds_alternative<String>(type)) {
                auto _string = std::get<String>(type);

                int len = _string.literal.size() ? _string.literal.size() : 1;

                addValue16(asmTokens, OpCode::SETC, Int16AsValue(len));
            } else {
                addValue16(asmTokens, OpCode::SETC, Int16AsValue(1));
            }
        }
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (tokens[current].type == TokenType::PLUS) {
        current++;
        return prefix(cpu, asmTokens, tokens, rbp);
    } else if (tokens[current].type == TokenType::MINUS) {
        current++;
        auto type = prefix(cpu, asmTokens, tokens, rbp);
        add(asmTokens, OpCode::POPB);
        addValue16(asmTokens, OpCode::SETA, Int16AsValue(0));
        add(asmTokens, OpCode::SUB);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (tokens[current].type == TokenType::INCREMENT) {
        current++;
        auto name = identifier(tokens[current]);

        if (!env->isVariable(name))
            error(tokens[current], "Variable expected");

        auto type = prefix(cpu, asmTokens, tokens, rbp);

        if (type != Integer)
            error(tokens[current], "Integer expected");

        add(asmTokens, OpCode::POPC);

        addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));

        if (env->inFunction()) {
            addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->get(name)));
        } else {
            addPointer(asmTokens, OpCode::STOREC, env->get(name));
        }

        add(asmTokens, OpCode::PUSHC);

        return type;
    } else if (tokens[current].type == TokenType::DECREMENT) {
        current++;
        auto name = identifier(tokens[current]);

        if (!env->isVariable(name))
            error(tokens[current], "Variable expected");

        auto type = prefix(cpu, asmTokens, tokens, rbp);

        if (type != Integer)
            error(tokens[current], "Integer expected");

        add(asmTokens, OpCode::POPC);

        addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));

        if (env->inFunction()) {
            addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->get(name)));
        } else {
            addPointer(asmTokens, OpCode::STOREC, env->get(name));
        }

        add(asmTokens, OpCode::PUSHC);

        return type;

    } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
        auto array = parseArray(cpu, asmTokens, tokens);

        addShort(asmTokens, OpCode::ALLOC, array.size());

        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(array.size()));

        for (size_t i = 0; i < array.size(); i++) {
            addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(-1));

            add(asmTokens, OpCode::POPC);
            add(asmTokens, OpCode::WRITECX);
        }

        add(asmTokens, OpCode::PUSHIDX);

        return array;
    } else {
        return TokenAsValue(cpu, asmTokens, tokens);
    }
}

static ValueType Op(int cpu, std::vector<AsmToken> &asmTokens, const Token &lhs, const ValueType lType, const std::vector<Token> &tokens) {
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
        checkTypeOrAny(tokens[current-2], lType, {Integer, Byte});
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        checkTypeOrAny(tokens[current-1], type, {Integer, Byte});

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::IDIV);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::LEFT_SHIFT) {
        checkTypeOrAny(tokens[current-2], lType, {Integer, Byte});
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        checkTypeOrAny(tokens[current-1], type, {Integer, Byte});

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LSHIFT);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::RIGHT_SHIFT) {
        checkTypeOrAny(tokens[current-2], lType, {Integer, Byte});
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        checkTypeOrAny(tokens[current-1], type, {Integer, Byte});

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::RSHIFT);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::AMPERSAND) {
        checkTypeOrAny(tokens[current-2], lType, {Integer, Byte});
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        checkTypeOrAny(tokens[current-1], type, {Integer, Byte});

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::BAND);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::PIPE) {
        checkTypeOrAny(tokens[current-2], lType, {Integer, Byte});
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        checkTypeOrAny(tokens[current-1], type, {Integer, Byte});

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::BOR);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::CARAT) {
        checkTypeOrAny(tokens[current-2], lType, {Integer, Byte});
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        checkTypeOrAny(tokens[current-1], type, {Integer, Byte});

        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::XOR);
        add(asmTokens, OpCode::PUSHC);
        return type;
    } else if (token.type == TokenType::LEFT_BRACKET) {
        auto varType = lType;

        if (std::holds_alternative<Array>(varType)) {
            auto array = std::get<Array>(varType);

            auto type = expression(cpu, asmTokens, tokens, 0);
            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            addValue16(asmTokens, OpCode::SETB, Int16AsValue(array.offset));

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

                if (tokens[current].type == TokenType::DECREMENT) {
                    current++;
                    addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));
                    add(asmTokens, OpCode::WRITECX);
                } else if (tokens[current].type == TokenType::INCREMENT) {
                    current++;
                    addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));
                    add(asmTokens, OpCode::WRITECX);
                }
            }

            return array.getType();
        } else if (std::holds_alternative<String>(varType)) {
            auto _string = std::get<String>(varType);

            auto type = expression(cpu, asmTokens, tokens, 0);
            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            add(asmTokens, OpCode::POPB);
            add(asmTokens, OpCode::POPA);
            add(asmTokens, OpCode::ADD);
            add(asmTokens, OpCode::PUSHC);
            add(asmTokens, OpCode::POPIDX);
            add(asmTokens, OpCode::IDXC);
            add(asmTokens, OpCode::PUSHC);

            return Byte;
        } else {
            error(tokens[current], "Array or string expected");
        }
    } else if (token.type == TokenType::ACCESSOR) {
        auto varType = lType;

        if (std::holds_alternative<SimpleType>(varType)) {
            error(tokens[current], "Struct expected");
        }

        auto _struct = std::get<Struct>(varType);

        auto property = identifier(tokens[current++]);

        auto offset = _struct.getOffset(property);

        add(asmTokens, OpCode::POPIDX);

        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(offset));

        add(asmTokens, OpCode::IDXC);
        add(asmTokens, OpCode::PUSHC);

        if (tokens[current].type == TokenType::DECREMENT) {
            current++;
            addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));
            add(asmTokens, OpCode::WRITECX);
        } else if (tokens[current].type == TokenType::INCREMENT) {
            current++;
            addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));
            add(asmTokens, OpCode::WRITECX);
        }

        return _struct.getType(property);
    } else if (token.type == TokenType::EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::EQ);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.type == TokenType::NOT_EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::NE);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.type == TokenType::LESS) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LT);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.type == TokenType::LESS_EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::LE);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.type == TokenType::GREATER) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::GT);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
    } else if (token.type == TokenType::GREATER_EQUAL) {
        auto type = expression(cpu, asmTokens, tokens, token.lbp);
        add(asmTokens, OpCode::POPB);
        add(asmTokens, OpCode::POPA);
        add(asmTokens, OpCode::GE);
        add(asmTokens, OpCode::PUSHC);
        return Integer;
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
        error(tokens[current], "op expected, got `" + token.str + "'");
    }

    return None;
}

static ValueType expression(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, int rbp=0) {
    if (tokens.size() == 0) {
        error(tokens[current], "Expression expected");
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
    check(tokens[current++], TokenType::VAL, "`val' expected");
    auto name = identifier(tokens[current++]);

    check(tokens[current++], TokenType::ASSIGN, "`=' expected");

    auto type = expression(cpu, asmTokens, tokens);
    check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

    if (type == None)
        error(tokens[current], "Cannot assign a void value to constant value `" + name + "'");

    if (type != Byte && type != Integer && type != Float && !std::holds_alternative<String>(type))
        error(tokens[current], "Cannot assign a " + ValueTypeToString(type) + " value to constant value `" + name + "'");

    add(asmTokens, OpCode::POPC);
    if (env->inFunction()) {
        addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->createConstant(name, type)));
    } else {
        addPointer(asmTokens, OpCode::STOREC, env->createConstant(name, type));
    }
}

static void define_variable(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    check(tokens[current++], TokenType::VAR, "`var' expected");
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

        ValueType type = Any;
        if (tokens[current].type == TokenType::COLON) {
            current++;
            if (tokens[current].type == TokenType::INT) {
                current++;
                type = Integer;
            } else if (tokens[current].type == TokenType::FLOAT) {
                current++;
                type = Float;
            } else if (tokens[current].type == TokenType::STR) {
                current++;
                type = String();
            } else {
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(tokens[current], type_name + " does not name a struct");

                type = env->getStruct(type_name);
            }    
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

            addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->create(name, type)));
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
                error(tokens[current], "Cannot assign a void value to variable `" + name + "'");

            add(asmTokens, OpCode::POPC);
            addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->create(name, type)));
        } else {
            auto type = expression(cpu, asmTokens, tokens);
            check(tokens[current++], TokenType::SEMICOLON, "`;' expected");

            if (type == None)
                error(tokens[current], "Cannot assign a void value to variable `" + name + "'");

            add(asmTokens, OpCode::POPC);
            addPointer(asmTokens, OpCode::STOREC, env->create(name, type));
        }
    } else {

    }
}

static ValueType statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens);
static ValueType declaration(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens);

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

static ValueType parseIndexStatement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, ValueType containerType) {
    if (tokens[current].type == TokenType::LEFT_BRACKET) {
        current++;

        if (std::holds_alternative<Array>(containerType)) {
            auto array = std::get<Array>(containerType);
            auto subType = array.getType();

            addValue16(asmTokens, OpCode::SETB, Int16AsValue(array.offset));

            auto index_type = expression(cpu, asmTokens, tokens);
            if (index_type != Integer && index_type != Byte)
                error(tokens[current], "Integer value expected");

            add(asmTokens, OpCode::POPA);
            add(asmTokens, OpCode::MUL);
            add(asmTokens, OpCode::PUSHC);

            add(asmTokens, OpCode::POPB);
            add(asmTokens, OpCode::POPA);
            add(asmTokens, OpCode::ADD);

            add(asmTokens, OpCode::PUSHC);

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            if (tokens[current].type == TokenType::LEFT_BRACKET || tokens[current].type == TokenType::ACCESSOR) {
                if (std::holds_alternative<Struct>(subType)) {
                    add(asmTokens, OpCode::POPIDX);
                    add(asmTokens, OpCode::IDXC);
                    add(asmTokens, OpCode::PUSHC);
                }
                return parseIndexStatement(cpu, asmTokens, tokens, subType);
            }

            return subType;
        } else if (std::holds_alternative<String>(containerType)) {
            auto _string = std::get<String>(containerType);

            auto index_type = expression(cpu, asmTokens, tokens);
            if (index_type != Integer && index_type != Byte)
                error(tokens[current], "Integer value expected");

            check(tokens[current++], TokenType::RIGHT_BRACKET, "`]' expected");

            add(asmTokens, OpCode::POPB);
            add(asmTokens, OpCode::POPA);
            add(asmTokens, OpCode::ADD);
            add(asmTokens, OpCode::PUSHC);

            return Byte;
        } else {
            error(tokens[current], "Array or string expected");
        }
    } else if (tokens[current].type == TokenType::ACCESSOR) {
        current++;

        if (!(std::holds_alternative<Struct>(containerType)))
            error(tokens[current], "Struct instance expected");

        auto _struct = std::get<Struct>(containerType);

        auto property = identifier(tokens[current++]);

        auto offset = _struct.getOffset(property);
        auto subType = _struct.getType(property);

        add(asmTokens, OpCode::POPIDX);

        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(offset));

        if (tokens[current].type == TokenType::LEFT_BRACKET || tokens[current].type == TokenType::ACCESSOR) {
            add(asmTokens, OpCode::IDXC);
            add(asmTokens, OpCode::PUSHC);
            return parseIndexStatement(cpu, asmTokens, tokens, subType);
        }

        add(asmTokens, OpCode::PUSHIDX);
        return subType;
    }

    return Integer;
}

static void assign_op_statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, OpCode opcode) {
    auto varname = tokens[current].str;

    if (env->isFunction(varname)) {
        error(tokens[current], "Cannot reassign function");
    } else if (env->isStruct(varname)) {
        error(tokens[current], "Cannot reassign struct");
    } else if (env->isConstant(varname)) {
        error(tokens[current], "Cannot reassign constant value");
    } else if (env->isGlobal(varname)) {
        current += 2;

        auto type = expression(cpu, asmTokens, tokens);

        if (type == None)
            error(tokens[current], "Cannot assign a void value to variable `" + varname + "'");

        addPointer(asmTokens, OpCode::LOADA, env->get(varname));
        add(asmTokens, OpCode::POPB);

        add(asmTokens, opcode);

        addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));
    } else {
        current += 2;

        auto type = expression(cpu, asmTokens, tokens);

        if (type == None)
            error(tokens[current], "Cannot assign a void value to variable `" + varname + "'");

        addValue16(asmTokens, OpCode::READA, Int16AsValue(env->get(varname)));

        add(asmTokens, OpCode::POPB);

        add(asmTokens, opcode);

        addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));
    }
}

static void assign_op_composite_statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens, OpCode opcode) {
    current++;
    add(asmTokens, OpCode::POPIDX);
    add(asmTokens, OpCode::IDXA);
    auto type = expression(cpu, asmTokens, tokens);

    add(asmTokens, OpCode::POPB);
    add(asmTokens, opcode);
    add(asmTokens, OpCode::WRITECX);
}


static ValueType statement(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::ASSIGN) {
       auto varname = tokens[current].str; 

        if (env->isFunction(varname)) {
            error(tokens[current], "Cannot reassign function");
        } else if (env->isStruct(varname)) {
            error(tokens[current], "Cannot reassign struct");
        } else if (env->isConstant(varname)) {
            error(tokens[current], "Cannot reassign constant value");
        } else if (env->isGlobal(varname)) {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            add(asmTokens, OpCode::POPC);
            addPointer(asmTokens, OpCode::STOREC, env->set(varname, type));

            return type;
        } else {
            current += 2;

            auto type = expression(cpu, asmTokens, tokens);

            if (type == None)
                error(tokens[current], "Cannot assign a void value to variable `" + varname + "'");

            add(asmTokens, OpCode::POPC);

            addValue16(asmTokens, OpCode::WRITEC, Int16AsValue(env->set(varname, type)));

            return type;
        }
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::PLUS_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::ADD);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::MINUS_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::SUB);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::STAR_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::MUL);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::SLASH_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::DIV);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::PERCENT_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::MOD);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::BACKSLASH_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::IDIV);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::LEFT_SHIFT_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::LSHIFT);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::RIGHT_SHIFT_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::RSHIFT);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::AMPERSAND_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::BAND);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::PIPE_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::BOR);
    } else if (tokens[current].type == TokenType::IDENTIFIER && tokens[current+1].type == TokenType::CARAT_ASSIGN) {
        assign_op_statement(cpu, asmTokens, tokens, OpCode::XOR);
    } else if (tokens[current].type == TokenType::IDENTIFIER && (tokens[current+1].type == TokenType::LEFT_BRACKET || tokens[current+1].type == TokenType::ACCESSOR)) {
        auto varname = tokens[current++].str;

        if (env->isFunction(varname)) {
            error(tokens[current], "Cannot index function");
        } else if (env->isStruct(varname)) {
            error(tokens[current], "Cannot index struct type");
        } else {
            auto varType = env->getType(varname);

            if (env->isGlobal(varname)) {
                addPointer(asmTokens, OpCode::LOADC, env->get(varname));
            } else {
                addValue16(asmTokens, OpCode::READC, Int16AsValue(env->get(varname)));
            }

            add(asmTokens, OpCode::PUSHC);

            auto ltype = parseIndexStatement(cpu, asmTokens, tokens, varType);

            if (tokens[current].type == TokenType::DECREMENT) {
                current++;
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::IDXC);
                addValue16(asmTokens, OpCode::INCC, Int16AsValue(-1));
                add(asmTokens, OpCode::WRITECX);
            } else if (tokens[current].type == TokenType::INCREMENT) {
                current++;
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::IDXC);
                addValue16(asmTokens, OpCode::INCC, Int16AsValue(1));
                add(asmTokens, OpCode::WRITECX);
            } else if (tokens[current].type == TokenType::PLUS_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::ADD);
            } else if (tokens[current].type == TokenType::MINUS_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::SUB);
            } else if (tokens[current].type == TokenType::STAR_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::MUL);
            } else if (tokens[current].type == TokenType::SLASH_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::DIV);
            } else if (tokens[current].type == TokenType::BACKSLASH_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::IDIV);
            } else if (tokens[current].type == TokenType::PERCENT_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::MOD);
            } else if (tokens[current].type == TokenType::LEFT_SHIFT_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::LSHIFT);
            } else if (tokens[current].type == TokenType::RIGHT_SHIFT_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::RSHIFT);
            } else if (tokens[current].type == TokenType::AMPERSAND_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::BAND);
            } else if (tokens[current].type == TokenType::PIPE_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::BOR);
            } else if (tokens[current].type == TokenType::CARAT_ASSIGN) {
                assign_op_composite_statement(cpu, asmTokens, tokens, OpCode::XOR);
            } else {
                check(tokens[current++], TokenType::ASSIGN, "`=' expected");

                auto type = expression(cpu, asmTokens, tokens);

                if (type == None)
                    error(tokens[current], "Cannot assign a void value");

                if (ltype != Undefined && type != ltype) {
                    std::ostringstream s;

                    s << "Type mismatch: expected " << ValueTypeToString(ltype) << ", got " << ValueTypeToString(type);

                    error(tokens[current], s.str());
                }

                add(asmTokens, OpCode::POPC);
                add(asmTokens, OpCode::POPIDX);
                add(asmTokens, OpCode::WRITECX);
            }
        }
    } else {
        auto type = expression(cpu, asmTokens, tokens);
        if (type != None)
            add(asmTokens, OpCode::POPC);
    }
    return None;
}


static ValueType declaration(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    if (tokens[current].type == TokenType::VAL) {
        define_const(cpu, asmTokens, tokens);
    } else if (tokens[current].type == TokenType::VAR) {
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
            error(tokens[current], "Cannot break when not in loop");
        add(asmTokens, OpCode::JMP, LOOP_BREAK);
        check(tokens[current++], TokenType::SEMICOLON, "`;' expected");
    } else if (tokens[current].type == TokenType::CONTINUE) {
        current++;
        if (LOOP_CONTINUE.size() == 0)
            error(tokens[current], "Cannot continue when not in loop");
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
            error(tokens[current], "Cannot return when not in function");
        }
        current++;
        ValueType type = None;

        if (tokens[current].type != TokenType::SEMICOLON) {
            type = expression(cpu, asmTokens, tokens);
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
    std::vector<std::pair<std::string, ValueType>> params;

    check(tokens[current++], TokenType::DEF, "`def' expected");
    auto name = identifier(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_PAREN, "`(' expected");

    if (tokens[current].type != TokenType::RIGHT_PAREN) {
        ValueType type = Any;

        auto param = identifier(tokens[current++]);
        if (tokens[current].type == TokenType::COLON) {
            current++;

            if (tokens[current].type == TokenType::INT) {
                current++;
                params.push_back(std::make_pair(param, Integer));
            } else if (tokens[current].type == TokenType::FLOAT) {
                current++;
                params.push_back(std::make_pair(param, Float));
            } else if (tokens[current].type == TokenType::STR) {
                current++;
                params.push_back(std::make_pair(param, String()));
            } else {
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(tokens[current], type_name + " does not name a struct");

                auto _struct = env->getStruct(type_name);
                params.push_back(std::make_pair(param, ValueType(_struct)));
            }
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

            if (tokens[current].type == TokenType::COLON) {
                current++;
                if (tokens[current].type == TokenType::INT) {
                    current++;
                    type = Integer;
                } else if (tokens[current].type == TokenType::FLOAT) {
                    current++;
                    type = Float;
                } else if (tokens[current].type == TokenType::STR) {
                    current++;
                    type = String();
                } else {
                    auto type_name = identifier(tokens[current++]);

                    if (!env->isStruct(type_name))
                        error(tokens[current], type_name + " does not name a struct");

                    type = env->getStruct(type_name);
                }
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
            params.push_back(std::make_pair(param, type));
        }
    }

    while (tokens[current].type != TokenType::RIGHT_PAREN) {
        check(tokens[current++], TokenType::COMMA, "`,' expected");

        ValueType type = Any;
        auto param = identifier(tokens[current++]);

        if (tokens[current].type == TokenType::COLON) {
            current++;
            if (tokens[current].type == TokenType::INT) {
                current++;
                params.push_back(std::make_pair(param, Integer));
            } else if (tokens[current].type == TokenType::FLOAT) {
                current++;
                params.push_back(std::make_pair(param, Float));
            } else if (tokens[current].type == TokenType::STR) {
                current++;
                params.push_back(std::make_pair(param, String()));
            } else {
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(tokens[current], type_name + " does not name a struct");

                auto _struct = env->getStruct(type_name);

                params.push_back(std::make_pair(param, ValueType(_struct)));
            }
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

            if (tokens[current].type == TokenType::COLON) {
                current++;

                if (tokens[current].type == TokenType::INT) {
                    current++;
                    type = Integer;
                } else if (tokens[current].type == TokenType::FLOAT) {
                    current++;
                    type = Float;
                } else if (tokens[current].type == TokenType::STR) {
                    current++;
                    type = String();
                } else {
                    auto type_name = identifier(tokens[current++]);

                    if (!env->isStruct(type_name))
                        error(tokens[current], type_name + " does not name a struct");

                    type = env->getStruct(type_name);
                }
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
            params.push_back(std::make_pair(param, type));
        }
    }
    check(tokens[current++], TokenType::RIGHT_PAREN, "`)' expected");

    auto function = env->defineFunction(name, params, Undefined);

    add(asmTokens, OpCode::JMP, name + "_END");
    add(asmTokens, OpCode::NOP, name);
    env = env->beginScope(name, env);

    auto rargs = params;
    std::reverse(rargs.begin(), rargs.end());

    addValue16(asmTokens, OpCode::MOVIDX, Int16AsValue(0));

    for (size_t i = 0; i< rargs.size(); i++) {
        add(asmTokens, OpCode::POPC);
        add(asmTokens, OpCode::WRITECX);
        addValue16(asmTokens, OpCode::INCIDX, Int16AsValue(1));
        env->create(rargs[i].first, rargs[i].second);
    }

    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");

    ValueType type = SimpleType::NONE;
    while (tokens[current].type != TokenType::RIGHT_BRACE) {
        auto newtype = declaration(cpu, asmTokens, tokens);

        if (type == None || type == Any) {
            type = newtype;
        } else if (type != newtype) {
            error(tokens[current], "Return type mismatch");
        }
    }

    check(tokens[current++], TokenType::RIGHT_BRACE, "`}' expected");

    env = env->endScope();

    function.returnType = type;
    if (function.returnType == Any) {
        warning(tokens[current], "Function " + name + " is returning an unbound value");
    }

    env->updateFunction(name, function);

    add(asmTokens, OpCode::RETURN);
    add(asmTokens, OpCode::NOP, name + "_END");
}

static Struct define_struct(int cpu, std::vector<AsmToken> &asmTokens, const std::vector<Token> &tokens) {
    std::vector<std::pair<std::string, ValueType>> slots;

    check(tokens[current++], TokenType::STRUCT, "`struct' expected");
    auto name = identifier(tokens[current++]);
    check(tokens[current++], TokenType::LEFT_BRACE, "`{' expected");
    check(tokens[current++], TokenType::SLOT, "`slot' expected");

    ValueType type = Undefined;

    auto slot = identifier(tokens[current++]);

    if (tokens[current].type == TokenType::COLON) {
        current++;

        if (tokens[current].type == TokenType::INT) {
            current++;
            type = Integer;
        } else if (tokens[current].type == TokenType::FLOAT) {
            current++;
            type = Float;
        } else if (tokens[current].type == TokenType::STR) {
            current++;
            type = String();
        } else {
            auto type_name = identifier(tokens[current++]);

            if (!env->isStruct(type_name))
                error(tokens[current], type_name + " does not name a struct");

            type = env->getStruct(type_name);
        }
    } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
        current++;

        std::stack<int> dimensions;

        if (tokens[current].type == TokenType::RIGHT_BRACKET) {
            dimensions.push(0);
        } else {
            check(tokens[current], TokenType::INTEGER, "integer expected");
            auto dim = std::stoi(tokens[current++].str);
            dimensions.push(dim);
        }

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
            if (tokens[current].type == TokenType::INT) {
                current++;
                type = Integer;
            } else if (tokens[current].type == TokenType::FLOAT) {
                current++;
                type = Float;
            } else if (tokens[current].type == TokenType::STR) {
                current++;
                type = String();
            } else {
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(tokens[current], type_name + " does not name a struct");

                type = env->getStruct(type_name);
            }
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

        ValueType type = Undefined;

        auto slot = identifier(tokens[current++]);

        if (tokens[current].type == TokenType::COLON) {
            current++;

            if (tokens[current].type == TokenType::INT) {
                current++;
                type = Integer;
            } else if (tokens[current].type == TokenType::FLOAT) {
                current++;
                type = Float;
            } else if (tokens[current].type == TokenType::STR) {
                current++;
                type = String();
            } else {
                auto type_name = identifier(tokens[current++]);

                if (!env->isStruct(type_name))
                    error(tokens[current], type_name + " does not name a struct");

                type = env->getStruct(type_name);
            }
        } else if (tokens[current].type == TokenType::LEFT_BRACKET) {
            current++;

            std::stack<int> dimensions;
            if (tokens[current].type == TokenType::RIGHT_BRACKET) {
                dimensions.push(0);
            } else {
                check(tokens[current], TokenType::INTEGER, "integer expected");
                auto dim = std::stoi(tokens[current++].str);
                dimensions.push(dim);
            }

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

                if (tokens[current].type == TokenType::INT) {
                    current++;
                    type = Integer;
                } else if (tokens[current].type == TokenType::FLOAT) {
                    current++;
                    type = Float;
                } else if (tokens[current].type == TokenType::STR) {
                    current++;
                    type = String();
                } else {
                    auto type_name = identifier(tokens[current++]);

                    if (!env->isStruct(type_name))
                        error(tokens[current], type_name + " does not name a struct");

                    type = env->getStruct(type_name);
                }
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

    //addPointer(asmTokens, OpCode::SETC, 0);

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

    std::vector<AsmToken> data;

    for (auto entry : StringTable) {
        auto str = entry.first;
        auto ptr = entry.second;

        addPointer(data, OpCode::SETIDX, ptr);
        addString(data, OpCode::SDATA, str);
    }

    data.insert(data.end(), asmTokens.begin(), asmTokens.end());

    return data;
}
