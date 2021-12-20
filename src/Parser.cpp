#include "Parser.h"

#include <fstream>
#include <sstream>
#include <ctime>

#include <stack>
#include <variant>

#include <algorithm>

static void error(uint32_t linenumber, const std::string &err);

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isWhitespace(char c) {
    return (c == ' ') || (c == '\t');
}

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

std::vector<Token> parse(const std::string &source) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t line = 1;
    size_t pos = 1;

    while (i < source.size()) {
        if (i == '\n') {
            line++;
            pos = 1;
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
            pos = 1;
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

            tokens.push_back(Token(tokenType, line, pos, source.substr(start, i-start)));
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

            tokens.push_back(Token(tokenType, line, pos, token, precedence));
        } else if (source[i] == '\'') {
            int start = i++;
            std::string str;

            while (source[i++] != '\'') {
                char c = source[i-1];

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

            tokens.push_back(Token(TokenType::CHARACTER, line, start, str));
        } else if (source[i] == '"') {
            int start = i++;
            std::string str;
            while (source[i++] != '"') {
                char c = source[i-1];

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
                            tokens.push_back(Token(TokenType::ACCESSOR, line, pos, "->", Precedence::CALL));
                            i++;
                            break;
                        case '-':
                            tokens.push_back(Token(TokenType::DECREMENT, line, pos, "--"));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::MINUS_ASSIGN, line, pos, "-=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::MINUS, line, pos, "-", Precedence::TERM));
                    }
                    break;
                case '+':
                    switch (source[i]) {
                        case '+':
                            tokens.push_back(Token(TokenType::INCREMENT, line, pos, "++"));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::PLUS_ASSIGN, line, pos, "+=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PLUS, line, pos, "+", Precedence::TERM));
                    }
                    break;
                case '/':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::SLASH_ASSIGN, line, pos, "/=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::SLASH, line, pos, "/", Precedence::FACTOR));
                    }
                    break;
                case '*':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::STAR_ASSIGN, line, pos, "*=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::STAR, line, pos, "*", Precedence::FACTOR));
                    }
                    break;
                case '^':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::CARAT_ASSIGN, line, pos, "^=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::CARAT, line, pos, "^", Precedence::TERM));
                    }
                    break;
                case '\\':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::BACKSLASH_ASSIGN, line, pos, "\\=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::BACKSLASH, line, pos, "\\", Precedence::FACTOR));
                    }
                    break;
                case '%':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::PERCENT_ASSIGN, line, pos, "%=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PERCENT, line, pos, "%", Precedence::FACTOR));
                    }
                    break;
                case ';':
                    tokens.push_back(Token(TokenType::SEMICOLON, line, pos, ";"));
                    break;
                case ':':
                    tokens.push_back(Token(TokenType::COLON, line, pos, ":"));
                    break;
                case '\'':
                    tokens.push_back(Token(TokenType::QUOTE, line, pos, "'"));
                    break;
                case ',':
                    tokens.push_back(Token(TokenType::COMMA, line, pos, ","));
                    break;
                case '.':
                    tokens.push_back(Token(TokenType::DOT, line, pos, "."));
                    break;
                case '?':
                    tokens.push_back(Token(TokenType::QUESTION_MARK, line, pos, "?"));
                    break;
                case '&':
                    switch (source[i]) {
                        case '&':
                            tokens.push_back(Token(TokenType::AND, line, pos, "&&", Precedence::AND));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::AMPERSAND_ASSIGN, line, pos, "&=", Precedence::ASSIGNMENT));
                            i++;
                            break;

                        default:
                            tokens.push_back(Token(TokenType::AMPERSAND, line, pos, "&", Precedence::TERM));
                    }
                    break;
                case '$':
                    tokens.push_back(Token(TokenType::DOLLAR, line, pos, "$"));
                    break;
                case '~':
                    tokens.push_back(Token(TokenType::TILDE, line, pos, "~"));
                    break;
                case '@':
                    tokens.push_back(Token(TokenType::AT, line, pos, "@"));
                    break;
                case '|':
                    switch (source[i]) {
                        case '|':
                            tokens.push_back(Token(TokenType::OR, line, pos, "||", Precedence::OR));
                            i++;
                            break;
                        case '=':
                            tokens.push_back(Token(TokenType::PIPE_ASSIGN, line, pos, "|=", Precedence::ASSIGNMENT));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PIPE, line, pos, "|", Precedence::TERM));
                    }
                    break;
                case '`':
                    tokens.push_back(Token(TokenType::BACKTICK, line, pos, "`"));
                    break;
                case '=':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::EQUAL, line, pos, "==", Precedence::EQUALITY));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::ASSIGN, line, pos, "=", Precedence::ASSIGNMENT));
                    }
                    break;
                case '!':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::NOT_EQUAL, line, pos, "!=", Precedence::EQUALITY));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::NOT, line, pos, "!"));
                    }
                    break;
                case '>':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::GREATER_EQUAL, line, pos, ">=", Precedence::COMPARISON));
                            i++;
                            break;
                        case '>':
                            switch (source[i+1]) {
                                case '=':
                                    tokens.push_back(Token(TokenType::RIGHT_SHIFT_ASSIGN, line, pos, ">>=", Precedence::ASSIGNMENT));
                                    i++;
                                    break;
                                default:
                                    tokens.push_back(Token(TokenType::RIGHT_SHIFT, line, pos, ">>", Precedence::FACTOR));
                            }
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::GREATER, line, pos, ">", Precedence::COMPARISON));
                    }
                    break;
                case '<':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::LESS_EQUAL, line, pos, "<=", Precedence::COMPARISON));
                            i++;
                            break;
                        case '<':
                            switch (source[i+1]) {
                                case '=':
                                    tokens.push_back(Token(TokenType::LEFT_SHIFT_ASSIGN, line, pos, "<<=", Precedence::ASSIGNMENT));
                                    i++;
                                    break;
                                default:
                                    tokens.push_back(Token(TokenType::LEFT_SHIFT, line, pos, "<<", Precedence::FACTOR));
                            }
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::LESS, line, pos, "<", Precedence::COMPARISON));
                    }
                    break;
                case '(':
                    tokens.push_back(Token(TokenType::LEFT_PAREN, line, pos, "(", Precedence::CALL));
                    break;
                case ')':
                    tokens.push_back(Token(TokenType::RIGHT_PAREN, line, pos, ")"));
                    break;
                case '{':
                    tokens.push_back(Token(TokenType::LEFT_BRACE, line, pos, "{"));
                    break;
                case '}':
                    tokens.push_back(Token(TokenType::RIGHT_BRACE, line, pos, "}"));
                    break;
                case '[':
                    tokens.push_back(Token(TokenType::LEFT_BRACKET, line, pos, "[", Precedence::CALL));
                    break;
                case ']':
                    tokens.push_back(Token(TokenType::RIGHT_BRACKET, line, pos, "]"));
                    break;

            }

        }
    }

    tokens.push_back(Token(TokenType::EOL, line, pos, ""));

    return tokens;
}
