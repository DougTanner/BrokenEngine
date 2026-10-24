#pragma once

#if defined(BT_SERVER)

namespace game
{

struct ReplayStagedMeta;

// The game half of the engine replay lifetime: metadata, the tick-keyed fleet records, the per-tick fleet checksums,
// and client resynchronization.
bool WriteReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename);
bool ReadReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, int64_t iNextGlobalId, ReplayStagedMeta& rStagedMetadata);
void AdoptReplayMetadata(ReplayStagedMeta&& rStagedMetadata);
void OnStateReplaced();

// Recording start, after the tick's writers exist and its fleet step ran.
void StartReplayRecording();
// Each dispatching playback tick, after its inputs and post-dispatch records are loaded.
void PrepareReplayPlaybackTick();
// After each recording or playback tick's CompleteTick.
void FinalizeReplayTick(bool bRecording);

} // namespace game

#endif // defined(BT_SERVER)
