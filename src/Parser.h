#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <iostream>

enum Precedence {
    NONE = 0,
    ASSIGNMENT,
    OR,
    AND,
    EQUALITY,
    COMPARISON,
    TERM,
    FACTOR,
    UNARY,
    CALL,
    PRIMARY
};

enum class TokenType {
    IDENTIFIER,

    STRING,
    REAL,
    INTEGER,
    BUILTIN,
    FUNCTION,
    CHARACTER,

    ASSIGN,
    EQUAL,
    GREATER,
    LESS,
    NOT_EQUAL,
    GREATER_EQUAL,
    LESS_EQUAL,

    INCREMENT,
    DECREMENT,

    LEFT_SHIFT,
    RIGHT_SHIFT,
    AMPERSAND,
    PIPE,
    CARAT,

    AND,
    OR,
    NOT,

    PLUS,
    MINUS,
    STAR,
    SLASH,
    BACKSLASH,
    PERCENT,

    PLUS_ASSIGN,
    MINUS_ASSIGN,
    STAR_ASSIGN,
    SLASH_ASSIGN,
    BACKSLASH_ASSIGN,
    PERCENT_ASSIGN,

    LEFT_SHIFT_ASSIGN,
    RIGHT_SHIFT_ASSIGN,
    AMPERSAND_ASSIGN,
    PIPE_ASSIGN,
    CARAT_ASSIGN,

    QUESTION_MARK,
    DOLLAR,
    TILDE, 
    AT, 
    BACKTICK,

    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE, 
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,

    SEMICOLON,
    COLON,
    COMMA,
    DOT,
    QUOTE,
    APOSTROPHE,

    ACCESSOR,

/*
    AUTO,
    BREAK,
    CASE,
    CHAR,
    CONST,
    CONTINUE,
    DEFAULT,
    DO,
    DOUBLE,
    ELSE,
    ENUM,
    EXTERN,
    FLOAT,
    FOR,
    GOTO,
    IF,
    INT,
    LONG,
    REGISTER,
    RETURN,
    SHORT,
    SIGNED,
    SIZEOF,
    STATIC,
    STRUCT,
    SWITCH,
    TYPEDEF,
    UNION,
    UNSIGNED,
    VOID,
    VOLATILE,
    WHILE,
*/

    AUTO,
    BREAK,
    CONST,
    CONTINUE,
    DEF,
    ELSE,
    FOR,
    IF,
    RETURN,
    SIZEOF,
    SLOT,
    STRUCT,
    WHILE,

    EOL,

    COUNT
};

struct Token {
    TokenType type;
    int line;
    int position;
    std::string str;
    int lbp;
    Token(TokenType type, const int line, const int position, const std::string &str, const int lbp=Precedence::NONE) : type(type), line(line), position(position), str(str), lbp(lbp) {
    }

    std::string toString() const {
        if (type == TokenType::STRING) {
            if (str.empty())
                return "\"\"";

            std::string res = "\"";

            for (const auto &c : str) {
                switch (c) {
                    case '\n':
                        res += '\\';
                        res += 'n';
                        break;
                    case '\t':
                        res += '\\';
                        res += 't';
                        break;
                    case '\\':
                        res += '\\';
                        res += '\\';
                        break;
                    case '"':
                        res += '\\';
                        res += '"';
                        break;
                    default:
                        res += c;
                }
            }

            res += "\"";

            return res;
        } else if (type == TokenType::CHARACTER) {
            return "'" + str + "'";
        }
        return str;
    }
};

std::vector<Token> parse(const std::string &source);

#endif //__PARSER_H__
