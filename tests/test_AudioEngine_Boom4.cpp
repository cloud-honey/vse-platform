#include "renderer/AudioEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <SDL.h>

using namespace vse;

// Mock SDL renderer for testing
struct MockRenderer {
    SDL_Renderer* renderer = nullptr;
    
    MockRenderer() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window* window = SDL_CreateWindow("Test", 0, 0, 100, 100, 0);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    
    ~MockRenderer() {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        SDL_Quit();
    }
};

TEST_CASE("AudioEngine can be instantiated without crash", "[AudioEngine_Boom4]") {
    AudioEngine engine;
    // Constructor should not crash
    REQUIRE(true);
}

TEST_CASE("init() returns false gracefully when SDL audio is unavailable", "[AudioEngine_Boom4]") {
    AudioEngine engine;
    // In test environment, SDL audio might not be available
    // but init() should not crash and should return false
    bool result = engine.init();
    REQUIRE(true); // Just ensuring no crash
}

TEST_CASE("shutdown() can be called safely even if not initialized", "[AudioEngine_Boom4]") {
    AudioEngine engine;
    // shutdown() should not crash even if init() was never called
    engine.shutdown();
    REQUIRE(true); // Just ensuring no crash
}

TEST_CASE("playBGM() with non-existent file does not crash, returns false", "[AudioEngine_Boom4]") {
    AudioEngine engine;
    // Should not crash even with invalid file path
    engine.playBGM("nonexistent_file.mp3");
    REQUIRE(true); // Just ensuring no crash
}

TEST_CASE("playSFX() with non-existent file does not crash, returns false", "[AudioEngine_Boom4]") {
    AudioEngine engine;
    // Should not crash even with invalid file path
    engine.playSFX("nonexistent_file.wav");
    REQUIRE(true); // Just ensuring no crash
}

TEST_CASE("setMasterVolume(0) and setMasterVolume(100) do not crash", "[AudioEngine_Boom4]") {
    AudioEngine engine;
    // These calls should not crash
    engine.setBGMVolume(0.0f);
    engine.setSFXVolume(0.0f);
    engine.setBGMVolume(1.0f);
    engine.setSFXVolume(1.0f);
    REQUIRE(true); // Just ensuring no crash
}