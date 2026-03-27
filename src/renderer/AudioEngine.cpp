/**
 * @file AudioEngine.cpp
 * @brief SDL2_mixer 기반 오디오 시스템 구현.
 * 
 * VSE_HAS_AUDIO=0일 때: 모든 메서드 no-op, isAvailable() false 반환
 * VSE_HAS_AUDIO=1일 때: SDL2_mixer 초기화 시도, 실패 시 graceful fallback
 */
#include "renderer/AudioEngine.h"
#include <spdlog/spdlog.h>

#if VSE_HAS_AUDIO == 1
#include <SDL_mixer.h>
#endif

namespace vse {

AudioEngine::AudioEngine() {
    // 생성자에서는 아무것도 하지 않음 — init()에서 초기화
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
#if VSE_HAS_AUDIO == 1
    if (audioInitialized_) {
        return true;
    }
    
    // SDL2_mixer 초기화
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        spdlog::warn("Failed to initialize SDL2_mixer: {}", Mix_GetError());
        audioInitialized_ = false;
        return false;
    }
    
    // 채널 수 설정 (효과음 동시 재생 가능 개수)
    Mix_AllocateChannels(16);
    
    spdlog::info("AudioEngine initialized successfully");
    audioInitialized_ = true;
    return true;
#else
    // VSE_HAS_AUDIO=0: 항상 false 반환
    return false;
#endif
}

void AudioEngine::shutdown() {
#if VSE_HAS_AUDIO == 1
    if (!audioInitialized_) {
        return;
    }
    
    // 배경음악 정리
    if (bgm_) {
        Mix_HaltMusic();
        Mix_FreeMusic(bgm_);
        bgm_ = nullptr;
    }
    
    // 효과음 캐시 정리
    for (auto& pair : sfxCache_) {
        Mix_FreeChunk(pair.second);
    }
    sfxCache_.clear();
    
    // SDL2_mixer 종료
    Mix_CloseAudio();
    audioInitialized_ = false;
    
    spdlog::info("AudioEngine shutdown");
#endif
}

void AudioEngine::playBGM(const std::string& path, bool loop) {
#if VSE_HAS_AUDIO == 1
    if (!audioInitialized_) {
        return;
    }
    
    // 현재 재생 중인 음악 정지
    stopBGM();
    
    // 새 음악 로드
    bgm_ = Mix_LoadMUS(path.c_str());
    if (!bgm_) {
        spdlog::warn("Failed to load BGM: {} - {}", path, Mix_GetError());
        return;
    }
    
    // 재생 (루프 설정)
    int loops = loop ? -1 : 0;
    if (Mix_PlayMusic(bgm_, loops) == -1) {
        spdlog::warn("Failed to play BGM: {} - {}", path, Mix_GetError());
        Mix_FreeMusic(bgm_);
        bgm_ = nullptr;
        return;
    }
    
    // 볼륨 설정 적용
    setBGMVolume(bgmVolume_);
    
    spdlog::debug("Playing BGM: {}", path);
#endif
}

void AudioEngine::stopBGM() {
#if VSE_HAS_AUDIO == 1
    if (!audioInitialized_) {
        return;
    }
    
    if (bgm_) {
        Mix_HaltMusic();
        Mix_FreeMusic(bgm_);
        bgm_ = nullptr;
        spdlog::debug("BGM stopped");
    }
#endif
}

void AudioEngine::playSFX(const std::string& path) {
#if VSE_HAS_AUDIO == 1
    if (!audioInitialized_) {
        return;
    }
    
    // 캐시에서 찾기
    Mix_Chunk* chunk = nullptr;
    auto it = sfxCache_.find(path);
    if (it != sfxCache_.end()) {
        chunk = it->second;
    } else {
        // 새로 로드
        chunk = Mix_LoadWAV(path.c_str());
        if (!chunk) {
            spdlog::warn("Failed to load SFX: {} - {}", path, Mix_GetError());
            return;
        }
        sfxCache_[path] = chunk;
    }
    
    // 재생 (볼륨 설정 적용)
    int channel = Mix_PlayChannel(-1, chunk, 0);
    if (channel == -1) {
        spdlog::warn("Failed to play SFX: {} - {}", path, Mix_GetError());
        return;
    }
    
    // 채널 볼륨 설정
    Mix_Volume(channel, static_cast<int>(sfxVolume_ * MIX_MAX_VOLUME));
    
    spdlog::debug("Playing SFX: {}", path);
#endif
}

bool AudioEngine::isAvailable() const {
#if VSE_HAS_AUDIO == 1
    return audioInitialized_;
#else
    return false;
#endif
}

void AudioEngine::setBGMVolume(float v) {
#if VSE_HAS_AUDIO == 1
    // 클램핑: 0.0f ~ 1.0f
    bgmVolume_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    
    if (audioInitialized_) {
        Mix_VolumeMusic(static_cast<int>(bgmVolume_ * MIX_MAX_VOLUME));
    }
#endif
}

void AudioEngine::setSFXVolume(float v) {
#if VSE_HAS_AUDIO == 1
    // 클램핑: 0.0f ~ 1.0f
    sfxVolume_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    
    if (audioInitialized_) {
        // 모든 채널에 볼륨 적용
        Mix_Volume(-1, static_cast<int>(sfxVolume_ * MIX_MAX_VOLUME));
    }
#endif
}

} // namespace vse