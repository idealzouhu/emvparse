#ifndef EMVPARSE_GLOBAL_H
#define EMVPARSE_GLOBAL_H

/**
 * @brief Shared-library export/import macros (`EMVPARSE_EXPORT`).
 *
 * Included automatically by public headers (`input_parser.h`, `tlv.h`, …).
 * Installed for ABI; application code should not `#include` this file directly.
 */
#ifndef EMVPARSE_EXPORT
    #if defined(_WIN32) || defined(_WIN64)
        #if defined(EMVPARSE_LIBRARY)
            #define EMVPARSE_EXPORT __declspec(dllexport)
        #else
            #define EMVPARSE_EXPORT __declspec(dllimport)
        #endif
    #else
        #if defined(EMVPARSE_LIBRARY)
            #define EMVPARSE_EXPORT __attribute__((visibility("default")))
        #else
            #define EMVPARSE_EXPORT
        #endif
    #endif
#endif

/** Internal library symbols (not part of the stable public ABI). */
#if defined(EMVPARSE_LIBRARY) && (defined(_WIN32) || defined(_WIN64))
    #define EMVPARSE_INTERNAL
#else
    #define EMVPARSE_INTERNAL
#endif

#endif // EMVPARSE_GLOBAL_H
