#pragma once

#if defined(BT_SERVER)

namespace game
{

struct ReplayStagedMeta;

// The game half of the engine replay lifetime: metadata and client resynchronization.
bool WriteReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename);
bool ReadReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, ReplayStagedMeta& rStagedMetadata);
void AdoptReplayMetadata(const ReplayStagedMeta&& rStagedMetadata);
void OnStateReplaced();

} // namespace game

#endif // defined(BT_SERVER)
