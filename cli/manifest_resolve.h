#ifndef EMVPARSE_CLI_MANIFEST_RESOLVE_H
#define EMVPARSE_CLI_MANIFEST_RESOLVE_H

#include <string>

std::string getExecutableDirectory();

/** Returns empty string if manifest cannot be resolved. */
std::string resolveManifestPath(const std::string& cliOverride);

#endif
