#pragma once

#include "FontFace.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

class FontManager {
public:
    FontManager() = default;
    ~FontManager();

    bool setup();

    std::shared_ptr<FontFace> loadFont(
        const std::string &id,
        const std::filesystem::path &path,
        int faceIndex = 0);

    std::shared_ptr<FontFace> getFont(const std::string &id) const;

    void addFallback(const std::string &primaryId, const std::string &fallbackId);
    const std::vector<std::string> &getFallbacks(const std::string &primaryId) const;

    void clear();

private:
    FT_Library ftLibrary = nullptr;
    std::unordered_map<std::string, std::shared_ptr<FontFace>> fonts;
    std::unordered_map<std::string, std::vector<std::string>> fallbackMap;
};
