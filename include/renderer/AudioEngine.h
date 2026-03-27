#pragma once
#include <string>
#include <unordered_map>

// Forward declarations for SDL2_mixer types
struct Mix_Music;
struct Mix_Chunk;

namespace vse {

/**
 * AudioEngine — SDL2_mixer 기반 오디오 시스템.
 * 
 * VSE_HAS_AUDIO=0일 때: 모든 메서드 no-op, isAvailable() false 반환
 * VSE_HAS_AUDIO=1일 때: SDL2_mixer 초기화 시도, 실패 시 graceful fallback
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // 초기화 — 오디오 사용 불가 시 false 반환 (비치명적)
    bool init();
    
    // 정리
    void shutdown();

    // 배경음악 재생/정지
    void playBGM(const std::string& path, bool loop = true);
    void stopBGM();
    
    // 효과음 재생
    void playSFX(const std::string& path);

    // 오디오 사용 가능 여부
    bool isAvailable() const;

    // 볼륨 설정: 0.0f ~ 1.0f
    void setBGMVolume(float v);
    void setSFXVolume(float v);

private:
#if VSE_HAS_AUDIO == 1
    // SDL2_mixer가 사용 가능할 때만 컴파일되는 멤버
    Mix_Music* bgm_ = nullptr;
    std::unordered_map<std::string, Mix_Chunk*> sfxCache_;
    
    // 현재 볼륨 설정
    float bgmVolume_ = 0.7f;
    float sfxVolume_ = 0.8f;
    
    // 오디오 초기화 성공 여부
    bool audioInitialized_ = false;
#endif
};

} // namespace vse