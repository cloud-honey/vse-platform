#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <string>
#include <vector>

namespace vse {

/**
 * SaveMetadata — save file header.
 * Stored as first entry in the MessagePack payload.
 * Used for save slot display without loading the full game state.
 */
struct SaveMetadata {
    uint32_t    version = 1;       // Save format version
    std::string buildingName;
    GameTime    gameTime;
    int         starRating = 1;
    int64_t     balance    = 0;    // Cents
    uint64_t    playtimeSeconds = 0;
};

class ISaveLoad {
public:
    virtual ~ISaveLoad() = default;

    virtual Result<bool> save(const std::string& filepath) = 0;
    virtual Result<bool> load(const std::string& filepath) = 0;
    virtual Result<bool> quickSave() = 0;
    virtual Result<bool> quickLoad() = 0;
    virtual Result<SaveMetadata> readMetadata(const std::string& filepath) const = 0;
    virtual std::vector<std::string> listSaves() const = 0;
};

} // namespace vse
