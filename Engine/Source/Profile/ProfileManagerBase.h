#pragma once

// This header is Engine.h's first include (Engine.h:4) and must stay engine-include-free — its
// upstream closure is Common + ExternalHeaders only. Any engine include added here becomes an
// aggregation-order landmine.

namespace engine
{

enum class ProfileScreen : uint8_t
{
	kOff,
	kCpu,
	kGpu,
	kNetwork,
	kCount,
};

enum class ProfileRowFlags : uint8_t
{
	kVisible      = 1 << 0,
	kSmoothAtStop = 1 << 1,
};

struct CpuCounter
{
	int64_t iCount = 0;
	common::Flags<ProfileRowFlags> flags {};
};

struct CpuTimerThreadState
{
	std::chrono::steady_clock::time_point startTimePoint;
	int64_t iStartAllocations = 0;
};

struct CpuTimer
{
	int64_t iThreads = 0;
	int64_t iTotalFrameTimeNanoseconds = 0;

	int64_t iAllocationsThisFrame = 0;

	common::Smoothed<int64_t> smoothedMicroseconds {};
	common::Smoothed<int64_t> smoothedAllocations {};

	common::Flags<ProfileRowFlags> flags {};
};

#if defined(BT_SERVER)
struct RawCpuTimerRecord
{
	int64_t iSampleSequence = 0;
	int64_t iSampleMicroseconds = 0;
	int64_t iInvocationCount = 0;
	int64_t iAuxiliaryCount = 0;
};

enum class RawCpuTimerEventFlags : uint8_t
{
	kAvailable = 1 << 0,
	kOverrun = 1 << 1,
};

enum class RawCpuTimerStateFlags : uint8_t
{
	kRegistered = 1 << 0,
	kEventRegistered = 1 << 1,
	kEventArmed = 1 << 2,
};

struct RawCpuTimerEventRecord
{
	int64_t iEventSequence = 0;
	int64_t iSampleSequence = 0;
	int64_t iSampleTick = 0;
	int64_t iSampleMicroseconds = 0;
	int64_t iInvocationCount = 0;
	int64_t iAuxiliaryCount = 0;
	common::Flags<RawCpuTimerEventFlags> flags {};
};
#endif // BT_SERVER

enum EngineCpuCounters : int64_t
{
	kCpuCounterBillboards,
		kCpuCounterBillboardsRendered,
	kCpuCounterHexShields,
		kCpuCounterHexShieldsRendered,
	kCpuCounterAreaLights,
		kCpuCounterAreaLightsRendered,
	kCpuCounterPointLights,
		kCpuCounterPointLightsRendered,
	kCpuCounterPuffs,
		kCpuCounterPuffsRendered,
	kCpuCounterSmokeTrails,
		kCpuCounterSmokeTrailsRendered,
	kCpuCounterExplosions,
	kCpuCounterPushers,
	kCpuCounterSounds,
	kCpuCounterStreams,

	kEngineCpuCounterCount
};

// One entry per EngineCpuCounters enumerator, in order. The static_assert catches a dropped/extra name — a missing
// initializer would otherwise silently misalign every later overlay row (member arrays can't deduce extent, so the
// names live here as a deduced-extent table).
inline constexpr std::string_view kEngineCpuCounterNames[]
{
	"Billboards",
	"    Rendered",
	"HexShields",
	"    Rendered",
	"AreaLights",
	"    Rendered",
	"PointLights",
	"    Rendered",
	"Puffs",
	"    Rendered",
	"SmokeTrails",
	"    Rendered",
	"Explosions",
	"Pushers",
	"Sounds",
	"Streams",
};
static_assert(std::size(kEngineCpuCounterNames) == static_cast<size_t>(kEngineCpuCounterCount));

enum EngineCpuTimers : int64_t
{
	kCpuTimerAcquireToGlobal,
	kCpuTimerRenderGlobal,
	kCpuTimerReduceInputLagFence,
	kCpuTimerAudio,
	kCpuTimerMessagesAndInput,
	kCpuTimerWaitFence,
	kCpuTimerRenderMain,
	kCpuTimerUpdateProfileText,
	kCpuTimerWaitPresentFuture,
	kCpuTimerSubmitGlobal,
	kCpuTimerSubmitImage,
	kCpuTimerPresent,
	kCpuTimerAcquireImage,
	kCpuTimerAcquireImageFence,
	kCpuTimerNetworkPollReconcile,
	kCpuTimerNetworkSend,

	kEngineCpuTimerCount
};

// One entry per EngineCpuTimers enumerator, in order; static_assert guards against drift (see kEngineCpuCounterNames).
inline constexpr std::string_view kEngineCpuTimerNames[]
{
	"Acquire to global",
	"Render global",
	"Reduce input lag fence",
	"Audio",
	"Messages and input",
	"Wait fence",
	"Render main",
	"Profile text",
	"Wait present future",
	"    Submit global",
	"    Submit image",
	"    Present",
	"Acquire image",
	"    Fence",
	"Network poll+reconcile",
	"Network send",
};
static_assert(std::size(kEngineCpuTimerNames) == static_cast<size_t>(kEngineCpuTimerCount));

#if defined(BT_CLIENT)
enum GpuTimers : int64_t
{
	kGpuTimerGlobal,
		kGpuTimerGlobalUniformCopy,
		kGpuTimerShadow,
		kGpuTimerTerrainElevation,
		kGpuTimerSpread,
			kGpuTimerWindSpread,
			kGpuTimerSmokeSpread,
		kGpuTimerParticles,
			kGpuTimerParticlesSpawn,
			kGpuTimerLongParticlesUpdate,
			kGpuTimerSquareParticlesUpdate,
	kGpuTimerMain,
		kGpuTimerMainUniformCopy,
		kGpuTimerSmokeEmit,
		kGpuTimerWindDeposit,
		kGpuTimerLightingDeposit,
		kGpuTimerLightingSpread,
		kGpuTimerLightingCombine,
		kGpuTimerLightingTemporal,
		kGpuTimerObjectShadows,
		kGpuTimerObjectShadowsBlur,
		kGpuTimerWaterDisplacement,
	kGpuTimerImage,
		kGpuTimerUiDepth,
		kGpuTimerObjects,
		kGpuTimerTransparentObjects,
		kGpuTimerTerrain,
		kGpuTimerWater,
		kGpuTimerHexShields,
		kGpuTimerLongParticlesRender,
		kGpuTimerSquareParticlesRender,
		kGpuTimerVisibleLights,
		kGpuTimerBillboards,
		kGpuTimerHdrResolve,
	kGpuTimerUiRender,

	kGpuTimerCount
};

// One entry per GpuTimers enumerator, in order; static_assert guards against drift (see kEngineCpuCounterNames).
inline constexpr std::string_view kGpuTimerNames[]
{
	"Global render",
	"    Uniform Copy",
	"    Shadow",
	"    Terrain Elevation",
	"    Spread",
	"        Wind Spread",
	"        Smoke Spread",
	"    Particles",
	"        Particles Spawn",
	"        Long Particles Update",
	"        Square Particles Update",
	"Main render",
	"    Uniform Copy",
	"    Smoke Emit",
	"    Wind Deposit",
	"    Lighting Deposit",
	"    Lighting Spread",
	"    Lighting Combine",
	"    Lighting Temporal",
	"    Object Shadows",
	"    Object Shadows blur",
	"    Water Displacement",
	"Image render",
	"    Ui Depth",
	"    Objects",
	"    Transparent Objects",
	"    Terrain",
	"    Water",
	"    Hex Shields",
	"    Long Particles Render",
	"    Square Particles Render",
	"    VisibleLights",
	"    Billboards",
	"    HDR Resolve",
	"Ui Render",
};
static_assert(std::size(kGpuTimerNames) == static_cast<size_t>(kGpuTimerCount));

struct GpuTimer
{
	common::Smoothed<int64_t> smoothedMicroseconds {};
	common::Flags<ProfileRowFlags> flags {};
};

struct GpuShadowSample
{
	int64_t iSequence = 0;
	int64_t iCurrentMicroseconds = 0;
};
#endif // BT_CLIENT

enum BootTimers : int64_t
{
	kBootTimerTotal,
		kBootTimerWaitForDataFile,
		kBootTimerWaitForTexturesFile,
		kBootTimerWaitForPriorityTextures,
		kBootTimerWaitForIslands,
		kBootTimerVulkan,
			kBootTimerInstanceManager,
			kBootTimerDeviceManager,
			kBootTimerSwapchainManager,
			kBootTimerParticleManager,
			kBootTimerPipelineManager,
			kBootTimerCommandBufferManager,
			kBootTimerBufferManager,
			kBootTimerIslands,
			kBootTimerTextureManager,
				kBootTimerTextureUpload,
				kModelTexturesGeneration,
			kBootTimerRecordCommandBuffers,
			kBootTimerRenderPresent,

	kBootTimerCount
};

// One entry per BootTimers enumerator, in order; static_assert guards against drift (see kEngineCpuCounterNames).
inline constexpr std::string_view kBootTimerNames[]
{
	"Total",
	"    Wait for data file",
	"    Wait for textures file",
	"    Wait for priority textures",
	"    Wait for priority islands",
	"    Vulkan",
	"      InstanceManager",
	"      DeviceManager",
	"      SwapchainManager",
	"      ParticleManager",
	"      PipelineManager",
	"      CommandBufferManager",
	"      BufferManager",
	"      Islands",
	"      TextureManager",
	"          TextureUpload",
	"          Pbr textures",
	"      Record command buffers",
	"      Render present",
};
static_assert(std::size(kBootTimerNames) == static_cast<size_t>(kBootTimerCount));

struct BootTimer
{
	std::chrono::steady_clock::time_point startTimePoint = std::chrono::steady_clock::time_point();
	std::chrono::nanoseconds timeNanoseconds = 0ns;
};

enum class CpuStopFlags : uint32_t
{
	kSmoothNow   = 0x01,
	kCrossThread = 0x02,
};
using CpuStopFlags_t = common::Flags<CpuStopFlags>;

class ProfileManagerBase
{
public:

	// The derived arrays are not constructed until after this base constructor returns. Store their addresses only;
	// no base-constructor path may dereference them.
	ProfileManagerBase(std::span<CpuCounter> gameCpuCounters, std::span<CpuTimer> gameCpuTimers, std::span<const std::string_view> gameCpuCounterNames, std::span<const std::string_view> gameCpuTimerNames);
	virtual ~ProfileManagerBase() = default;

	void Create();
	void Destroy();

	void ToggleProfileText();

	void CpuStart(int64_t iCpuTimer, int64_t iThreads = 1);
	void CpuStop(int64_t iCpuTimer, CpuStopFlags_t flags = {});

#if defined(BT_SERVER)
	void RegisterRawCpuTimer(int64_t iCpuTimer);
	void RegisterRawCpuTimerEvent(int64_t iCpuTimer);
	void AddRawCpuTimerAuxiliaryCount(int64_t iCpuTimer, int64_t iCount);
	void LatchRawCpuTimer(int64_t iCpuTimer, bool bAccept);
	void LatchRawCpuTimers(bool bAccept, int64_t iSampleTick);
	bool ArmRawCpuTimerEvent(int64_t iCpuTimer, int64_t iMinimumSampleTick);
	// The caller must hold mCpuTimerMutex.
	bool ArmRawCpuTimerEventLocked(int64_t iCpuTimer, int64_t iMinimumSampleTick);
	// The caller must hold mCpuTimerMutex. Publication is performed by the derived latch hook.
	bool PublishRawCpuTimerEvent(int64_t iCpuTimer, int64_t iSampleTick);
	// The caller must hold mCpuTimerMutex.
	bool AcknowledgeRawCpuTimerEvent(int64_t iCpuTimer, int64_t iEventSequence);
#endif // BT_SERVER

#if defined(BT_CLIENT)
	void ResetQueryPools(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eStart, GpuTimers eEnd);

	void GpuStart(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eGpuTimer);
	void GpuStop(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eGpuTimer);
	void GpuRead(int64_t iCommandBuffer, GpuTimers eStart, GpuTimers eEnd, bool bLatchShadowSample);

	void SetClockCorrection(int64_t iOffset, int64_t iTargetBehind, int64_t iError);

	// Called when rendering resumes after skipped frames, so the first rendered frame reports only its own samples.
	void DiscardSkippedFrameSamples();
#endif // BT_CLIENT

	void BootStart(BootTimers eBootTimer);
	void BootStop(BootTimers eBootTimer);
	void BootLog();

	void SmoothCpuTimers();
	void LogTimers();
	void UpdateProfileText();

	// Advances the shared profile-text visibility clock; returns true on a re-evaluation boundary (>= kProfileVisibilityInterval since last).
	bool TickVisibilityCadence();

	CpuCounter& GetCpuCounter(int64_t iIndex);
	CpuTimer& GetCpuTimer(int64_t iIndex);
	std::string_view GetCpuCounterName(int64_t iIndex);
	std::string_view GetCpuTimerName(int64_t iIndex);

#if defined(BT_CLIENT)
	GpuShadowSample mGpuShadowSample {};
#endif // BT_CLIENT
	// Guards CPU-timer arrays and per-thread states shared by dispatch, submit, and network threads. Timer scopes must remain coarse to avoid distorting measurements. FormatCpuTimersText and LogTimers hold this mutex while reading.
	std::mutex mCpuTimerMutex;

	common::InTheLastSecond mFullUpdatesInTheLastSecond;
	common::InTheLastSecond mInterpolateUpdatesInTheLastSecond;

	ProfileScreen meProfileScreen = kbShowProfileTextByDefault ? ProfileScreen::kCpu : ProfileScreen::kOff;

#if defined(BT_SERVER) && !defined(ENABLE_CRT_DEBUG_HEAP)
	int64_t miMimallocCommittedMebibytes = 0;
	int64_t miMimallocPeakCommittedMebibytes = 0;
	int64_t miMimallocHeapUsedMebibytes = 0;
	int64_t miMimallocPeakHeapUsedMebibytes = 0;
#endif

protected:

#if defined(BT_CLIENT)
	void DumpTimers();
#endif // BT_CLIENT

	// Names live in the deduced-extent kXxx*Names tables above (static_assert-guarded); these arrays carry only the
	// per-row runtime state and are indexed by the same enums.
	CpuCounter mEngineCpuCounters[kEngineCpuCounterCount];

	CpuTimer mEngineCpuTimers[kEngineCpuTimerCount];

	CpuCounter* mpGameCpuCounters = nullptr;
	CpuTimer* mpGameCpuTimers = nullptr;
	const std::string_view* mpGameCpuCounterNames = nullptr;
	const std::string_view* mpGameCpuTimerNames = nullptr;
public:
	int64_t miCpuCounterCount = 0;

	int64_t miCpuTimerCount = 0;
protected:

#if defined(BT_CLIENT)
public:
	GpuTimer mGpuTimers[kGpuTimerCount];
protected:
#endif // BT_CLIENT

	BootTimer mBootTimers[kBootTimerCount];

	std::chrono::steady_clock::time_point mLastVisibilityEvaluationTime = std::chrono::steady_clock::time_point();

	std::unordered_map<std::thread::id, std::vector<CpuTimerThreadState>> mPerThreadTimerStates;

public:
	common::Smoothed<int64_t> mSmoothedAllocations;
protected:

#if defined(BT_SERVER)
	// Called while mCpuTimerMutex is held, after accepted raw records have been copied.
	virtual void OnRawCpuTimersLatched(int64_t)
	{
	}

	struct RawCpuTimerState
	{
		int64_t iTotalTimeNanoseconds = 0;
		int64_t iInvocationCount = 0;
		std::atomic<int64_t> iAuxiliaryCount = 0;
		RawCpuTimerRecord record {};
		common::Flags<RawCpuTimerStateFlags> flags {};
		RawCpuTimerEventRecord eventRecord {};
		int64_t iMinimumSampleTick = 0;
	};

public:
	// The caller must hold mCpuTimerMutex and gate reads with kbProfiling; storage is absent when profiling is disabled.
	std::unique_ptr<RawCpuTimerState[]> mpRawCpuTimers;
protected:
#endif // BT_SERVER

#if defined(BT_CLIENT)
	VkQueryPool mVkQueryPool = VK_NULL_HANDLE;

	std::unique_ptr<common::DiagnosticLog> mpDumpLog;
	std::chrono::steady_clock::time_point mDumpStartTime = std::chrono::steady_clock::time_point();
	std::chrono::steady_clock::time_point mLastDumpTime = std::chrono::steady_clock::time_point();
#endif // BT_CLIENT

#if defined(BT_CLIENT)
public:
	common::Smoothed<int64_t> mSmoothedRoundTripTime;
	common::Smoothed<int64_t> mSmoothedJitter;
	common::Smoothed<int64_t> mSmoothedClockOffset;

	common::Smoothed<int64_t> mSmoothedClockTarget;
	common::Smoothed<int64_t> mSmoothedClockError;
	common::Smoothed<int64_t> mSmoothedRollback;
	common::Smoothed<int64_t> mSmoothedBuffer;
	common::Smoothed<int64_t> mSmoothedReceived;

	common::InTheLastSecond mCrcValidatedTicksPerSecond;

	common::InTheLastSecond mAssumedTicksPerSecond;
	common::InTheLastSecond mCrcFastPathEventsPerSecond;

	common::InTheLastSecond mStatusChangeReplayTicksPerSecond;
	common::InTheLastSecond mKnockOnReplayTicksPerSecond;
#endif // BT_CLIENT
};

class [[nodiscard]] ScopedBootTimer
{
public:

	ScopedBootTimer(BootTimers eBootTimer);
	~ScopedBootTimer();
	ScopedBootTimer(const ScopedBootTimer&) = delete;
	ScopedBootTimer& operator=(const ScopedBootTimer&) = delete;

private:

	BootTimers meBootTimer = kBootTimerTotal;
};

class [[nodiscard]] ScopedCpuProfile
{
public:

	ScopedCpuProfile(int64_t iCpuTimer, int64_t iThreads = 1);
	~ScopedCpuProfile();
	ScopedCpuProfile(const ScopedCpuProfile&) = delete;
	ScopedCpuProfile& operator=(const ScopedCpuProfile&) = delete;

private:

	int64_t miCpuTimer = 0;
};

void FormatCpuTimersText(common::Workbuffer& rWorkbuffer, bool bReevaluate);
void FormatCpuCountersText(common::Workbuffer& rWorkbuffer, bool bReevaluate);

#if defined(BT_CLIENT)
void FormatFramesPerSecondHeader(common::Workbuffer& rWorkbuffer, std::chrono::microseconds elapsedCpuTime);
void FormatCpuScreen(common::Workbuffer& rWorkbuffer, bool bReevaluate);
void FormatGpuScreen(common::Workbuffer& rWorkbuffer, bool bReevaluate);
void FormatNetworkScreen(common::Workbuffer& rWorkbuffer);
void RenderImPlotGraphs();
#endif

} // namespace engine
