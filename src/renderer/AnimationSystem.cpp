#include "renderer/AnimationSystem.h"
#include <spdlog/spdlog.h>
#include <cmath>

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
    
    int frameCount = endFrame - startFrame + 1;
    // Advance multiple frames if dt > frameDuration (prevents timer drift)
    if (frameDuration > 0.0f && anim.timer >= frameDuration) {
        int steps = static_cast<int>(anim.timer / frameDuration);
        anim.timer = std::fmod(anim.timer, frameDuration);
        if (frameCount > 1) {
            anim.frameIndex = startFrame + ((anim.frameIndex - startFrame + steps) % frameCount);
        } else {
            anim.frameIndex = startFrame;
        }
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
            // rest_0 only (frame 6) — distinct from elevator (frame 7)
            startFrame = 6;
            endFrame = 6;
            frameDuration = 0.8f;
            break;
            
        case AgentState::WaitingElevator:
        case AgentState::InElevator:
            // elevator_0 (frame 7, static)
            startFrame = 7;
            endFrame = 7;
            frameDuration = 1.0f;
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

void AnimationSystem::pruneStale(const std::unordered_set<EntityId>& activeIds) {
    for (auto it = animStates_.begin(); it != animStates_.end(); ) {
        if (activeIds.find(it->first) == activeIds.end()) {
            it = animStates_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace vse
