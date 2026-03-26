#include "renderer/AnimationSystem.h"
#include <spdlog/spdlog.h>

namespace vse {

AnimationSystem::AnimationSystem() = default;

int AnimationSystem::getFrame(EntityId agentId, AgentState state, float dt) {
    // Get or create animation state for this agent
    auto it = animStates_.find(agentId);
    if (it == animStates_.end()) {
        // Create new animation state — initialize to current state's startFrame
        int startFrame, endFrame;
        float frameDuration;
        getAnimationParams(state, startFrame, endFrame, frameDuration);
        AnimationState animState;
        animState.frameIndex = startFrame;
        animState.frameDuration = frameDuration;
        animStates_[agentId] = animState;
        it = animStates_.find(agentId);
    }
    
    AnimationState& anim = it->second;
    
    // Get animation parameters for this state
    int startFrame, endFrame;
    float frameDuration;
    getAnimationParams(state, startFrame, endFrame, frameDuration);

    // If state changed (frame out of range for this state), reset to startFrame
    if (anim.frameIndex < startFrame || anim.frameIndex > endFrame) {
        anim.frameIndex = startFrame;
        anim.timer = 0.0f;
    }

    // Update timer
    anim.timer += dt;
    
    // Check if we need to advance to next frame
    if (anim.timer >= frameDuration) {
        anim.timer = 0.0f;
        
        // Calculate number of frames in this animation
        int frameCount = endFrame - startFrame + 1;
        
        if (frameCount > 1) {
            // Animated state: cycle through frames
            anim.frameIndex = startFrame + ((anim.frameIndex - startFrame + 1) % frameCount);
        } else {
            // Static state: stay on the same frame
            anim.frameIndex = startFrame;
        }
        
        // Update frame duration (in case state changed)
        anim.frameDuration = frameDuration;
    }
    
    return anim.frameIndex;
}

void AnimationSystem::resetAgent(EntityId agentId) {
    auto it = animStates_.find(agentId);
    if (it != animStates_.end()) {
        it->second = AnimationState();
    }
}

void AnimationSystem::removeAgent(EntityId agentId) {
    animStates_.erase(agentId);
}

void AnimationSystem::clear() {
    animStates_.clear();
}

void AnimationSystem::getAnimationParams(AgentState state, int& startFrame, int& endFrame, float& frameDuration) const {
    switch (state) {
        case AgentState::Idle:
            // idle_0, idle_1
            startFrame = 0;
            endFrame = 1;
            frameDuration = 0.5f; // 0.5 seconds per frame
            break;
            
        case AgentState::Working:
            // work_0, work_1
            startFrame = 4;
            endFrame = 5;
            frameDuration = 0.3f; // 0.3 seconds per frame (faster)
            break;
            
        case AgentState::Resting:
            // rest_0, rest_1 (using rest_0 and elevator_0 as rest frames)
            startFrame = 6;
            endFrame = 7;
            frameDuration = 0.8f; // 0.8 seconds per frame (slower)
            break;
            
        case AgentState::WaitingElevator:
        case AgentState::InElevator:
            // elevator_0 (static)
            startFrame = 7;
            endFrame = 7;
            frameDuration = 1.0f; // Not used for static frame
            break;
            
        default:
            // Walking or any movement state
            // walk_0, walk_1
            startFrame = 2;
            endFrame = 3;
            frameDuration = 0.2f; // 0.2 seconds per frame (fastest)
            break;
    }
}

} // namespace vse