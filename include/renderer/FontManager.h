#pragma once
#include <string>
#include <memory>

// Forward declaration to avoid including SDL_ttf.h in header
struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;

namespace vse {

/**
 * FontManager — SDL2_ttf 폰트 로드 및 관리.
 *
 * 설계 원칙:
 * - 폰트 파일이 없거나 로드 실패 시 isLoaded() == false 반환
 * - get()은 항상 null이 아닌 포인터 반환 (로드 실패 시 nullptr)
 * - 렌더러는 isLoaded() 확인 후 폰트 사용
 * - 단일 폰트만 관리 (Phase 1 요구사항)
 */
class FontManager {
public:
    FontManager();
    ~FontManager();

    // 복사/이동 금지
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

    /**
     * TTF 폰트 파일 로드.
     * @param path 폰트 파일 경로 (실행 파일 기준 상대 경로)
     * @param size 폰트 크기 (pt)
     * @return 로드 성공 시 true, 실패 시 false
     */
    bool load(const std::string& path, int size);

    /**
     * 내부 TTF_Font 포인터 반환.
     * @return 로드된 폰트 포인터, 로드 실패 시 nullptr
     */
    TTF_Font* get() const { return font_.get(); }

    /**
     * 폰트가 성공적으로 로드되었는지 확인.
     * @return true if font is loaded and ready to use
     */
    bool isLoaded() const { return font_ != nullptr; }

    /**
     * 현재 폰트 크기 반환.
     * @return 폰트 크기 (pt), 로드 실패 시 0
     */
    int getSize() const { return size_; }

private:
    struct FontDeleter {
        void operator()(TTF_Font* font) const;
    };

    std::unique_ptr<TTF_Font, FontDeleter> font_;
    int size_ = 0;
};

} // namespace vse