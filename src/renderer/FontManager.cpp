#include "renderer/FontManager.h"
#include <SDL_ttf.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace vse {

namespace fs = std::filesystem;

FontManager::FontManager() : font_(nullptr), size_(0) {
}

FontManager::~FontManager() {
    // unique_ptr with custom deleter will handle cleanup
}

bool FontManager::load(const std::string& path, int size) {
    // 이미 로드된 폰트가 있으면 해제
    font_.reset();
    size_ = 0;

    if (size <= 0) {
        spdlog::warn("FontManager::load: invalid font size {}", size);
        return false;
    }

    // 경로 확인
    fs::path fontPath(path);
    if (!fs::exists(fontPath)) {
        spdlog::warn("FontManager::load: font file not found: {}", path);
        return false;
    }

    // SDL2_ttf 초기화 확인
    if (!TTF_WasInit() && TTF_Init() == -1) {
        spdlog::warn("FontManager::load: TTF_Init failed: {}", TTF_GetError());
        return false;
    }

    // 폰트 로드
    TTF_Font* rawFont = TTF_OpenFont(path.c_str(), size);
    if (!rawFont) {
        spdlog::warn("FontManager::load: TTF_OpenFont failed: {}", TTF_GetError());
        return false;
    }

    font_.reset(rawFont);
    size_ = size;
    spdlog::info("FontManager::load: loaded {} (size={})", path, size);
    return true;
}

void FontManager::FontDeleter::operator()(TTF_Font* font) const {
    if (font) {
        TTF_CloseFont(font);
    }
}

} // namespace vse