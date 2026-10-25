#pragma once

#if defined(BT_SERVER)

namespace engine
{

class ServerMonitorHost
{
public:

	ServerMonitorHost();
	~ServerMonitorHost();
	void Update();

private:

	struct TextSection
	{
		uint32_t uiColor = 0;
		std::string text;
	};

	struct Snapshot
	{
		std::vector<TextSection> statistics;
		std::vector<TextSection> profile;
		std::vector<common::ServerMonitorCell> cells;
	};

	std::shared_ptr<const Snapshot> Capture();
	void Run(std::stop_token stopToken, std::promise<void> startup);
	void Serialize(const Snapshot& rSnapshot);
	void Disconnect(std::string_view reason, bool bRejected = false);

	int64_t miUpdateCount = 0;
	std::atomic<bool> mbAuthenticated = false;
	std::mutex mSnapshotMutex;
	std::shared_ptr<const Snapshot> mpLatestSnapshot;
	std::string mSecretKey;
	bool mbLoopbackOnly = false;
	SOCKET mListenSocket = INVALID_SOCKET;
	SOCKET mMonitorSocket = INVALID_SOCKET;
	std::array<uint8_t, common::kiServerMonitorResponseSize> mResponse {};
	std::array<uint8_t, common::kiServerMonitorResponseSize> mExpectedResponse {};
	int64_t miResponseSize = 0;
	std::chrono::steady_clock::time_point mAcceptedTime;
	std::optional<std::chrono::steady_clock::time_point> mOptionalSendTime;
	std::vector<uint8_t> mSendBytes;
	int64_t miSentSize = 0;
	std::shared_ptr<const Snapshot> mpSentSnapshot;
	std::jthread mThread;
};

} // namespace engine

#endif // BT_SERVER
