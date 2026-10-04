#pragma once

#if defined(BT_SERVER)

namespace game
{

struct ReplayStagedMeta;

// The game half of the engine replay lifetime: metadata, replay-only game fixtures, and client resynchronization.
bool WriteReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename);
bool ReadReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, ReplayStagedMeta& rStagedMetadata);
void AdoptReplayMetadata(const ReplayStagedMeta&& rStagedMetadata);
void OnReplayStreamsInvalidated();
void OnStateReplaced();

class GameSaveLoad
{
public:

	bool ServerSave(const std::filesystem::path& rFilename); // appdata-relative; caller validates the bare filename
	bool ServerLoad(const std::filesystem::path& rFilename);
	void ServerReset();
	bool Autosave();
	void TickAutosave();
	bool Autoload();

	static constexpr std::chrono::seconds kAutosaveInterval = 3'600s;
	common::Timer mAutosaveTimer;
};

} // namespace game

#endif // defined(BT_SERVER)
