#ifndef EMVPARSE_GLOBAL_H
#define EMVPARSE_GLOBAL_H

/**
 * @brief EmvParse 库导出宏
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

#endif // EMVPARSE_GLOBAL_H
