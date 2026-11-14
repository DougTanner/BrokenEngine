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

enum class RawCpuTimerStateFlags : uint8_t
{
	kRegistered = 1 << 0,
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
inline constexpr std::string_view kpEngineCpuCounterNames[]
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
static_assert(std::size(kpEngineCpuCounterNames) == static_cast<size_t>(kEngineCpuCounterCount));

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

// One entry per EngineCpuTimers enumerator, in order; static_assert guards against drift (see kpEngineCpuCounterNames).
inline constexpr std::string_view kpEngineCpuTimerNames[]
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
static_assert(std::size(kpEngineCpuTimerNames) == static_cast<size_t>(kEngineCpuTimerCount));

#if defined(BT_CLIENT)
struct FrameTickPhaseTimings
{
	enum Phase : size_t
	{
		kCellSetup,
		kInterpolateCopyUpdate,
		kPostRenderCopyUpdate,
		kCollisionAreaDamage,
		kTransferDestroySpawn,
		kSharedCrc,
		kElevationGridBuild,
		kRenderPlacementCacheBuild,
		kCount,
	};

	std::array<int64_t, kCount> nanoseconds {};
};

struct ReconciliationDiagnostics
{
	int64_t iActiveCoords = 0;
	int64_t iExecutedCoordTicks = 0;
	int64_t iRecalculatedCoordTicks = 0;
	int64_t iTickExecutionNanoseconds = 0;
	int64_t iDispatcherNanoseconds = 0;
	FrameTickPhaseTimings phaseTimings;
	int64_t iStatusChangeReplayTicks = 0;
	int64_t iKnockOnReplayTicks = 0;
	bool bDesync = false;
};

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
		kGpuTimerWaterFoam,
			kGpuTimerWaterFoamCompute,
			kGpuTimerWaterFoamHistory,
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
		kGpuTimerSceneResolve,
	kGpuTimerUiRender,
		kGpuTimerHdrResolve,

	kGpuTimerCount
};

// One entry per GpuTimers enumerator, in order; static_assert guards against drift (see kpEngineCpuCounterNames).
inline constexpr std::string_view kpGpuTimerNames[]
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
	"    Water Foam",
	"        Water Foam Compute",
	"        Water Foam History",
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
	"    Scene Resolve",
	"Ui Render",
	"    HDR Resolve",
};
static_assert(std::size(kpGpuTimerNames) == static_cast<size_t>(kGpuTimerCount));

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

// One entry per BootTimers enumerator, in order; static_assert guards against drift (see kpEngineCpuCounterNames).
inline constexpr std::string_view kpBootTimerNames[]
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
static_assert(std::size(kpBootTimerNames) == static_cast<size_t>(kBootTimerCount));

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

#if defined(BT_CLIENT)
struct AcquisitionDiagnostics;
#endif // BT_CLIENT

class ProfileManager : public common::Singleton<ProfileManager>
{
public:

	ProfileManager();

	void Create();
	void Destroy();

	void ToggleProfileText();

	void CpuStart(int64_t iCpuTimer, int64_t iThreads = 1);
	void CpuStop(int64_t iCpuTimer, CpuStopFlags_t flags = {});

#if defined(BT_SERVER)
	void RegisterRawCpuTimer(int64_t iCpuTimer);
	void AddRawCpuTimerAuxiliaryCount(int64_t iCpuTimer, int64_t iCount);
	void LatchRawCpuTimers(bool bAccept);
#endif // BT_SERVER

#if defined(BT_CLIENT)
	void ResetQueryPools(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eStart, GpuTimers eEnd);

	void GpuStart(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eGpuTimer);
	void GpuStop(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eGpuTimer);
	void GpuSubmitted(int64_t iCommandBuffer, bool bWorld, bool bAcquireBarriers);
	void GpuRead(int64_t iCommandBuffer);
	void GpuFenceObserved(int64_t iFramebuffer);
	void GpuImageWait(std::chrono::steady_clock::time_point waitStart, std::chrono::steady_clock::time_point waitEnd);
	void LogGraphicsStall(const char* pcOperation, const AcquisitionDiagnostics* pAcquisitionDiagnostics, std::chrono::steady_clock::time_point stallStart);

	void SetClockCorrection(int64_t iOffset, int64_t iTargetBehind, int64_t iError);

	// Called when rendering resumes after skipped frames, so the first rendered frame reports only its own samples.
	void DiscardSkippedFrameSamples();

	void RecordRenderedFrameInterval(std::chrono::nanoseconds interval);
	void SetDiagnosticFocus(bool bHasFocus);
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
	ReconciliationDiagnostics mReconciliationDiagnostics {};
	common::Smoothed<double> mNormalReconciliationTickNanoseconds;
	GpuShadowSample mGpuShadowSample {};
	std::chrono::nanoseconds mExpectedRenderedFrameInterval = 0ns;
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

	// Names live in the deduced-extent kXxx*Names tables (static_assert-guarded); these arrays carry only the per-row
	// runtime state for engine and game indices together. They are sized at construction because this header cannot
	// name the game counts.
	std::unique_ptr<CpuCounter[]> mpCpuCounters;

	std::unique_ptr<CpuTimer[]> mpCpuTimers;
public:
	int64_t miCpuCounterCount = 0;

	int64_t miCpuTimerCount = 0;
protected:

#if defined(BT_CLIENT)
public:
	GpuTimer mpGpuTimers[kGpuTimerCount];
protected:
#endif // BT_CLIENT

	BootTimer mpBootTimers[kBootTimerCount];

	std::chrono::steady_clock::time_point mLastVisibilityEvaluationTime = std::chrono::steady_clock::time_point();

	std::unordered_map<std::thread::id, std::vector<CpuTimerThreadState>> mPerThreadTimerStates;

public:
	common::Smoothed<int64_t> mSmoothedAllocations;
protected:

#if defined(BT_SERVER)
	struct RawCpuTimerState
	{
		int64_t iTotalTimeNanoseconds = 0;
		int64_t iInvocationCount = 0;
		std::atomic<int64_t> iAuxiliaryCount = 0;
		RawCpuTimerRecord record {};
		common::Flags<RawCpuTimerStateFlags> flags {};
	};

public:
	// The caller must hold mCpuTimerMutex and gate reads with kbProfiling; storage is absent when profiling is disabled.
	std::unique_ptr<RawCpuTimerState[]> mpRawCpuTimers;
protected:
#endif // BT_SERVER

#if defined(BT_CLIENT)
	// Main-thread diagnostic history survives Graphics recreation; scopes read the cached expected interval in O(1).
	common::Smoothed<int64_t, 256> mRenderedFrameIntervals;
	enum class DiagnosticFlags : uint8_t
	{
		kFocused = 1 << 0,
		kDiscardNextInterval = 1 << 1,
	};
	common::Flags<DiagnosticFlags> mDiagnosticFlags {DiagnosticFlags::kFocused};

	VkQueryPool mVkQueryPool = VK_NULL_HANDLE;
	struct GpuSubmission
	{
		int64_t iSequence = 0;
		bool bWorld = false;
		std::chrono::nanoseconds expectedInterval = 0ns;
		std::chrono::steady_clock::time_point submitTime {};
	};
	struct GpuDiagnosticSample
	{
		GpuSubmission submission;
		int64_t iFramebuffer = 0;
		std::array<std::optional<int64_t>, kGpuTimerCount> microseconds {};

		std::optional<int64_t> RootMicroseconds() const;
	};
	void LogGpuSample(const GpuDiagnosticSample& rSample, std::string_view pcContext);
	std::vector<GpuSubmission> mGpuSubmissions;
	int64_t miGpuSubmissionSequence = 0;
	std::array<common::Smoothed<int64_t>, kGpuTimerCount> mGpuDiagnosticHistories {};
	GpuDiagnosticSample mLatestGpuSample {};

	// Created at submit and completed later by the fence, image-wait, and retirement hooks.
	struct GpuFrameTimeline
	{
		int64_t iSequence = 0;
		int64_t iFramebuffer = 0;
		bool bWorld = false;
		bool bAcquireBarriers = false;
		std::chrono::steady_clock::time_point submitTime {};
		// Start/end per root: Global, Main, Image, UiRender.
		std::array<std::optional<uint64_t>, 8> rootTicks {};
		std::optional<std::chrono::steady_clock::time_point> optionalFenceObservedTime;
		// First prior-acquisition fence wait after this submission.
		std::optional<std::chrono::steady_clock::time_point> optionalImageWaitStart;
		std::optional<std::chrono::steady_clock::time_point> optionalImageWaitEnd;
	};
	static constexpr int64_t kiGpuTimelineCapacity = 16;
	// Indexed by submission sequence modulo capacity.
	std::array<GpuFrameTimeline, kiGpuTimelineCapacity> mGpuTimeline {};
	// Graphics stall whose timeline prints when submission miPendingStallSequence retires; 0 = none.
	int64_t miPendingStallSequence = 0;
	const char* mpcPendingStallOperation = nullptr;
	std::chrono::steady_clock::time_point mPendingStallStart {};
	void LogStallTimeline(const GpuDiagnosticSample& rSample, bool bAnomaly);

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

inline ProfileManager* gpProfileManager = nullptr;

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

#if defined(BT_CLIENT)
struct AcquisitionDiagnostics
{
	struct AcquireCall
	{
		int64_t iNanoseconds = 0;
		VkResult vkResult = VK_SUCCESS;
	};

	uint64_t uiSequence = 0;
	int64_t iFrameCounter = 0;
	int64_t iIncomingFramebuffer = 0;
	bool bFenceWaitRequired = false;
	std::optional<int64_t> optionalFenceNanoseconds;
	std::optional<AcquireCall> optionalAcquireCall;
	std::optional<uint32_t> optionalImageIndex;
};

class [[nodiscard]] ScopedSlowOperation
{
public:

	ScopedSlowOperation(common::LogCategory eCategory, const char* pcOperation, const ReconciliationDiagnostics* pReconciliationDiagnostics = nullptr, const AcquisitionDiagnostics* pAcquisitionDiagnostics = nullptr);
	~ScopedSlowOperation();
	ScopedSlowOperation(const ScopedSlowOperation&) = delete;
	ScopedSlowOperation& operator=(const ScopedSlowOperation&) = delete;

private:

	common::LogCategory meCategory = kDefault;
	const char* mpcOperation = nullptr;
	const ReconciliationDiagnostics* mpReconciliationDiagnostics = nullptr;
	const AcquisitionDiagnostics* mpAcquisitionDiagnostics = nullptr;
	std::chrono::nanoseconds mExpectedRenderedFrameInterval = 0ns;
	std::chrono::steady_clock::time_point mStartTime = std::chrono::steady_clock::time_point();
};
#endif // BT_CLIENT

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

using engine::gpProfileManager;
