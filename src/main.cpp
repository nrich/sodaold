#include <cstdlib>
#include <memory>
#include <functional>
#include <map>

#include <iostream>

#include "ezOptionParser.hpp"

#include "Parser.h"
#include "Compiler.h"
#include "Binary.h"

int main(int argc, char **argv) {
    ez::ezOptionParser opt;

    opt.overview = "soda compiler";
    opt.syntax = std::string(argv[0]) + " [OPTIONS] [runfile]\n";
    opt.example = std::string(argv[0]) + " -o test.obj file.soda\n";
    opt.footer = std::string(argv[0]) + " v" + std::string(VERSION) + "\n";

    opt.add(
        "", // Default.
        0, // Required?
        0, // Number of args expected.
        0, // Delimiter if expecting multiple args.
        "Display usage instructions.", // Help description.
        "-h"     // Flag token.
    );

    opt.add(
        "", // Default.
        0, // Required?
        1, // Number of args expected.
        0, // Delimiter if expecting multiple args.
        "Output file", // Help description.
        "-o"     // Flag token.
    );

    opt.add(
        "", // Default.
        0, // Required?
        0, // Number of args expected.
        0, // Delimiter if expecting multiple args.
        "assembly output", // Help description.
        "-s"     // Flag token.
    );

    opt.add(
        "", // Default.
        0, // Required?
        0, // Number of args expected.
        0, // Delimiter if expecting multiple args.
        "optimise output", // Help description.
        "-O"     // Flag token.
    );

    opt.parse(argc, (const char**)argv);

    if (opt.isSet("-h")) {
        std::string usage;
        opt.getUsage(usage);
        std::cout << usage << std::endl;
        exit(1);
    }

    int cpu = 16;

    std::string filename = *opt.lastArgs[0];
    std::ifstream infile(filename);

    if (!infile.is_open()) {
        std::cerr << "Could not open `" << filename << "'" << std::endl;
        exit(-1);
    }

    std::stringstream buffer;
    buffer << infile.rdbuf();

    auto tokens = parse(buffer.str());
    auto asmTokens = compile(cpu, tokens);

    if (opt.isSet("-O")) {
        asmTokens = optimise(cpu, asmTokens);
    }

    if (opt.isSet("-s")) {
        std::string filename;

        if (opt.isSet("-o")) {
            opt.get("-o")->getString(filename);

            std::ofstream ofs(filename);
        
            for (const auto &token : asmTokens) {
                ofs << token.toString() << std::endl;
            }

            ofs.close();
        } else {
            for (const auto &token : asmTokens) {
                std::cout << token.toString() << std::endl;
            }
        }
    } else {
        std::string filename;

        if (opt.isSet("-o")) {
            opt.get("-o")->getString(filename);
        } else {
            filename = "a.obj";
        }

        std::string ExeHeader = "GR16";

        std::ofstream ofs(filename, std::ios::binary);
        ofs.write(ExeHeader.c_str(), 4);

        Binary binary(cpu);
        auto code = binary.translate(asmTokens);

        ofs.write(reinterpret_cast<char *>(code.data()), code.size());
        ofs.close();
    }
}
