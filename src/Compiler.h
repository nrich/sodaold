#ifndef __COMPILER_H__
#define __COMPILER_H__

#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "Parser.h"
#include "Assembly.h"

std::vector<AsmToken> compile(const int cpu, const std::vector<Token> &tokens);

#endif //__COMPILER_H__
