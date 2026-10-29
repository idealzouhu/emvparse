#include <gtest/gtest.h>
#include "tag_registry_ops.h"
#include <cstdio>
#include <fstream>

TEST(TagRegistryOps, ValidateAllSchemes) {
    const std::string manifestPath = "../../registry/manifest.json";
    const RegistryValidationResult result = validateAllSchemes(manifestPath);
    ASSERT_EQ(result.schemes.size(), 9u);
    for (const auto& scheme : result.schemes) {
        EXPECT_TRUE(scheme.loadOk) << scheme.scheme;
        EXPECT_GT(scheme.tagCount, 0u) << scheme.scheme;
    }
    EXPECT_TRUE(result.allOk());
}

TEST(TagRegistryOps, MergeEmvScheme) {
    const std::string manifestPath = "../../registry/manifest.json";
    const std::string outPath = "merged_emv_test_out.json";
    std::string err;
    ASSERT_TRUE(mergeSchemeToFile(manifestPath, "EMV", outPath, err)) << err;
    std::ifstream in(outPath);
    ASSERT_TRUE(in.good());
    std::string line;
    std::getline(in, line);
    EXPECT_FALSE(line.empty());
    std::remove(outPath.c_str());
}
