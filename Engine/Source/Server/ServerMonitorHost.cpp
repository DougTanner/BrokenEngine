#include "Pch.h"

#if defined(BT_SERVER)

#include "Server/ServerMonitorHost.h"

#include "Network/Server/Server.h"

#include "Frame/ServerCellStats.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace engine
{

static std::array<int64_t, std::size(game::kServerEntityCountNames)> sTotalEntityCounts {};

static void UpdateStatistics()
{
	if constexpr (!kbProfiling)
	{
		return;
	}

	sTotalEntityCounts.fill(0);
	int64_t iTotalExplosions = 0;

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCells.at(rCoordinate).pCurrent));
		for (int64_t i = 0; i < std::ssize(sTotalEntityCounts); ++i)
		{
			sTotalEntityCounts[i] += cellStatistics.entityCounts[i];
		}
		iTotalExplosions += cellStatistics.iExplosions;
	}

	gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount = iTotalExplosions;

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	mi_stats_t mimallocStatistics = {};
	mimallocStatistics.size = sizeof(mi_stats_t);
	mimallocStatistics.version = MI_STAT_VERSION;
	mi_stats_get(&mimallocStatistics);

	gpProfileManager->miMimallocCommittedMebibytes = mimallocStatistics.committed.current / (1'024 * 1'024);
	gpProfileManager->miMimallocPeakCommittedMebibytes = mimallocStatistics.committed.peak / (1'024 * 1'024);
	gpProfileManager->miMimallocHeapUsedMebibytes = mimallocStatistics.page_committed.current / (1'024 * 1'024);
	gpProfileManager->miMimallocPeakHeapUsedMebibytes = mimallocStatistics.page_committed.peak / (1'024 * 1'024);
#endif

	gpProfileManager->UpdateProfileText();
}

ServerMonitorHost::ServerMonitorHost()
{
	std::string secret = common::GetFileOrStringContent(gpFileManager->mAppDataDirectory / L"ServerMonitorSecret.txt").second;
	mSecretKey = common::ServerMonitorSecretKey(secret);
	mbLoopbackOnly = mSecretKey.empty() || (gLaunchOptions.flags & LaunchOptionFlags::kLoopbackOnly);
	std::promise<void> startup;
	std::future<void> ready = startup.get_future();
	mThread = std::jthread(common::ThreadLocal::Entry([this, startup = std::move(startup)](std::stop_token stopToken) mutable
	{
		Run(stopToken, std::move(startup));
	}, 64 * 1'024));
	ready.get();
}

ServerMonitorHost::~ServerMonitorHost()
{
	mThread.request_stop();
	mThread.join();
}

void ServerMonitorHost::Update()
{
	UpdateStatistics();
	if (++miUpdateCount < 8)
	{
		return;
	}
	miUpdateCount = 0;
	if (!mbAuthenticated.load(std::memory_order_relaxed))
	{
		return;
	}

	std::shared_ptr<const Snapshot> pSnapshot = Capture();
	{
		std::lock_guard lock(mSnapshotMutex);
		mpLatestSnapshot.swap(pSnapshot);
	}
}

std::shared_ptr<const ServerMonitorHost::Snapshot> ServerMonitorHost::Capture()
{
	auto pSnapshot = std::make_shared<Snapshot>();
#if !defined(ENABLE_CRT_DEBUG_HEAP)
	pSnapshot->statistics.reserve(4);
	pSnapshot->profile.reserve(4);
#else
	pSnapshot->statistics.reserve(3);
	pSnapshot->profile.reserve(3);
#endif
	int64_t iMicroseconds = gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average();
	int64_t iPotential = iMicroseconds > 0 ? 1'000'000 / iMicroseconds : 0;
	int64_t iFps = gpProfileManager->mFullUpdatesInTheLastSecond.Get();
	game::ServerCellStats origin = game::GetServerCellStatistics(*game::gpGame->mCells.at(kOriginCoordinate).pCurrent);
	pSnapshot->statistics.push_back({.uiColor = RGB(100, 180, 255), .text = std::format("FPS: {}\nPotential: {}\n\n", iFps, iPotential)});
	pSnapshot->statistics.push_back({.uiColor = RGB(200, 200, 200), .text = std::format("Tick: {}\nTime: {:.2f} s\nActive cells: {}\nClients: {}\n", origin.iTick, origin.durationCurrentTime.count(), std::ssize(game::gpGame->mActiveCoordinates), std::ssize(gpServer->mClients))});
	std::string entities;
	for (int64_t i = 0; i < std::ssize(sTotalEntityCounts); ++i)
	{
		entities += std::format("{}: {}\n", game::kServerEntityCountNames[i], sTotalEntityCounts[i]);
	}
	entities += std::format("Explosions: {}\n", gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount);
	pSnapshot->statistics.push_back({.uiColor = RGB(150, 220, 150), .text = std::move(entities)});
	pSnapshot->profile.push_back({.uiColor = RGB(100, 180, 255), .text = std::format("FPS: {}  Potential: {}\n", iFps, iPotential)});
	bool bReevaluate = gpProfileManager->TickVisibilityCadence();
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	{
		common::ScopedWorkbufferArena arena = rWorkbuffer.Push();
		FormatCpuTimersText(rWorkbuffer, bReevaluate);
		pSnapshot->profile.push_back({.uiColor = RGB(220, 220, 220), .text = std::string(rWorkbuffer.View())});
	}
	{
		common::ScopedWorkbufferArena arena = rWorkbuffer.Push();
		FormatCpuCountersText(rWorkbuffer, bReevaluate);
		pSnapshot->profile.push_back({.uiColor = RGB(150, 220, 150), .text = std::string(rWorkbuffer.View())});
	}
#if !defined(ENABLE_CRT_DEBUG_HEAP)
	std::string memory = std::format("Committed: {} MiB\nPeak cmtd: {} MiB\nHeap used: {} MiB\nPeak heap: {} MiB\n", gpProfileManager->miMimallocCommittedMebibytes, gpProfileManager->miMimallocPeakCommittedMebibytes, gpProfileManager->miMimallocHeapUsedMebibytes, gpProfileManager->miMimallocPeakHeapUsedMebibytes);
	pSnapshot->statistics.push_back({.uiColor = RGB(220, 180, 100), .text = "\n" + memory});
	pSnapshot->profile.push_back({.uiColor = RGB(220, 180, 100), .text = std::move(memory)});
#endif

	std::unordered_map<GridCoord, common::ServerMonitorCell> cells;
	auto Cell = [&cells](const GridCoord& rCoordinate) -> common::ServerMonitorCell&
	{
		return cells.try_emplace(rCoordinate, common::ServerMonitorCell {.iX = rCoordinate.iQ, .iY = rCoordinate.iR, .uiClientCount = 0, .uiFlags = 0, .iEntityCount = 0}).first->second;
	};
	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		common::ServerMonitorCell& rCell = Cell(rCoordinate);
		rCell.uiFlags |= common::kuiServerMonitorCellActive;
		game::ServerCellStats statistics = game::GetServerCellStatistics(*game::gpGame->mCells.at(rCoordinate).pCurrent);
		rCell.iEntityCount = statistics.iExplosions;
		for (int64_t iCount : statistics.entityCounts)
		{
			rCell.iEntityCount += iCount;
		}
	}
	for (const ClientConnection& rClient : gpServer->mClients)
	{
		for (const GridCoord& rCoordinate : rClient.authorizedCoordinates)
		{
			++Cell(rCoordinate).uiClientCount;
		}
		for (const ClientConnection::SlotState& rSlot : rClient.slots)
		{
			if (rSlot.subscription.flags & SubscriptionFlags::kActive)
			{
				Cell(rSlot.subscription.coordinate).uiFlags |= common::kuiServerMonitorCellSubscribed;
			}
		}
	}
	pSnapshot->cells.reserve(cells.size());
	for (const auto& [rCoordinate, rCell] : cells)
	{
		pSnapshot->cells.push_back(rCell);
	}
	return pSnapshot;
}

void ServerMonitorHost::Serialize(const Snapshot& rSnapshot)
{
	mSendBytes.clear();
	miSentSize = 0;
	mOptionalSendTime.reset();
	auto Append = [this](const auto& rValue)
	{
		const uint8_t* pBytes = reinterpret_cast<const uint8_t*>(&rValue);
		mSendBytes.append_range(std::span(pBytes, sizeof(rValue)));
	};
	Append(common::ServerMonitorFrameHeader {.uiSize = 0, .eType = common::ServerMonitorMessageType::kSnapshot});
	Append(common::ServerMonitorSnapshotHeader {.uiStatisticsSectionCount = static_cast<uint32_t>(std::ssize(rSnapshot.statistics)), .uiProfileSectionCount = static_cast<uint32_t>(std::ssize(rSnapshot.profile)), .uiCellCount = static_cast<uint32_t>(std::ssize(rSnapshot.cells))});
	auto Sections = [this, &Append](const std::vector<TextSection>& rSections)
	{
		for (const TextSection& rSection : rSections)
		{
			Append(common::ServerMonitorTextSection {.uiColor = rSection.uiColor, .uiTextSize = static_cast<uint32_t>(rSection.text.size())});
			mSendBytes.append_range(rSection.text);
		}
	};
	Sections(rSnapshot.statistics);
	Sections(rSnapshot.profile);
	for (const common::ServerMonitorCell& rCell : rSnapshot.cells)
	{
		Append(rCell);
	}
	ASSERT(std::ssize(mSendBytes) - static_cast<int64_t>(sizeof(common::ServerMonitorFrameHeader)) <= common::kuiServerMonitorMaximumFrameSize);
	uint32_t uiBodySize = static_cast<uint32_t>(std::ssize(mSendBytes) - static_cast<int64_t>(sizeof(common::ServerMonitorFrameHeader)));
	std::memcpy(mSendBytes.data(), &uiBodySize, sizeof(uiBodySize));
}

void ServerMonitorHost::Disconnect(std::string_view reason, bool bRejected)
{
	if (bRejected)
	{
		LOG(kNetwork, kWarning, "ServerMonitorHost rejected ({})", reason);
	}
	else
	{
		LOG(kNetwork, kInfo, "ServerMonitorHost disconnected {}", reason);
	}
	closesocket(mMonitorSocket);
	mMonitorSocket = INVALID_SOCKET;
	mbAuthenticated.store(false, std::memory_order_relaxed);
	mSendBytes.clear();
	miSentSize = 0;
	miResponseSize = 0;
	mOptionalSendTime.reset();
	mpSentSnapshot.reset();
}

void ServerMonitorHost::Run(std::stop_token stopToken, std::promise<void> startup)
{
	// Heap: owned wire buffers, socket APIs, HMAC, and worker logging allocate independently of snapshot capture.
	ScopedSuppressAllocationTracking suppress;
	common::ScopedLambda closeSockets([this]()
	{
		if (mMonitorSocket != INVALID_SOCKET)
		{
			closesocket(mMonitorSocket);
		}
		if (mListenSocket != INVALID_SOCKET)
		{
			closesocket(mListenSocket);
		}
	});
	try
	{
		auto Fail = [](std::string_view operation)
		{
			std::string message = std::format("ServerMonitorHost {} failed: {}", operation, WSAGetLastError());
			LOG(kNetwork, kError, "{}", message);
			throw std::runtime_error(message);
		};
		mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mListenSocket == INVALID_SOCKET)
		{
			Fail("socket");
		}
		sockaddr_in address {};
		address.sin_family = AF_INET;
		address.sin_port = htons(common::kiServerMonitorPort);
		address.sin_addr.s_addr = htonl(mbLoopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
		if (bind(mListenSocket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
		{
			Fail("bind");
		}
		if (listen(mListenSocket, 1) == SOCKET_ERROR)
		{
			Fail("listen");
		}
		u_long uiNonblocking = 1;
		if (ioctlsocket(mListenSocket, FIONBIO, &uiNonblocking) == SOCKET_ERROR)
		{
			Fail("ioctlsocket");
		}
		LOG(kNetwork, kInfo, "ServerMonitorHost listening on {}:{} ({})", mbLoopbackOnly ? "127.0.0.1" : "0.0.0.0", common::kiServerMonitorPort, mSecretKey.empty() ? "no ServerMonitorSecret.txt: loopback only" : "secret required");
		startup.set_value();
	}
	catch (...)
	{
		startup.set_exception(std::current_exception());
		return;
	}

	while (!stopToken.stop_requested())
	{
		bool bAuthenticated = mbAuthenticated.load(std::memory_order_relaxed);
		if (bAuthenticated && mSendBytes.empty())
		{
			std::shared_ptr<const Snapshot> pSnapshot;
			{
				std::lock_guard lock(mSnapshotMutex);
				pSnapshot = mpLatestSnapshot;
			}
			if (pSnapshot != nullptr && pSnapshot != mpSentSnapshot)
			{
				Serialize(*pSnapshot);
				mpSentSnapshot = std::move(pSnapshot);
			}
		}
		SOCKET socketPoll = mMonitorSocket == INVALID_SOCKET ? mListenSocket : mMonitorSocket;
		fd_set readSet {};
		FD_ZERO(&readSet);
		FD_SET(socketPoll, &readSet);
		fd_set writeSet {};
		FD_ZERO(&writeSet);
		if (!mSendBytes.empty())
		{
			FD_SET(socketPoll, &writeSet);
		}
		timeval timeout {.tv_sec = 0, .tv_usec = 50'000};
		int64_t iSelected = select(0, &readSet, &writeSet, nullptr, &timeout);
		if (stopToken.stop_requested())
		{
			break;
		}
		if (iSelected == SOCKET_ERROR)
		{
			if (mMonitorSocket != INVALID_SOCKET)
			{
				Disconnect("select error");
			}
			continue;
		}
		if (mMonitorSocket == INVALID_SOCKET)
		{
			if (!FD_ISSET(mListenSocket, &readSet))
			{
				continue;
			}
			sockaddr_in address {};
			int iAddressSize = sizeof(address);
			mMonitorSocket = accept(mListenSocket, reinterpret_cast<sockaddr*>(&address), &iAddressSize);
			if (mMonitorSocket == INVALID_SOCKET)
			{
				continue;
			}
			u_long uiNonblocking = 1;
			if (ioctlsocket(mMonitorSocket, FIONBIO, &uiNonblocking) == SOCKET_ERROR)
			{
				Disconnect("ioctlsocket error");
				continue;
			}
			char pcAddress[INET_ADDRSTRLEN] {};
			inet_ntop(AF_INET, &address.sin_addr, pcAddress, sizeof(pcAddress));
			LOG(kNetwork, kInfo, "ServerMonitorHost connection from {}", pcAddress);
			common::ServerMonitorChallenge challenge {.uiMagic = common::kuiServerMonitorMagic, .uiVersion = common::kuiServerMonitorProtocolVersion, .nonce = {}};
			NTSTATUS iStatus = BCryptGenRandom(nullptr, challenge.nonce.data(), static_cast<ULONG>(challenge.nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
			ASSERT(BCRYPT_SUCCESS(iStatus));
			if (!mSecretKey.empty())
			{
				bool bComputed = common::ComputeServerMonitorResponse(mSecretKey, challenge.nonce, mExpectedResponse);
				ASSERT(bComputed);
			}
			common::ServerMonitorFrameHeader header {.uiSize = sizeof(challenge), .eType = common::ServerMonitorMessageType::kChallenge};
			mSendBytes.resize(sizeof(header) + sizeof(challenge));
			std::memcpy(mSendBytes.data(), &header, sizeof(header));
			std::memcpy(mSendBytes.data() + sizeof(header), &challenge, sizeof(challenge));
			mAcceptedTime = std::chrono::steady_clock::now();
			continue;
		}
		if (!bAuthenticated && std::chrono::steady_clock::now() - mAcceptedTime >= 5s)
		{
			Disconnect("response timeout", true);
			continue;
		}
		if (mOptionalSendTime && std::chrono::steady_clock::now() - *mOptionalSendTime >= 3s)
		{
			Disconnect("send timeout");
			continue;
		}
		if (FD_ISSET(mMonitorSocket, &readSet))
		{
			uint8_t uiUnexpected = 0;
			char* pDestination = bAuthenticated ? reinterpret_cast<char*>(&uiUnexpected) : reinterpret_cast<char*>(mResponse.data()) + miResponseSize;
			int64_t iReceived = recv(mMonitorSocket, pDestination, bAuthenticated ? 1 : static_cast<int>(mResponse.size() - miResponseSize), 0);
			if (iReceived == 0 || (iReceived == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
			{
				Disconnect("receive closed/error");
				continue;
			}
			if (iReceived > 0)
			{
				if (bAuthenticated)
				{
					Disconnect("unexpected data", true);
					continue;
				}
				miResponseSize += iReceived;
				if (miResponseSize == std::ssize(mResponse))
				{
					if (!mSecretKey.empty() && mResponse != mExpectedResponse)
					{
						Disconnect("wrong response", true);
						continue;
					}
					mbAuthenticated.store(true, std::memory_order_relaxed);
					LOG(kNetwork, kInfo, "ServerMonitorHost authenticated");
				}
			}
		}
		if (!mSendBytes.empty())
		{
			if (!mOptionalSendTime)
			{
				mOptionalSendTime = std::chrono::steady_clock::now();
			}
			{
				int64_t iSent = send(mMonitorSocket, reinterpret_cast<const char*>(mSendBytes.data()) + miSentSize, static_cast<int>(std::ssize(mSendBytes) - miSentSize), 0);
				if (iSent == 0 || (iSent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
				{
					Disconnect("send closed/error");
					continue;
				}
				if (iSent > 0)
				{
					miSentSize += iSent;
				}
			}
			if (miSentSize == std::ssize(mSendBytes))
			{
				mSendBytes.clear();
				miSentSize = 0;
				mOptionalSendTime.reset();
			}
		}
	}
}

} // namespace engine

#endif // BT_SERVER

