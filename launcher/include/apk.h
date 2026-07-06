#pragma once
#include <string>
#include <vector>

// Which native library ABI(s) an APK ships. Android Horizon's engine only
// loads AArch64 (arm64-v8a) code — detected up front during scanning so the
// launcher can tag/gray out anything that can't run, instead of only finding
// out after a full extraction attempt.
enum class ApkArch {
    Unknown,     // couldn't tell (no lib/ folder found, e.g. a pure-Java/Kotlin app)
    Arm64,       // has arm64-v8a — supported
    Arm32Only,   // has armeabi-v7a/x86/etc. but no arm64-v8a — not supported yet
};

struct ApkInfo {
    std::string filename;
    std::string path;
    std::string appName;
    std::string packageName;
    std::string versionName;
    std::vector<uint8_t> iconPng;
    uint64_t    fileSizeBytes = 0;
    bool        installed     = false;  // true if already extracted to games dir
    ApkArch     arch          = ApkArch::Unknown;
};

ApkInfo              parseApk       (const std::string& path);
std::vector<ApkInfo> scanApks       (const std::string& dir);

// Returns true if <pkg_name> has an .installed marker in the games directory.
bool                 apkIsInstalled (const std::string& pkg_name);
