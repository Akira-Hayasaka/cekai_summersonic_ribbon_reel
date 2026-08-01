#include "FontManager.h"

FontManager::~FontManager() {
    clear();
}

bool FontManager::setup() {
    if (ftLibrary) {
        return true;
    }

    const FT_Error err = FT_Init_FreeType(&ftLibrary);
    if (err != 0 || !ftLibrary) {
        ofLogError("ofxTypo") << "FT_Init_FreeType failed: " << err;
        ftLibrary = nullptr;
        return false;
    }

    return true;
}

std::shared_ptr<FontFace> FontManager::loadFont(
    const std::string &id,
    const std::filesystem::path &path,
    int faceIndex) {
    if (!ftLibrary && !setup()) {
        return nullptr;
    }

    auto font = std::make_shared<FontFace>();
    if (!font->load(ftLibrary, path, faceIndex)) {
        ofLogError("ofxTypo") << "Failed to load font id=" << id << " path=" << path.string();
        return nullptr;
    }

    font->setId(id);
    fonts[id] = font;
    return font;
}

std::shared_ptr<FontFace> FontManager::getFont(const std::string &id) const {
    const auto it = fonts.find(id);
    if (it == fonts.end()) {
        return nullptr;
    }
    return it->second;
}

void FontManager::addFallback(const std::string &primaryId, const std::string &fallbackId) {
    fallbackMap[primaryId].push_back(fallbackId);
}

const std::vector<std::string> &FontManager::getFallbacks(const std::string &primaryId) const {
    static const std::vector<std::string> kEmpty;
    const auto it = fallbackMap.find(primaryId);
    if (it == fallbackMap.end()) {
        return kEmpty;
    }
    return it->second;
}

void FontManager::clear() {
    fonts.clear();
    fallbackMap.clear();

    if (ftLibrary) {
        FT_Done_FreeType(ftLibrary);
        ftLibrary = nullptr;
    }
}
