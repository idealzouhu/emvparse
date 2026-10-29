#ifndef EMVPARSE_ISO_NUMERIC_LOOKUP_H
#define EMVPARSE_ISO_NUMERIC_LOOKUP_H

#include <string>

namespace emvparse {

/** Load iso-lookups.json from a registry directory (manifest parent). Safe to call repeatedly. */
void loadIsoNumericLookupsFromRegistryDir(const std::string& registryDir);

/** Normalize BCD digit string to 3-digit ISO numeric key (3166 / 4217). */
std::string isoNumericLookupKey(const std::string& bcdDigits);

std::string lookupIso3166Numeric(const std::string& bcdDigits);
std::string lookupIso4217Numeric(const std::string& bcdDigits);

/** enumRef values used in tag registry JSON. */
std::string lookupIsoByEnumRef(const std::string& enumRef, const std::string& bcdDigits);

} // namespace emvparse

#endif
