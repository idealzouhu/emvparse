#include "manifest_resolve.h"
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace {

bool isRegularFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

std::string joinPath(const std::string& dir, const std::string& file) {
    if (dir.empty()) {
        return file;
    }
    const char last = dir.back();
    if (last == '/' || last == '\\') {
        return dir + file;
    }
    return dir + "/" + file;
}

std::string manifestFromRegistryRoot(const std::string& root) {
    const std::string asFile = joinPath(root, "manifest.json");
    if (isRegularFile(asFile)) {
        return asFile;
    }
    const std::string inDir = joinPath(root, "tag-registry/manifest.json");
    if (isRegularFile(inDir)) {
        return inDir;
    }
    return {};
}

bool fileExists(const std::string& path) {
    return isRegularFile(path);
}

std::string parentDirectory(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return {};
    }
    if (pos == 0) {
        return path.substr(0, 1);
    }
    return path.substr(0, pos);
}

std::string findSourceTreeManifest(const std::string& startDir) {
    std::string dir = startDir;
    for (int depth = 0; depth < 12 && !dir.empty(); ++depth) {
        const std::string candidate = joinPath(dir, "registry/manifest.json");
        if (fileExists(candidate)) {
            return candidate;
        }
        const std::string parent = parentDirectory(dir);
        if (parent == dir) {
            break;
        }
        dir = parent;
    }
    return {};
}

} // namespace

std::string getExecutableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return ".";
    }
    std::string path(buffer, len);
    const size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return ".";
    }
    return path.substr(0, pos);
#else
    char buffer[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        return ".";
    }
    buffer[len] = '\0';
    std::string path(buffer);
    const size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return ".";
    }
    return path.substr(0, pos);
#endif
}

std::string resolveManifestPath(const std::string& cliOverride) {
    if (!cliOverride.empty()) {
        if (fileExists(cliOverride)) {
            return cliOverride;
        }
        const std::string fromDir = manifestFromRegistryRoot(cliOverride);
        if (!fromDir.empty()) {
            return fromDir;
        }
        return {};
    }

    if (const char* env = std::getenv("EMVPARSE_REGISTRY")) {
        const std::string root(env);
        if (fileExists(root)) {
            return root;
        }
        const std::string fromEnv = manifestFromRegistryRoot(root);
        if (!fromEnv.empty()) {
            return fromEnv;
        }
    }

    const std::string exeDir = getExecutableDirectory();

    const std::string shareRel = joinPath(exeDir, "../share/emvparse/tag-registry/manifest.json");
    if (fileExists(shareRel)) {
        return shareRel;
    }

    const std::string bundleRel = joinPath(exeDir, "tag-registry/manifest.json");
    if (fileExists(bundleRel)) {
        return bundleRel;
    }

#ifdef EMVPARSE_DEFAULT_REGISTRY_DIR
    {
        const std::string compiled = manifestFromRegistryRoot(EMVPARSE_DEFAULT_REGISTRY_DIR);
        if (!compiled.empty()) {
            return compiled;
        }
    }
#endif

    return findSourceTreeManifest(exeDir);
}
