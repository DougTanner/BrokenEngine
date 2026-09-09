#pragma once

#if defined(BT_SERVER)

namespace engine
{

class GameBase;

}

namespace game
{

struct ReplayStagedMeta;

// The game half of the engine replay lifetime: metadata, replay-only game fixtures, and client resynchronization.
bool WriteReplayMeta(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename);
bool ReadReplayMeta(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, ReplayStagedMeta& rStagedMeta);
void AdoptReplayMeta(ReplayStagedMeta&& rStagedMeta);
void OnReplayStreamsInvalidated();
void OnStateReplaced();

class GameSaveLoad
{
public:

	GameSaveLoad(engine::GameBase& rGameBase);

	bool ServerSave();
	bool ServerLoad();
	bool ServerSave(const std::filesystem::path& rFilename); // appdata-relative; caller validates the bare filename
	bool ServerLoad(const std::filesystem::path& rFilename);
	void ServerReset();
	bool Autosave();
	void TickAutosave();
	bool Autoload();

private:

	engine::GameBase& mrGameBase;

	static constexpr std::chrono::seconds kAutosaveInterval = 3600s;
	common::Timer mAutosaveTimer;
};

} // namespace game

#endif // defined(BT_SERVER)
