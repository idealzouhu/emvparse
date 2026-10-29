#ifndef TAGREGISTRY_H
#define TAGREGISTRY_H

#include <unordered_map>
#include <memory>
#include <map>
#include <string>
#include <vector>

struct BitFieldRule {
    int position;
    int length;
    std::string type;
    std::string name;
    std::map<std::string, std::string> enumMap;
};

struct TagInfo {
    std::string tag;
    std::string name;
    std::string type;
    std::string format;
    std::string length;
    std::string kernel;
    std::string source;
    std::string description;
    std::string note;
    std::string template_;
    std::vector<BitFieldRule> bitFields;
    std::map<std::string, std::string> enumMap;
    /** Optional shared lookup table id (e.g. iso3166, iso4217) from registry/iso-lookups.json */
    std::string enumRef;
};

class Logger;

class TagRegistry {
public:
    void setLogger(Logger* logger);

    /** Loads manifest base JSON then scheme overlay chain; later files override tag keys. */
    bool loadManifest(const std::string& manifestPath, const std::string& scheme);

    std::shared_ptr<TagInfo> getTagInfo(const std::string& tagHex) const;

    std::size_t tagCount() const { return tagMap_.size(); }

    std::vector<std::shared_ptr<TagInfo>> allTags() const;

private:
    Logger* logger_ = nullptr;

    Logger& logSink() const;

    struct MergeLayer {
        std::string filePath;
        std::string schemeLabel;
    };

    bool loadRegistryFile(const std::string& filePath, const MergeLayer& layer);

    std::unordered_map<std::string, std::shared_ptr<TagInfo>> tagMap_;
    std::unordered_map<std::string, std::string> tagSourceFile_;
};

#endif // TAGREGISTRY_H
