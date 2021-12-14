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

    while (i < source.size()) {
        if (isWhitespace(source[i])) {
            i++;
            continue;
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

            tokens.push_back(Token(tokenType, i, source.substr(start, i-start)));
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
                case 'l':
                    if (keyword == "len") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "line") {
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
                    break;
                case 'w':
                    if (keyword == "wait") {
                        tokenType = TokenType::BUILTIN;
                    }
                    else
                    if (keyword == "while") {
                        tokenType = TokenType::WHILE;
                    }
                    break;
            }

            if (tokenType != TokenType::IDENTIFIER)
                token = keyword;

            tokens.push_back(Token(tokenType, i, token, precedence));
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

            tokens.push_back(Token(TokenType::CHARACTER, start, str));
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

            tokens.push_back(Token(TokenType::STRING, start, str));
        } else {
            switch (source[i++]) {
                case '-':
                    switch (source[i]) {
                        case '>':
                            tokens.push_back(Token(TokenType::ACCESSOR, i, "->", Precedence::CALL));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::MINUS, i, "-", Precedence::TERM));
                    }
                    break;
                case '+':
                    tokens.push_back(Token(TokenType::PLUS, i, "+", Precedence::TERM));
                    break;
                case '/':
                    tokens.push_back(Token(TokenType::SLASH, i, "/", Precedence::FACTOR));
                    break;
                case '*':
                    tokens.push_back(Token(TokenType::STAR, i, "*", Precedence::FACTOR));
                    break;
                case '^':
                    tokens.push_back(Token(TokenType::CARAT, i, "^", Precedence::FACTOR));
                    break;
                case '\\':
                    tokens.push_back(Token(TokenType::BACKSLASH, i, "\\", Precedence::FACTOR));
                    break;
                case '%':
                    tokens.push_back(Token(TokenType::PERCENT, i, "%", Precedence::FACTOR));
                    break;
                case ';':
                    tokens.push_back(Token(TokenType::SEMICOLON, i, ";"));
                    break;
                case ':':
                    tokens.push_back(Token(TokenType::COLON, i, ":"));
                    break;
                case '\'':
                    tokens.push_back(Token(TokenType::QUOTE, i, "'"));
                    break;
                case ',':
                    tokens.push_back(Token(TokenType::COMMA, i, ","));
                    break;
                case '.':
                    tokens.push_back(Token(TokenType::DOT, i, "."));
                    break;
                case '?':
                    tokens.push_back(Token(TokenType::QUESTION_MARK, i, "?"));
                    break;
                case '&':
                    switch (source[i]) {
                        case '&':
                            tokens.push_back(Token(TokenType::AND, i, "&&", Precedence::AND));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::AMPERSAND, i, "&"));
                    }
                    break;
                case '$':
                    tokens.push_back(Token(TokenType::DOLLAR, i, "$"));
                    break;
                case '~':
                    tokens.push_back(Token(TokenType::TILDE, i, "~"));
                    break;
                case '@':
                    tokens.push_back(Token(TokenType::AT, i, "@"));
                    break;
                case '|':
                    switch (source[i]) {
                        case '|':
                            tokens.push_back(Token(TokenType::OR, i, "||", Precedence::OR));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::PIPE, i, "|"));
                    }
                    break;
                case '`':
                    tokens.push_back(Token(TokenType::BACKTICK, i, "`"));
                    break;
                case '=':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::EQUAL, i, "==", Precedence::EQUALITY));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::ASSIGN, i, "="));
                    }
                    break;
                case '!':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::NOT_EQUAL, i, "!=", Precedence::EQUALITY));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::NOT, i, "!"));
                    }
                    break;
                case '>':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::GREATER_EQUAL, i, ">=", Precedence::COMPARISON));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::GREATER, i, ">", Precedence::COMPARISON));
                    }
                    break;
                case '<':
                    switch (source[i]) {
                        case '=':
                            tokens.push_back(Token(TokenType::LESS_EQUAL, i, "<=", Precedence::COMPARISON));
                            i++;
                            break;
                        default:
                            tokens.push_back(Token(TokenType::LESS, i, "<", Precedence::COMPARISON));
                    }
                    break;
                case '(':
//                    if (tokens.back().type == TokenType::IDENTIFIER) {
//                        tokens.back().type = TokenType::FUNCTION;
//                    }
                    tokens.push_back(Token(TokenType::LEFT_PAREN, i, "(", Precedence::CALL));
                    break;
                case ')':
                    tokens.push_back(Token(TokenType::RIGHT_PAREN, i, ")"));
                    break;
                case '{':
                    tokens.push_back(Token(TokenType::LEFT_BRACE, i, "{"));
                    break;
                case '}':
                    tokens.push_back(Token(TokenType::RIGHT_BRACE, i, "}"));
                    break;
                case '[':
                    tokens.push_back(Token(TokenType::LEFT_BRACKET, i, "[", Precedence::CALL));
                    break;
                case ']':
                    tokens.push_back(Token(TokenType::RIGHT_BRACKET, i, "]"));
                    break;

            }

        }
    }

    tokens.push_back(Token(TokenType::EOL, i, ""));

    return tokens;
}
