#include "Parser.h"

#include <fstream>
#include <sstream>
#include <ctime>

#include <stack>
#include <variant>

#include <algorithm>

//static void error(uint32_t linenumber, const std::string &err);

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isWhitespace(char c) {
    return (c == ' ') || (c == '\t');
}

/*
static std::string str_toupper(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return std::toupper(c); }
    );

    return s;
}
*/

static void error(int linenumber, int position, const std::string &err) {
    std::cerr << "Parsing error on line " << linenumber << " at ppsition " << position << ": " << err << std::endl;
    exit(-1);
}

static std::string str_tolower(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return std::tolower(c); }
    );

    return s;
}

std::vector<Token> parse(const std::string &source) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t line = 1;
    size_t pos = 1;

    while (i < source.size()) {
        if (source[i] == '\n') {
            line++;
            pos = i;
        }

        if (isWhitespace(source[i])) {
            i++;
            continue;
        }

        if (source[i] == '/' && source[i+1] == '/') {
            i += 2;
            while (source[i] != '\n')
                i++;

            line++;
            pos = i;
        }

        if (isDigit(source[i])) {
            auto tokenType = TokenType::INTEGER;
            size_t start = i++;

            while (isDigit(source[i]))
                i++;

            if (source[i] == '.' && isDigit(source[i+1])) {
                tokenType = TokenType::REAL;
                i++;

                while (isDigit(source[i]))
                    i++;
            }

            tokens.push_back(Token(tokenType, line, i-pos, source.substr(start, i-start)));
        } else if (isAlpha(source[i])) {
            auto precedence = Precedence::NONE;
            size_t start = i++;
            auto tokenType = TokenType::IDENTIFIER;

            while((isAlpha(source[i]) || isDigit(source[i])))
                i++;

            auto token = source.substr(start, i-start);
            auto keyword = str_tolower(token);

            switch (keyword[0]) {
                case 'a':
                    if (keyword == "abs") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "auto") {
                        tokenType = TokenType::AUTO;
                    }
                    else
                    if (keyword == "atan") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'b':
                    if (keyword == "break") {
                        tokenType = TokenType::BREAK;
                    }
                    break;
                case 'c':
                    if (keyword == "cls") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "const") {
                        tokenType = TokenType::CONST;
                    }
                    else
                    if (keyword == "continue") {
                        tokenType = TokenType::CONTINUE;
                    }
                    else
                    if (keyword == "cos") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'd':
                    if (keyword == "def") {
                        tokenType = TokenType::DEF;
                    }
                    else
                    if (keyword == "drawbox") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "drawline") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "drawpixel") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'e':
                    if (keyword == "else") {
                        tokenType = TokenType::ELSE;
                    }
                    break;
                case 'f':
                    if (keyword == "float") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "for") {
                        tokenType = TokenType::FOR;
                    }
                    else
                    if (keyword == "free") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'g':
                    if (keyword == "getc") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "gets") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'i':
                    if (keyword == "if") {
                        tokenType = TokenType::IF;
                    }
                    else
                    if (keyword == "int") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'k':
                    if (keyword == "keypressed") {
                        tokenType = TokenType::BUILTIN;
                    }
                case 'l':
                    if (keyword == "len") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "log") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'm':
                    if (keyword == "max") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "min") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'p':
                    if (keyword == "puts") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'r':
                    if (keyword == "rand") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "return") {
                        tokenType = TokenType::RETURN;
                    }
                    break;
                case 's':
                    if (keyword == "setcolours") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "setcursor") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "setpalette") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "sin") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "sizeof") {
                        tokenType = TokenType::SIZEOF;
                    }
                    else
                    if (keyword == "slot") {
                        tokenType = TokenType::SLOT;
                    }
                    else
                    if (keyword == "sqrt") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "srand") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "sound") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "string") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "strcpy") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "strlen") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "struct") {
                        tokenType = TokenType::STRUCT;
                    }
                    break;
                case 't':
                    if (keyword == "tan") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'v':
                    if (keyword == "voice") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "vsync") {
                        tokenType = TokenType::BUILTIN;
                    }
                    break;
                case 'w':
                    if (keyword == "while") {
                        tokenType = TokenType::WHILE;
                    }
                    break;
            }

            if (tokenType != TokenType::IDENTIFIER)
                token = keyword;

            tokens.push_back(Token(tokenType, line, start-pos, token, precedence));
        } else if (source[i] == '\'') {
            int start = i++;
            std::string str;

            char c = source[i++];

            if (c == '\n')
                error(line, i-pos, "Unterminated character literal");

            if (c == '\\' && source[i] == 'n') {
                c = '\n';
                i++;
            } else if (c == '\\' && source[i] == '"') {
                c = '"';
                i++;
            } else if (c == '\\' && source[i] == 't') {
                c = '\t';
                i++;
            } else if (c == '\\' && source[i] == '\\') {
                c = '\\';
                i++;
            }

            if (source[i++] != '\'')
                error(line, i-pos, "Unterminated character literal");

            str += c;

            tokens.push_back(Token(TokenType::CHARACTER, line, start-pos, str));
        } else if (source[i] == '"') {
            int start = i++;
            std::string str;
            while (source[i++] != '"') {
                char c = source[i-1];

                if (c == '\n')
                    error(line, i-pos, "Unterminated string literal");

                if (c == '\\' && source[i] == 'n') {
                    c = '\n';
                    i++;
                } else if (c == '\\' && source[i] == '"') {
                    c = '"';
                    i++;
                } else if (c == '\\' && source[i] == 't') {
                    c = '\t';
                    i++;
                } else if (c == '\\' && source[i] == '\\') {
                    c = '\\';
                    i++;
                }

                str += c;
            }

            tokens.push_back(Token(TokenType::STRING, line, start, str));
        } else {
            switch (source[i++]) {
                case '-':
                    switch (source[i]) {
                        case '>':
                            tokens.push_back(Token(TokenType::ACCESSOR, line, i-pos, "->", Precedence::CALL));
                            i++;
                            break;
                        case '-':
                            tokens.push_back(Token(TokenType::DECREMENT, line, i-pos, "--"));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::MINUS_ASSIGN, line, i-pos, "-=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::MINUS, line, i-pos, "-", Precedence::TERM));
                    }
                    break;
                case '+':
                    switch (source[i]) {
                        case '+':
                            tokens.push_back(Token(TokenType::INCREMENT, line, i-pos, "++"));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::PLUS_ASSIGN, line, i-pos, "+=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PLUS, line, i-pos, "+", Precedence::TERM));
                    }
                    break;
                case '/':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::SLASH_ASSIGN, line, i-pos, "/=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::SLASH, line, i-pos, "/", Precedence::FACTOR));
                    }
                    break;
                case '*':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::STAR_ASSIGN, line, i-pos, "*=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::STAR, line, i-pos, "*", Precedence::FACTOR));
                    }
                    break;
                case '^':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::CARAT_ASSIGN, line, i-pos, "^=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::CARAT, line, i-pos, "^", Precedence::TERM));
                    }
                    break;
                case '\\':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::BACKSLASH_ASSIGN, line, i-pos, "\\=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::BACKSLASH, line, i-pos, "\\", Precedence::FACTOR));
                    }
                    break;
                case '%':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::PERCENT_ASSIGN, line, i-pos, "%=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PERCENT, line, i-pos, "%", Precedence::FACTOR));
                    }
                    break;
                case ';':
                    tokens.push_back(Token(TokenType::SEMICOLON, line, i-pos, ";"));
                    break;
                case ':':
                    tokens.push_back(Token(TokenType::COLON, line, i-pos, ":"));
                    break;
                case '\'':
                    tokens.push_back(Token(TokenType::QUOTE, line, i-pos, "'"));
                    break;
                case ',':
                    tokens.push_back(Token(TokenType::COMMA, line, i-pos, ","));
                    break;
                case '.':
                    tokens.push_back(Token(TokenType::DOT, line, i-pos, "."));
                    break;
                case '?':
                    tokens.push_back(Token(TokenType::QUESTION_MARK, line, i-pos, "?"));
                    break;
                case '&':
                    switch (source[i]) {
                        case '&':
                            tokens.push_back(Token(TokenType::AND, line, i-pos, "&&", Precedence::AND));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::AMPERSAND_ASSIGN, line, i-pos, "&=", Precedence::ASSIGNMENT));
                            i++;
                            break;

                        default:
                            tokens.push_back(Token(TokenType::AMPERSAND, line, i-pos, "&", Precedence::TERM));
                    }
                    break;
                case '$':
                    tokens.push_back(Token(TokenType::DOLLAR, line, i-pos, "$"));
                    break;
                case '~':
                    tokens.push_back(Token(TokenType::TILDE, line, i-pos, "~"));
                    break;
                case '@':
                    tokens.push_back(Token(TokenType::AT, line, i-pos, "@"));
                    break;
                case '|':
                    switch (source[i]) {
                        case '|':
                            tokens.push_back(Token(TokenType::OR, line, i-pos, "||", Precedence::OR));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::PIPE_ASSIGN, line, i-pos, "|=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PIPE, line, i-pos, "|", Precedence::TERM));
                    }
                    break;
                case '`':
                    tokens.push_back(Token(TokenType::BACKTICK, line, i-pos, "`"));
                    break;
                case '=':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::EQUAL, line, i-pos, "==", Precedence::EQUALITY));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::ASSIGN, line, i-pos, "=", Precedence::ASSIGNMENT));
                    }
                    break;
                case '!':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::NOT_EQUAL, line, i-pos, "!=", Precedence::EQUALITY));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::NOT, line, i-pos, "!"));
                    }
                    break;
                case '>':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::GREATER_EQUAL, line, i-pos, ">=", Precedence::COMPARISON));
                            i++;
                            break;
                        case '>':
                            switch (source[i+1]) {
                                case '=':
                                    tokens.push_back(Token(TokenType::RIGHT_SHIFT_ASSIGN, line, i-pos, ">>=", Precedence::ASSIGNMENT));
                                    i++;
                                    break;
                                default:
                                    tokens.push_back(Token(TokenType::RIGHT_SHIFT, line, i-pos, ">>", Precedence::FACTOR));
                            }
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::GREATER, line, i-pos, ">", Precedence::COMPARISON));
                    }
                    break;
                case '<':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::LESS_EQUAL, line, i-pos, "<=", Precedence::COMPARISON));
                            i++;
                            break;
                        case '<':
                            switch (source[i+1]) {
                                case '=':
                                    tokens.push_back(Token(TokenType::LEFT_SHIFT_ASSIGN, line, i-pos, "<<=", Precedence::ASSIGNMENT));
                                    i++;
                                    break;
                                default:
                                    tokens.push_back(Token(TokenType::LEFT_SHIFT, line, i-pos, "<<", Precedence::FACTOR));
                            }
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::LESS, line, i-pos, "<", Precedence::COMPARISON));
                    }
                    break;
                case '(':
                    tokens.push_back(Token(TokenType::LEFT_PAREN, line, i-pos, "(", Precedence::CALL));
                    break;
                case ')':
                    tokens.push_back(Token(TokenType::RIGHT_PAREN, line, i-pos, ")"));
                    break;
                case '{':
                    tokens.push_back(Token(TokenType::LEFT_BRACE, line, i-pos, "{"));
                    break;
                case '}':
                    tokens.push_back(Token(TokenType::RIGHT_BRACE, line, i-pos, "}"));
                    break;
                case '[':
                    tokens.push_back(Token(TokenType::LEFT_BRACKET, line, i-pos, "[", Precedence::CALL));
                    break;
                case ']':
                    tokens.push_back(Token(TokenType::RIGHT_BRACKET, line, i-pos, "]"));
                    break;

            }

        }
    }

    tokens.push_back(Token(TokenType::EOL, line, i-pos, ""));

    return tokens;
}
