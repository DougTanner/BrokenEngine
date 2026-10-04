#pragma once

#if defined(BT_CLIENT) && defined(BT_DEBUG)
#include "Agent/Commands/AudioStreamingFixture.h"
#endif

namespace engine
{

// Loopback-only (127.0.0.1) TCP JSON command channel embedded in both executables. Transport only — knows no
// dispatch semantics; the listener thread reads length-framed request frames, parses them, and hands each to the main
// thread via Drain(), which dispatches to game::ExecuteAgentCommand. Dormant unless --agent-port is passed.
//
// Framing (both directions): 4-byte little-endian uint32 payload length, then UTF-8 JSON.
// Envelope: request {"id"?, "cmd", "params"?} -> success {"id","ok":true,"result"} / failure {"id","ok":false,"error"}.
// An unknown top-level request key (anything but id/cmd/params) is rejected before dispatch as a failure envelope.
class AgentCommandServer
{
public:

	// Startup socket failures log the WSA error and throw std::runtime_error; Main fails startup.
	// SO_REUSEADDR permits rapid relaunches; address-in-use retries block the startup thread for at most ~2.5 s.
	// Duplicate listeners can share the port, so the harness must quit and wait for the exact PID before relaunching.
	explicit AgentCommandServer(int64_t iPort);
	~AgentCommandServer();

	AgentCommandServer(const AgentCommandServer&) = delete;
	AgentCommandServer& operator=(const AgentCommandServer&) = delete;

	// Main-thread, once per frame: if a request is pending, execute it (unlocked) and publish the response.
	void Drain();

	// Called from a command handler during Drain() to complete asynchronously: Drain() withholds this request's
	// response and instead polls `Poll` at the top of every later Drain(); the first non-empty result is published
	// as {"id",ok:true,"result"} echoing the deferred request's id, and a poll exception as the failure envelope.
	// Only one request is ever in flight, so there is a single deferred slot. Main-thread only.
	void DeferResponse(std::function<std::optional<nlohmann::json>()> Poll);

	// Deferred-poll liveness bound (~30 s at 60 fps): a capture lost to a device-loss Graphics recreation (mailboxes
	// wiped) never resolves, so cap the wait and publish a failure rather than deadlock the channel forever.
	static constexpr int64_t kiDeferredTimeoutDrains = 1'800;

	// Drops a deferred command while its captured runtime dependencies are still alive.
	void ClearDeferredResponse();

#if defined(BT_CLIENT) && defined(BT_DEBUG)
	std::unique_ptr<AudioStreamingFixture> mpAudioStreamingFixture = std::make_unique<AudioStreamingFixture>();
#endif

private:

	// A request parsed off the listener thread and handed to the main thread, stamped with the connection generation
	// it arrived on. bParsed == false means the frame was not valid JSON (Drain answers with an id:null error).
	struct PendingRequest
	{
		nlohmann::json request;
		uint64_t uiGeneration = 0;
		bool bParsed = false;
	};

	void ListenerLoop(std::stop_token stopToken);
	void ServeConnection(SOCKET uiClientSocket, const std::stop_token& rStopToken);

	// Serialize, cap, and hand a response envelope to the listener thread (stores mPendingResponse + notifies). A
	// response whose request generation is no longer the live connection's is dropped without storing or notifying.
	void PublishResponse(nlohmann::json response);

	static bool ReadExact(SOCKET uiClientSocket, std::span<uint8_t> buffer, const std::stop_token& rStopToken);
	static bool SendExact(SOCKET uiClientSocket, std::span<const uint8_t> buffer, const std::chrono::steady_clock::time_point& rDeadline);
	static bool SendFrame(SOCKET uiClientSocket, std::string_view payload);

	static constexpr uint32_t kuiMaximumRequestBytes = 1ui32 * 1'024ui32 * 1'024ui32; // 1 MiB — larger request frames are rejected
	static constexpr int64_t kiMaximumResponseBytes = 16i64 * 1'024i64 * 1'024i64; // 16 MiB response cap

	SOCKET muiListenSocket = INVALID_SOCKET;
	SOCKET muiActiveSocket = INVALID_SOCKET; // current connection; final close is owned by ListenerLoop after I/O exits

	std::mutex mMutex;
	std::condition_variable mResponseReady;
	std::optional<PendingRequest> mPendingRequest; // listener -> main
	std::optional<std::string> mPendingResponse; // main -> listener
	uint64_t muiConnectionGeneration = 0; // mMutex-guarded; bumped on each connection teardown

	// Deferred-response state — touched only on the main thread (Drain and the handlers it calls), so no lock.
	std::function<std::optional<nlohmann::json>()> mDeferredPoll; // set = a response is deferred, polled each Drain
	nlohmann::json mDeferredIdentifier; // id echoed when the deferred poll completes
	bool mbResponseDeferred = false; // set by DeferResponse within the current Drain dispatch
	uint64_t muiDeferredGeneration = 0; // handoff generation of the request Drain last took; its response publishes only while it is live
	int64_t miDeferredDrainCount = 0; // Drains elapsed since the deferral (liveness timeout)

	std::jthread mListenerThread; // last member: its body reads every other member via `this`
};

inline AgentCommandServer* gpAgentCommandServer = nullptr;

inline bool gbQuit = false;

} // namespace engine
