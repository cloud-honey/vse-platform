#pragma once

#include <SDL.h>
#include <string>
#include <memory>

namespace vse {

/**
 * @brief Sprite sheet loader and renderer with graceful fallback
 * 
 * Loads a sprite sheet texture via SDL2_image. If loading fails,
 * falls back to colored rectangles matching the current NPC rendering.
 */
class SpriteSheet {
public:
    /**
     * @brief Construct a new Sprite Sheet object
     * 
     * @param renderer SDL_Renderer to use for texture creation
     * @param path Path to the sprite sheet PNG file
     */
    SpriteSheet(SDL_Renderer* renderer, const std::string& path);
    
    ~SpriteSheet();
    
    /**
     * @brief Draw a specific frame from the sprite sheet
     * 
     * @param frameIndex Index of the frame to draw (0-7)
     * @param x Screen X position
     * @param y Screen Y position
     * @param w Width to draw
     * @param h Height to draw
     */
    void drawFrame(int frameIndex, float x, float y, float w, float h);
    
    /**
     * @brief Check if the sprite sheet texture is loaded
     * 
     * @return true Texture is loaded and ready
     * @return false Texture failed to load, using fallback
     */
    bool isLoaded() const { return texture_ != nullptr; }
    
    /**
     * @brief Get the frame count in the sprite sheet
     * 
     * @return int Number of frames (8 for NPC sheet)
     */
    int getFrameCount() const { return frameCount_; }
    
    /**
     * @brief Get the width of a single frame
     * 
     * @return int Frame width in pixels
     */
    int getFrameWidth() const { return frameWidth_; }
    
    /**
     * @brief Get the height of a single frame
     * 
     * @return int Frame height in pixels
     */
    int getFrameHeight() const { return frameHeight_; }
    
private:
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    int frameWidth_;
    int frameHeight_;
    int frameCount_;
    
    // Fallback colors for each frame (matching current NPC colors)
    struct FrameColor {
        uint8_t headR, headG, headB;
        uint8_t bodyR, bodyG, bodyB;
    };
    
    static const FrameColor frameColors_[8];
    
    /**
     * @brief Draw fallback colored rectangle for a frame
     * 
     * @param frameIndex Index of the frame
     * @param x Screen X position
     * @param y Screen Y position
     * @param w Width to draw
     * @param h Height to draw
     */
    void drawFallback(int frameIndex, float x, float y, float w, float h);
    
    // Disable copying
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet& operator=(const SpriteSheet&) = delete;
};

} // namespace vse