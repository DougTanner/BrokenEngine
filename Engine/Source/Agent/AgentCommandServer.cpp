#include "Pch.h"

#include "Agent/AgentCommandServer.h"

#include "Agent/AgentCommands.h"

namespace engine
{

// Bounds dormant agent-command connection latency to 20 wakes per second; dtor notification does not depend on this cadence.
constexpr std::chrono::milliseconds kListenerRetryInterval = 50ms;
// A response that has been moved out of the shared slot is flushed as one frame during shutdown, but never longer
// than this deadline. The same 50 ms readiness cadence keeps stop/join bounded while a peer is not reading.
constexpr std::chrono::seconds kResponseFlushTimeout = 3s;

AgentCommandServer::AgentCommandServer(int64_t iPort)
{
	// WSAStartup is guaranteed by NetworkManager (enet_initialize), constructed before this.

	// Loopback only — never bind a routable interface.
	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(static_cast<uint16_t>(iPort));

	// SO_REUSEADDR permits rebinding a port in TIME_WAIT after rapid relaunches (Windows default ~120 s).
	// The loopback channel trusts local processes; duplicate listeners can bind the same port and route connections nondeterministically.
	// The harness must quit and wait for the exact PID before relaunching.
	// WSAEADDRINUSE retries are bounded and block only the startup thread, outside the main loop.
	static constexpr int64_t kiMaximumBindAttempts = 10;
	static constexpr std::chrono::milliseconds kBindRetryInterval = 250ms; // ~2.5 s worst case across the attempts
	for (int64_t i = 0; ; ++i)
	{
		muiListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (muiListenSocket == INVALID_SOCKET)
		{
			LOG(kNetwork, kError, "AgentCommandServer socket creation failed: {}", WSAGetLastError());
			throw std::runtime_error("agent socket creation failed");
		}

		BOOL bReuseAddress = TRUE;
		setsockopt(muiListenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&bReuseAddress), sizeof(bReuseAddress));

		if (bind(muiListenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR)
		{
			break;
		}

		int64_t iBindError = WSAGetLastError();
		closesocket(muiListenSocket); // a failed bind leaves the socket unusable; recreate (and re-arm SO_REUSEADDR) next attempt
		muiListenSocket = INVALID_SOCKET;

		// Only WSAEADDRINUSE is retryable; other bind errors fail startup immediately.
		if (iBindError != WSAEADDRINUSE || i + 1 >= kiMaximumBindAttempts)
		{
			LOG(kNetwork, kError, "AgentCommandServer bind to 127.0.0.1:{} failed: {}", iPort, iBindError);
			throw std::runtime_error("agent bind failed");
		}

		if (i == 0)
		{
			LOG(kNetwork, kWarning, "AgentCommandServer bind to 127.0.0.1:{} in use (WSAEADDRINUSE), retrying up to {} attempts", iPort, kiMaximumBindAttempts);
		}
		Sleep(static_cast<DWORD>(kBindRetryInterval.count()));
	}

	if (listen(muiListenSocket, 1) == SOCKET_ERROR) // backlog 1 — a single connection at a time
	{
		LOG(kNetwork, kError, "AgentCommandServer listen on 127.0.0.1:{} failed: {}", iPort, WSAGetLastError());
		closesocket(muiListenSocket);
		muiListenSocket = INVALID_SOCKET;
		throw std::runtime_error("agent listen failed");
	}

	u_long uiNonBlocking = 1;
	if (ioctlsocket(muiListenSocket, FIONBIO, &uiNonBlocking) == SOCKET_ERROR)
	{
		LOG(kNetwork, kError, "AgentCommandServer listener non-blocking configuration failed: {}", WSAGetLastError());
		closesocket(muiListenSocket);
		muiListenSocket = INVALID_SOCKET;
		throw std::runtime_error("agent listener non-blocking configuration failed");
	}

	LOG(kNetwork, kInfo, "AgentCommandServer listening on 127.0.0.1:{}", iPort);

	// Small workbuffer: the JSON parse and socket buffers live on the heap, off the sim path.
	mListenerThread = std::jthread(common::ThreadLocal::Entry([this](std::stop_token stopToken)
	{
		ListenerLoop(std::move(stopToken));
	}, 64 * 1'024));
#if defined(BT_CLIENT) && defined(BT_DEBUG)
	AudioStreamingHarnessRig::Attach(*mpAudioStreamingHarnessRig);
#endif
}

AgentCommandServer::~AgentCommandServer()
{
	ClearDeferredResponse();
#if defined(BT_CLIENT) && defined(BT_DEBUG)
	mpAudioStreamingHarnessRig->Shutdown();
	AudioStreamingHarnessRig::Detach(*mpAudioStreamingHarnessRig);
	mpAudioStreamingHarnessRig.reset();
#endif
	// Teardown only requests stop under the lock and wakes the listener; the listener closes both sockets itself
	// before the jthread joins.
	{
		std::unique_lock lock(mMutex);
		mListenerThread.request_stop();
	}
	mResponseReady.notify_all();
}

void AgentCommandServer::ClearDeferredResponse()
{
	mDeferredPoll = nullptr;
	mDeferredIdentifier = nullptr;
	mbResponseDeferred = false;
	miDeferredGeneration = 0;
	miDeferredDrainCount = 0;
}

void AgentCommandServer::ListenerLoop(std::stop_token stopToken)
{
	// Heap: agent listener thread — socket buffers and JSON parse, off the sim path.
	ScopedSuppressAllocationTracking suppress;

	while (true)
	{
		SOCKET uiClientSocket = INVALID_SOCKET;
		{
			std::unique_lock lock(mMutex);
			if (stopToken.stop_requested())
			{
				break;
			}

			uiClientSocket = accept(muiListenSocket, nullptr, nullptr);
			if (uiClientSocket == INVALID_SOCKET)
			{
				int64_t iAcceptError = WSAGetLastError();
				if (iAcceptError == WSAEWOULDBLOCK)
				{
					mResponseReady.wait_for(lock, kListenerRetryInterval, [&stopToken]()
					{
						return stopToken.stop_requested();
					});
					continue;
				}

				LOG(kNetwork, kError, "AgentCommandServer accept failed: {}", iAcceptError);
				break;
			}
		}

		u_long uiNonBlocking = 1;
		if (ioctlsocket(uiClientSocket, FIONBIO, &uiNonBlocking) == SOCKET_ERROR)
		{
			LOG(kNetwork, kError, "AgentCommandServer accepted socket non-blocking configuration failed: {}", WSAGetLastError());
			closesocket(uiClientSocket);
			continue;
		}

		{
			std::unique_lock lock(mMutex);
			if (stopToken.stop_requested())
			{
				closesocket(uiClientSocket);
				break;
			}
		}

		ServeConnection(uiClientSocket, stopToken);

		// Bump the generation so any response still deferred from this connection is discarded by Drain instead of
		// landing in the next connection's stream (id desync / wedged deferred branch), and drop any response the main
		// thread published after the peer stopped waiting so it can't satisfy the next connection's first request.
		// mDeferredPoll itself is main-thread-only — never bare-written here.
		{
			std::unique_lock lock(mMutex);
			++miConnectionGeneration;
			mPendingResponse.reset();
			closesocket(uiClientSocket);
		}
	}

	closesocket(muiListenSocket);
}

void AgentCommandServer::ServeConnection(SOCKET uiClientSocket, const std::stop_token& rStopToken)
{
	while (!rStopToken.stop_requested())
	{
		// Read the 4-byte little-endian length prefix (x64 host is little-endian — use the bytes directly).
		uint32_t uiLength = 0;
		if (!ReadExact(uiClientSocket, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&uiLength), sizeof(uiLength)), rStopToken))
		{
			return; // peer closed or socket error
		}

		if (uiLength > kiMaximumRequestBytes)
		{
			LOG(kNetwork, kWarning, "AgentCommandServer request frame too large ({} bytes), closing connection", uiLength);
			return;
		}

		std::string payload;
		payload.resize(uiLength);
		if (uiLength > 0 && !ReadExact(uiClientSocket, std::span<uint8_t>(reinterpret_cast<uint8_t*>(payload.data()), uiLength), rStopToken))
		{
			return;
		}

		PendingRequest request;
		try
		{
			request.request = nlohmann::json::parse(payload);
			request.bParsed = true;
		}
		catch (const std::exception&)
		{
			request.bParsed = false;
		}

		{
			std::unique_lock lock(mMutex);
			if (rStopToken.stop_requested())
			{
				return; // stop can race the completed read; do not publish a request after shutdown begins
			}
			request.iGeneration = miConnectionGeneration;
			mPendingRequest = std::move(request);
		}

		// Wait for the main thread's Drain() to publish the serialized response. Between timed waits, peek the socket
		// outside the lock so a peer that closes mid-wait releases the connection instead of parking it until publish.
		// MSG_PEEK leaves any unsolicited bytes queued for the next request read.
		std::string response;
		{
			std::unique_lock lock(mMutex);
			while (!mResponseReady.wait_for(lock, kListenerRetryInterval, [this, &rStopToken]()
			{
				return mPendingResponse.has_value() || rStopToken.stop_requested();
			}))
			{
				lock.unlock();
				char cPeekByte = 0;
				int64_t iPeeked = recv(uiClientSocket, &cPeekByte, 1, MSG_PEEK);
				if (iPeeked == 0 || (iPeeked == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
				{
					return; // peer closed or reset; teardown bumps the generation so its response is dropped
				}
				lock.lock();
			}
			if (!mPendingResponse.has_value())
			{
				return; // stop requested during shutdown
			}
			response = std::move(*mPendingResponse);
			mPendingResponse.reset();
		}

		if (!SendFrame(uiClientSocket, response))
		{
			return;
		}
	}
}

void AgentCommandServer::Drain()
{
	// Heap: JSON request/response construction runs inside the tracked main loop
	ScopedSuppressAllocationTracking suppress;

	// An in-flight deferred response (async screenshot / dump capture) takes precedence: poll it before accepting a
	// new request. While its connection is live, the listener waits on mResponseReady until the poll yields a result,
	// so no new request arrives. If that peer closes, a later connection's request can wait in mPendingRequest; the
	// stale poll is discarded below on the next Drain before that request is taken.
	if (mDeferredPoll)
	{
		// Discard a deferred response whose generation differs from the live connection so it cannot enter the next
		// connection's stream and desynchronize IDs. The listener tears the connection down when its peer closes
		// mid-deferral, which bumps the generation. Shutdown also invalidates the generation, and no peer remains to
		// receive that response.
		bool bStaleConnection = false;
		{
			std::unique_lock lock(mMutex);
			bStaleConnection = miDeferredGeneration != miConnectionGeneration;
		}
		if (bStaleConnection)
		{
			mDeferredPoll = nullptr;
			mbResponseDeferred = false;
			return;
		}

		nlohmann::json response;
		response["id"] = mDeferredIdentifier;

		// Liveness timeout: a capture request lost to device-loss Graphics recreation (mailboxes wiped) never
		// resolves — bound the wait and publish a failure so the channel isn't deadlocked forever.
		if (++miDeferredDrainCount > kiDeferredTimeoutDrains)
		{
			mDeferredPoll = nullptr;
			response["ok"] = false;
			response["error"] = "capture timed out";
			PublishResponse(std::move(response));
			return;
		}

		try
		{
			common::ScopedExpectedThrows scopedExpectedThrows; // validation throws here are a designed error path — keep them off the VEH crash-diagnostic walk
			std::optional<nlohmann::json> result = mDeferredPoll();
			if (!result.has_value())
			{
				return; // still pending — do not accept a new request
			}
			response["ok"] = true;
			response["result"] = std::move(*result);
		}
		catch (const std::exception& rException)
		{
			response["ok"] = false;
			response["error"] = rException.what();
		}
		mDeferredPoll = nullptr;
		PublishResponse(std::move(response));
		return;
	}

	PendingRequest request;
	{
		std::unique_lock lock(mMutex);
		if (!mPendingRequest.has_value())
		{
			return;
		}
		request = std::move(*mPendingRequest);
		mPendingRequest.reset();
	}
	// Every response to this request, synchronous or deferred, publishes only while its handoff generation is live.
	miDeferredGeneration = request.iGeneration;

	// Build the response envelope. Malformed JSON or a missing/invalid cmd answers with id:null; a handler
	// exception echoes the request id. The lock is never held across game-state work.
	nlohmann::json response;
	if (!request.bParsed)
	{
		response["id"] = nullptr;
		response["ok"] = false;
		response["error"] = "malformed JSON";
	}
	else if (!request.request.contains("cmd") || !request.request["cmd"].is_string())
	{
		response["id"] = nullptr;
		response["ok"] = false;
		response["error"] = "missing cmd";
	}
	else
	{
		response["id"] = request.request.contains("id") ? request.request["id"] : nlohmann::json(nullptr);

		// Reject an unknown top-level envelope key before dispatch (before mDeferredIdentifier is armed) so a typo'd field
		// can never be silently ignored. Only id/cmd/params are legitimate; the first offending key is reported.
		bool bUnknownKey = false;
		for (const auto& [rKey, rValue] : request.request.items())
		{
			if (rKey != "cmd" && rKey != "params" && rKey != "id")
			{
				response["ok"] = false;
				response["error"] = "unknown envelope key '" + rKey + "'";
				bUnknownKey = true;
				break;
			}
		}
		if (!bUnknownKey)
		{
			std::string command = request.request["cmd"].get<std::string>();
			const nlohmann::json& rParameters = request.request.contains("params") ? request.request["params"] : nlohmann::json::object();

			// A handler may call DeferResponse() to complete asynchronously; record the id it must echo first.
			mDeferredIdentifier = response["id"];
			mbResponseDeferred = false;
			try
			{
				common::ScopedExpectedThrows scopedExpectedThrows; // validation throws here are a designed error path — keep them off the VEH crash-diagnostic walk
				nlohmann::json result;
				game::ExecuteAgentCommand(command, rParameters, result);
				if (mbResponseDeferred)
				{
					return; // response published later by the deferred-poll path above
				}
				response["ok"] = true;
				response["result"] = std::move(result);
			}
			catch (const std::exception& rException)
			{
				mDeferredPoll = nullptr; // a handler that deferred then threw does not leave a stale poll
				mbResponseDeferred = false;
				response["ok"] = false;
				response["error"] = rException.what();
			}
		}
	}

	PublishResponse(std::move(response));
}

void AgentCommandServer::DeferResponse(std::move_only_function<std::optional<nlohmann::json>()> Poll)
{
	mDeferredPoll = std::move(Poll);
	mbResponseDeferred = true;
	miDeferredDrainCount = 0;
}

void AgentCommandServer::PublishResponse(nlohmann::json response)
{
	std::string responseString = response.dump();
	if (static_cast<int64_t>(responseString.size()) > kiMaximumResponseBytes)
	{
		LOG(kNetwork, kError, "AgentCommandServer response exceeds cap ({} bytes), replacing with error", responseString.size());
		nlohmann::json capped;
		capped["id"] = response["id"];
		capped["ok"] = false;
		capped["error"] = "response too large";
		responseString = capped.dump();
	}

	{
		std::unique_lock lock(mMutex);
		if (miDeferredGeneration != miConnectionGeneration)
		{
			return; // the request's connection was torn down; a later connection must never receive this response
		}
		mPendingResponse = std::move(responseString);
	}
	mResponseReady.notify_one();
}

bool AgentCommandServer::ReadExact(SOCKET uiClientSocket, std::span<uint8_t> buffer, const std::stop_token& rStopToken)
{
	int64_t iTotal = 0;
	while (iTotal < std::ssize(buffer))
	{
		if (rStopToken.stop_requested())
		{
			return false;
		}

		fd_set readSet {};
		FD_ZERO(&readSet);
		FD_SET(uiClientSocket, &readSet);
		timeval timeout {};
		timeout.tv_sec = static_cast<long>(kListenerRetryInterval.count() / 1'000);
		timeout.tv_usec = static_cast<long>((kListenerRetryInterval.count() % 1'000) * 1'000);
		int64_t iReady = select(0, &readSet, nullptr, nullptr, &timeout);
		if (iReady == SOCKET_ERROR)
		{
			return false;
		}
		if (iReady == 0)
		{
			continue;
		}
		if (rStopToken.stop_requested())
		{
			return false;
		}

		int64_t iReceived = recv(uiClientSocket, reinterpret_cast<char*>(buffer.data() + iTotal), static_cast<int>(std::ssize(buffer) - iTotal), 0);
		if (iReceived > 0)
		{
			iTotal += iReceived;
			continue;
		}
		if (iReceived == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			continue;
		}
		return false; // 0 = peer closed; other errors end the connection
	}
	return true;
}

// ServeConnection moved the response out of the shared slot before calling SendFrame, so this frame must flush
// despite stop. rDeadline bounds the detached flush.
bool AgentCommandServer::SendExact(SOCKET uiClientSocket, std::span<const uint8_t> buffer, const std::chrono::steady_clock::time_point& rDeadline)
{
	int64_t iTotal = 0;
	while (iTotal < std::ssize(buffer))
	{
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		if (now >= rDeadline)
		{
			return false;
		}

		std::chrono::microseconds waitDuration = std::chrono::duration_cast<std::chrono::microseconds>(rDeadline - now);
		std::chrono::microseconds wakeDuration = std::chrono::duration_cast<std::chrono::microseconds>(kListenerRetryInterval);
		if (waitDuration > wakeDuration)
		{
			waitDuration = wakeDuration;
		}
		waitDuration = (std::max)(waitDuration, 1us);

		fd_set writeSet {};
		FD_ZERO(&writeSet);
		FD_SET(uiClientSocket, &writeSet);
		timeval timeout {};
		timeout.tv_sec = static_cast<long>(waitDuration.count() / 1'000'000);
		timeout.tv_usec = static_cast<long>(waitDuration.count() % 1'000'000);
		int64_t iReady = select(0, nullptr, &writeSet, nullptr, &timeout);
		if (iReady == SOCKET_ERROR)
		{
			return false;
		}
		if (iReady == 0)
		{
			continue;
		}

		int64_t iSent = send(uiClientSocket, reinterpret_cast<const char*>(buffer.data() + iTotal), static_cast<int>(std::ssize(buffer) - iTotal), 0);
		if (iSent > 0)
		{
			iTotal += iSent;
			continue;
		}
		if (iSent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			continue;
		}
		return false;
	}
	return true;
}

bool AgentCommandServer::SendFrame(SOCKET uiClientSocket, std::string_view payload)
{
	uint32_t uiLength = static_cast<uint32_t>(payload.size());
	std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + kResponseFlushTimeout;
	if (!SendExact(uiClientSocket, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&uiLength), sizeof(uiLength)), deadline))
	{
		return false;
	}
	return SendExact(uiClientSocket, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()), deadline);
}

} // namespace engine
