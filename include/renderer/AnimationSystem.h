#pragma once

#include "core/Types.h"
#include <unordered_map>
#include <cstdint>

namespace vse {

/**
 * @brief Animation state for a single agent
 */
struct AnimationState {
    int frameIndex;        // Current frame index (0-7)
    float timer;           // Time accumulator for frame transitions
    float frameDuration;   // Current frame duration in seconds
    
    AnimationState() : frameIndex(0), timer(0.0f), frameDuration(0.5f) {}
};

/**
 * @brief Animation system for NPC sprite sheet rendering
 * 
 * Manages animation states for agents and determines which frame
 * to display based on agent state and elapsed time.
 */
class AnimationSystem {
public:
    AnimationSystem();
    
    /**
     * @brief Get the current frame for an agent
     * 
     * @param agentId Unique agent identifier
     * @param state Current agent state
     * @param dt Delta time in seconds
     * @return int Frame index (0-7)
     */
    int getFrame(EntityId agentId, AgentState state, float dt);
    
    /**
     * @brief Reset animation state for an agent
     * 
     * @param agentId Agent identifier
     */
    void resetAgent(EntityId agentId);
    
    /**
     * @brief Remove animation state for an agent
     * 
     * @param agentId Agent identifier
     */
    void removeAgent(EntityId agentId);
    
    /**
     * @brief Clear all animation states
     */
    void clear();
    
private:
    std::unordered_map<EntityId, AnimationState> animStates_;
    
    /**
     * @brief Get frame range and duration for an agent state
     * 
     * @param state Agent state
     * @param startFrame Output: first frame index
     * @param endFrame Output: last frame index
     * @param frameDuration Output: duration per frame in seconds
     */
    void getAnimationParams(AgentState state, int& startFrame, int& endFrame, float& frameDuration) const;
};

} // namespace vse