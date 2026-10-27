#include "cli_options.h"
#include "commands_parse.h"
#include "commands_registry.h"
#include <cstring>
#include <iostream>

#ifndef EMVPARSE_VERSION
#define EMVPARSE_VERSION "unknown"
#endif

static void printHelp() {
    std::cout
        << "emvparse — EMV BER-TLV parser (JSON, table, or HTML via tag registry)\n"
        << "\n"
        << "Usage:\n"
        << "  emvparse [global-options] <command> [arguments]\n"
        << "\n"
        << "Commands:\n"
        << "  parse       Parse hex TLV (json/table to stdout; html to a file)\n"
        << "  tag-info    Print registry metadata for one tag (JSON)\n"
        << "  registry    Validate or merge registry JSON (see subcommands below)\n"
        << "\n"
        << "Global options:\n"
        << "  -h, --help       Show this help\n"
        << "  -V, --version    Print version\n"
        << "\n"
        << "Common options (parse, tag-info, registry):\n"
        << "  --scheme <name>     Payment scheme: EMV (default), Visa, UPI\n"
        << "  --registry <path>   manifest.json or directory that contains it\n"
        << "                      (alias: --manifest)\n"
        << "\n"
        << "Examples:\n"
        << "  emvparse parse 9F33020141 --scheme EMV\n"
        << "  emvparse parse 770E8202198094081001020120010300 --format table\n"
        << "  emvparse parse 770E8202198094081001020120010300 --format html\n"
        << "  emvparse parse --file capture.hex --registry ./registry\n"
        << "  emvparse tag-info 9F33\n"
        << "  emvparse registry validate\n"
        << "  emvparse registry merge --scheme EMV --output merged.json\n"
        << "\n"
        << "Environment:\n"
        << "  EMVPARSE_REGISTRY   Same as --registry when the flag is omitted\n"
        << "\n"
        << "Command help:\n"
        << "  emvparse parse --help\n"
        << "  emvparse tag-info --help\n"
        << "  emvparse registry --help\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    if (isHelpOption(argv[1])) {
        printHelp();
        return 0;
    }

    if (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-V") == 0) {
        std::cout << EMVPARSE_VERSION << "\n";
        return 0;
    }

    if (std::strcmp(argv[1], "parse") == 0) {
        return runParseCommand(argc, argv);
    }
    if (std::strcmp(argv[1], "tag-info") == 0) {
        return runTagInfoCommand(argc, argv);
    }
    if (std::strcmp(argv[1], "registry") == 0) {
        return runRegistryCommand(argc, argv);
    }

    std::cerr << "unknown command: " << argv[1] << "\n\n";
    printHelp();
    return 1;
}
