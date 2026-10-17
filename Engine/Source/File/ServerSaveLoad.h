#pragma once

#if defined(BT_SERVER)

namespace engine
{

class ServerSaveLoad
{
public:

	bool ServerSave(const std::filesystem::path& rFilename); // appdata-relative; caller validates the bare filename
	bool ServerLoad(const std::filesystem::path& rFilename);
	void ServerReset();
	bool Autosave();
	void TickAutosave();
	void FinalAutosave();
	bool Autoload();

	static constexpr std::chrono::seconds kAutosaveInterval = 3'600s;
	common::Timer mAutosaveTimer;
	bool mbFinalAutosaveDone = false;
};

} // namespace engine

#endif // defined(BT_SERVER)
