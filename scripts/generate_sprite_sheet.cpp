#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output_path>" << std::endl;
        return 1;
    }
    
    std::string outputPath = argv[1];
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cerr << "SDL_image could not initialize! IMG_Error: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create a 128x32 surface (8 frames of 16x32)
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 128, 32, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        std::cerr << "Could not create surface! SDL_Error: " << SDL_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Fill with transparent background
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    
    // Define frame colors (matching SpriteSheet.cpp)
    struct FrameColor {
        uint8_t headR, headG, headB;
        uint8_t bodyR, bodyG, bodyB;
    };
    
    FrameColor frameColors[8] = {
        // idle_0: bright cyan
        { 50, 250, 255, 0, 200, 220 },
        // idle_1: slightly brighter cyan
        { 60, 255, 255, 10, 210, 230 },
        // walk_0: bright cyan
        { 50, 250, 255, 0, 200, 220 },
        // walk_1: slightly brighter cyan
        { 60, 255, 255, 10, 210, 230 },
        // work_0: bright green
        { 50, 255, 150, 0, 220, 100 },
        // work_1: slightly brighter green
        { 60, 255, 160, 10, 230, 110 },
        // rest_0: bright yellow
        { 255, 255, 100, 255, 220, 50 },
        // elevator_0: bright purple
        { 230, 150, 255, 180, 100, 255 }
    };
    
    // Draw each frame
    for (int frame = 0; frame < 8; frame++) {
        int xOffset = frame * 16;
        const FrameColor& color = frameColors[frame];
        
        // Draw body (bottom 70% = 22px, starting at y=10)
        SDL_Rect bodyRect = { xOffset, 10, 16, 22 };
        SDL_FillRect(surface, &bodyRect, 
                     SDL_MapRGBA(surface->format, 
                                 color.bodyR, color.bodyG, color.bodyB, 230));
        
        // Draw head (top 30% = 10px, width 12px centered)
        SDL_Rect headRect = { xOffset + 2, 0, 12, 10 };
        SDL_FillRect(surface, &headRect,
                     SDL_MapRGBA(surface->format,
                                 std::min(255, static_cast<int>(color.headR)),
                                 std::min(255, static_cast<int>(color.headG)),
                                 std::min(255, static_cast<int>(color.headB)),
                                 230));
        
        // Draw white outline
        SDL_Rect outlineRect = { xOffset, 0, 16, 32 };
        Uint32 outlineColor = SDL_MapRGBA(surface->format, 255, 255, 255, 180);
        
        // Draw outline lines
        SDL_Rect topLine = { xOffset, 0, 16, 1 };
        SDL_Rect bottomLine = { xOffset, 31, 16, 1 };
        SDL_Rect leftLine = { xOffset, 0, 1, 32 };
        SDL_Rect rightLine = { xOffset + 15, 0, 1, 32 };
        
        SDL_FillRect(surface, &topLine, outlineColor);
        SDL_FillRect(surface, &bottomLine, outlineColor);
        SDL_FillRect(surface, &leftLine, outlineColor);
        SDL_FillRect(surface, &rightLine, outlineColor);
    }
    
    // Save as PNG
    if (IMG_SavePNG(surface, outputPath.c_str()) != 0) {
        std::cerr << "Failed to save PNG: " << IMG_GetError() << std::endl;
        SDL_FreeSurface(surface);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    
    std::cout << "Sprite sheet generated: " << outputPath << std::endl;
    
    // Cleanup
    SDL_FreeSurface(surface);
    IMG_Quit();
    SDL_Quit();
    
    return 0;
}