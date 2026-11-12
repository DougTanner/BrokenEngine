#include "Pch.h"

#include "ProfileManager.h"

#include "Memory/GlobalAllocator.h"

#include "Profile/ProfileRows.h"
#include "Game.h"

namespace engine
{

// Game timer and counter indices follow the engine indices contiguously. The timer anchor is kCpuTimerFrameUpdate, which engine phase scopes use; the counter anchor is kGameCpuCounterFirst.
static_assert(static_cast<int64_t>(game::kCpuTimerFrameUpdate) == static_cast<int64_t>(kEngineCpuTimerCount), "First game CPU timer must start at kEngineCpuTimerCount (contiguous engine->game index space).");
static_assert(static_cast<int64_t>(game::kGameCpuCounterFirst) == static_cast<int64_t>(kEngineCpuCounterCount), "First game CPU counter must start at kEngineCpuCounterCount (contiguous engine->game index space).");

// All profile-text rows re-evaluate their show/hide state together on this cadence; a state therefore persists at least this long.
constexpr std::chrono::seconds kProfileVisibilityInterval = 2s;

#if defined(BT_CLIENT)
// kbProfilingDump mode: one CSV sample of every timer/counter per interval, written for offline analysis.
constexpr std::chrono::seconds kProfileDumpInterval = 1s;
constexpr double kfGpuAnomalyMultiplier = 1.9;
#endif // BT_CLIENT

ProfileManager::ProfileManager()
: common::Singleton<ProfileManager>(gpProfileManager, this)
{
	miCpuCounterCount = game::kGameCpuCounterCount;
	miCpuTimerCount = game::kGameCpuTimerCount;
	mpCpuCounters = std::make_unique<CpuCounter[]>(static_cast<size_t>(miCpuCounterCount));
	mpCpuTimers = std::make_unique<CpuTimer[]>(static_cast<size_t>(miCpuTimerCount));
#if defined(BT_SERVER)
	if constexpr (kbProfiling)
	{
		mpRawCpuTimers = std::make_unique<RawCpuTimerState[]>(static_cast<size_t>(miCpuTimerCount));
	}
#endif // BT_SERVER

	if constexpr (kbProfiling)
	{
#if defined(BT_SERVER)
		RegisterRawCpuTimer(game::kCpuTimerPostRenderUpdateNavigationQuery);
#endif // BT_SERVER

		BootStart(kBootTimerTotal);
	}
}

void ProfileManager::Create()
{
	if constexpr (kbProfiling)
	{
#if defined(BT_CLIENT)
		// Graphics::Create calls this after Instance, Device, and Swapchain creation; those managers must be live.
		if constexpr (kbProfilingDump)
		{
			if (mpDumpLog == nullptr)
			{
				// Initial log creation runs before allocation tracking is armed. DiagnosticLog creates the parent directory and writes through a stack buffer, suppressing allocation tracking and flushing each line.
				wchar_t pcDirectory[MAX_PATH] {};
				int64_t iTempPathLength = GetTempPathW(static_cast<DWORD>(std::size(pcDirectory) - 1), pcDirectory);
				if (iTempPathLength != 0 && iTempPathLength < std::ssize(pcDirectory) - 1)
				{
					std::filesystem::path filename(pcDirectory);
					filename /= game::kGameName;
					filename /= "ProfileDump.csv";
					mpDumpLog = std::make_unique<common::DiagnosticLog>(3, filename.string().c_str());
					mpDumpLog->Write("ms,type,index,name,current,average,max,allocations");

					mDumpStartTime = std::chrono::steady_clock::now();
					mLastDumpTime = mDumpStartTime;
				}
			}
		}

		if (mVkQueryPool != VK_NULL_HANDLE)
		{
			return;
		}

		int64_t iTimestampValidBits = gpInstanceManager->mVkQueueFamilyProperties.at(gpInstanceManager->miGraphicsQueueFamilyIndex).timestampValidBits;
		if (iTimestampValidBits == 0)
		{
			LOG(kDefault, kWarning, "Warning: Graphics queue family does not support timestamp queries. GPU profiling disabled.");
			return;
		}

		int64_t iQueryCount = std::ssize(gpSwapchainManager->mFramebuffers) * 2 * kGpuTimerCount;

		VkQueryPoolCreateInfo vkQueryPoolCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queryType = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = static_cast<uint32_t>(iQueryCount),
			.pipelineStatistics = 0,
		};

		CHECK_VK(vkCreateQueryPool(gpDeviceManager->mVkDevice, &vkQueryPoolCreateInfo, nullptr, &mVkQueryPool));
		mGpuSubmissions.resize(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
		VkName(VK_OBJECT_TYPE_QUERY_POOL, mVkQueryPool, "Timestamp");

		// Queries must be reset before first use; no submitted work references this fresh pool yet.
		vkResetQueryPool(gpDeviceManager->mVkDevice, mVkQueryPool, 0, static_cast<uint32_t>(iQueryCount));
#endif // BT_CLIENT
	}
}

void ProfileManager::Destroy()
{
	if constexpr (kbProfiling)
	{
#if defined(BT_CLIENT)
		if (gpDeviceManager != nullptr && mVkQueryPool != VK_NULL_HANDLE)
		{
			vkDestroyQueryPool(gpDeviceManager->mVkDevice, mVkQueryPool, nullptr);
		}

		mVkQueryPool = VK_NULL_HANDLE;
		mGpuShadowSample = {};
		mGpuSubmissions.clear();
		mGpuDiagnosticHistories = {};
		mLatestGpuSample = {};

		// mpDumpLog deliberately not reset: Destroy() also runs on swapchain-tier recreates (resize, settings), and the dump file must span the whole session. The unique_ptr closes it at ProfileManager destruction.
#endif // BT_CLIENT
	}
}

bool ProfileManager::TickVisibilityCadence()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	bool bReevaluate = now - mLastVisibilityEvaluationTime >= kProfileVisibilityInterval;
	if (bReevaluate)
	{
		mLastVisibilityEvaluationTime = now;
	}

	return bReevaluate;
}

void ProfileManager::ToggleProfileText()
{
	if constexpr (kbProfiling)
	{
		meProfileScreen = static_cast<ProfileScreen>((static_cast<uint8_t>(meProfileScreen) + 1) % static_cast<uint8_t>(ProfileScreen::kCount));

		// Re-evaluate visibility immediately on the switched-to screen instead of showing stale flags.
		mLastVisibilityEvaluationTime = {};

#if defined(BT_CLIENT)
		for (int64_t i = kTextGraphics; i < kTextAreasCount; ++i)
		{
			gpImGuiManager->UpdateTextArea(static_cast<TextAreas>(i), "");
		}
#endif
	}
}

void ProfileManager::CpuStart(int64_t iCpuTimer, int64_t iThreads)
{
	if constexpr (kbProfiling)
	{
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		int64_t iAllocations = giAllocationsThisFrame.load(std::memory_order_relaxed);

		std::lock_guard lock(mCpuTimerMutex);

		// Exclude timer-state insertion and resizing from allocation measurements.
		ScopedSuppressAllocationTracking suppress;

		std::vector<CpuTimerThreadState>& rThreadStates = mPerThreadTimerStates.try_emplace(std::this_thread::get_id()).first->second;
		if (std::ssize(rThreadStates) < miCpuTimerCount)
		{
			rThreadStates.resize(static_cast<size_t>(miCpuTimerCount));
		}

		CpuTimerThreadState& rState = rThreadStates.at(static_cast<size_t>(iCpuTimer));
		ASSERT(rState.startTimePoint == std::chrono::steady_clock::time_point());
		rState.startTimePoint = now;
		rState.iStartAllocations = iAllocations;

		GetCpuTimer(iCpuTimer).iThreads = iThreads;
	}
}

void ProfileManager::CpuStop(int64_t iCpuTimer, CpuStopFlags_t flags)
{
	if constexpr (kbProfiling)
	{
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		int64_t iAllocations = giAllocationsThisFrame.load(std::memory_order_relaxed);

		std::lock_guard lock(mCpuTimerMutex);

		CpuTimerThreadState* pState = nullptr;

		if (flags & CpuStopFlags::kCrossThread)
		{
			for (auto& [rThreadId, rStates] : mPerThreadTimerStates)
			{
				if (rStates.at(static_cast<size_t>(iCpuTimer)).startTimePoint != std::chrono::steady_clock::time_point())
				{
					pState = &rStates.at(static_cast<size_t>(iCpuTimer));
					break;
				}
			}
		}
		else
		{
			std::vector<CpuTimerThreadState>& rThreadStates = mPerThreadTimerStates.try_emplace(std::this_thread::get_id()).first->second;
			if (std::ssize(rThreadStates) < miCpuTimerCount)
			{
				// Exclude timer-state resizing from allocation measurements.
				ScopedSuppressAllocationTracking suppress;
				rThreadStates.resize(static_cast<size_t>(miCpuTimerCount));
			}
			pState = &rThreadStates.at(static_cast<size_t>(iCpuTimer));
			ASSERT(pState->startTimePoint != std::chrono::steady_clock::time_point());
		}

		CpuTimer& rCpuTimer = GetCpuTimer(iCpuTimer);

		if (pState != nullptr) [[likely]]
		{
			int64_t iElapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now - pState->startTimePoint).count();
			rCpuTimer.iTotalFrameTimeNanoseconds += iElapsedNanoseconds;
			pState->startTimePoint = std::chrono::steady_clock::time_point();
			rCpuTimer.iAllocationsThisFrame += std::max(0i64, iAllocations - pState->iStartAllocations);

#if defined(BT_SERVER)
			if constexpr (kbProfiling)
			{
				RawCpuTimerState& rRawTimer = mpRawCpuTimers[static_cast<size_t>(iCpuTimer)];
				if (rRawTimer.flags & RawCpuTimerStateFlags::kRegistered)
				{
					rRawTimer.iTotalTimeNanoseconds += iElapsedNanoseconds;
					++rRawTimer.iInvocationCount;
				}
			}
#endif // BT_SERVER
		}

		if (flags & CpuStopFlags::kSmoothNow) [[unlikely]]
		{
			rCpuTimer.flags.Set(ProfileRowFlags::kSmoothAtStop);
			rCpuTimer.smoothedMicroseconds = rCpuTimer.iTotalFrameTimeNanoseconds / 1'000;
			rCpuTimer.iTotalFrameTimeNanoseconds = 0;
			rCpuTimer.smoothedAllocations = rCpuTimer.iAllocationsThisFrame;
			rCpuTimer.iAllocationsThisFrame = 0;
		}
	}
}

#if defined(BT_SERVER)
void ProfileManager::RegisterRawCpuTimer(int64_t iCpuTimer)
{
	if constexpr (kbProfiling)
	{
		ASSERT(mpRawCpuTimers != nullptr);
		mpRawCpuTimers[static_cast<size_t>(iCpuTimer)].flags.Set(RawCpuTimerStateFlags::kRegistered);
	}
}

void ProfileManager::AddRawCpuTimerAuxiliaryCount(int64_t iCpuTimer, int64_t iCount)
{
	if constexpr (kbProfiling)
	{
		mpRawCpuTimers[static_cast<size_t>(iCpuTimer)].iAuxiliaryCount.fetch_add(iCount, std::memory_order_relaxed);
	}
}

void ProfileManager::LatchRawCpuTimers(bool bAccept)
{
	if constexpr (kbProfiling)
	{
		std::lock_guard lock(mCpuTimerMutex);
		for (int64_t i = 0; i < miCpuTimerCount; ++i)
		{
			RawCpuTimerState& rRawTimer = mpRawCpuTimers[static_cast<size_t>(i)];
			if (!(rRawTimer.flags & RawCpuTimerStateFlags::kRegistered))
			{
				continue;
			}

			int64_t iAuxiliaryCount = rRawTimer.iAuxiliaryCount.exchange(0, std::memory_order_relaxed);
			if (bAccept)
			{
				++rRawTimer.record.iSampleSequence;
				rRawTimer.record.iSampleMicroseconds = rRawTimer.iTotalTimeNanoseconds / 1'000;
				rRawTimer.record.iInvocationCount = rRawTimer.iInvocationCount;
				rRawTimer.record.iAuxiliaryCount = iAuxiliaryCount;
			}
			rRawTimer.iTotalTimeNanoseconds = 0;
			rRawTimer.iInvocationCount = 0;
		}
	}
}
#endif // BT_SERVER

#if defined(BT_CLIENT)
void ProfileManager::ResetQueryPools(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eStart, GpuTimers eEnd)
{
	if constexpr (kbProfiling)
	{
		if (mVkQueryPool == VK_NULL_HANDLE)
		{
			return;
		}

		int64_t iIndex = 2 * (kGpuTimerCount * iCommandBuffer + eStart);
		int64_t iCount = 2 * (eEnd - eStart);
		vkCmdResetQueryPool(vkCommandBuffer, mVkQueryPool, static_cast<uint32_t>(iIndex), static_cast<uint32_t>(iCount));
	}
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void ProfileManager::GpuStart(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eGpuTimer)
{
	if constexpr (kbProfiling)
	{
		if (mVkQueryPool == VK_NULL_HANDLE)
		{
			return;
		}

		int64_t iCounterIndex = 2 * kGpuTimerCount * iCommandBuffer + 2 * eGpuTimer;
		vkCmdWriteTimestamp(vkCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mVkQueryPool, static_cast<uint32_t>(iCounterIndex));

		bool bRoot = eGpuTimer == kGpuTimerGlobal || eGpuTimer == kGpuTimerMain || eGpuTimer == kGpuTimerImage;
		if (vkCmdBeginDebugUtilsLabelEXT != nullptr)
		{
			float fColor = bRoot ? 0.5f : 0.0f;
			VkDebugUtilsLabelEXT vkDebugUtilsLabelEXT =
			{
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pLabelName = kpGpuTimerNames[eGpuTimer].data(),
				.color = {fColor, fColor, fColor, fColor},
			};
			vkCmdBeginDebugUtilsLabelEXT(vkCommandBuffer, &vkDebugUtilsLabelEXT);
		}
	}
}

void ProfileManager::GpuStop(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, GpuTimers eGpuTimer)
{
	if constexpr (kbProfiling)
	{
		if (mVkQueryPool == VK_NULL_HANDLE)
		{
			return;
		}

		int64_t iCounterIndex = 2 * kGpuTimerCount * iCommandBuffer + 2 * eGpuTimer + 1;
		vkCmdWriteTimestamp(vkCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mVkQueryPool, static_cast<uint32_t>(iCounterIndex));

		if (vkCmdEndDebugUtilsLabelEXT != nullptr)
		{
			vkCmdEndDebugUtilsLabelEXT(vkCommandBuffer);
		}
	}
}

void ProfileManager::GpuSubmitted(int64_t iCommandBuffer, bool bWorld)
{
	if constexpr (kbProfiling)
	{
		if (mVkQueryPool == VK_NULL_HANDLE)
		{
			return;
		}
		mGpuSubmissions.at(iCommandBuffer) = GpuSubmission {
			.iSequence = ++miGpuSubmissionSequence,
			.bWorld = bWorld,
			.expectedInterval = mExpectedRenderedFrameInterval,
			.submitTime = std::chrono::steady_clock::now(),
		};
	}
}

void ProfileManager::GpuRead(int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		if (mVkQueryPool == VK_NULL_HANDLE)
		{
			return;
		}

		GpuDiagnosticSample sample {.submission = mGpuSubmissions.at(iCommandBuffer), .iFramebuffer = iCommandBuffer};
		mGpuSubmissions.at(iCommandBuffer) = {};
		if (sample.submission.iSequence == 0)
		{
			return;
		}
		int64_t iStart = sample.submission.bWorld ? kGpuTimerGlobal : kGpuTimerUiRender;
		for (int64_t i = iStart; i < kGpuTimerCount; ++i)
		{
			GpuTimers eGpuTimer = static_cast<GpuTimers>(i);
			uint64_t puiResults[2] {};
			int64_t iCounterIndex = 2 * kGpuTimerCount * iCommandBuffer + 2 * eGpuTimer;
			VkResult vkResultGetQueryPoolResults = vkGetQueryPoolResults(gpDeviceManager->mVkDevice, mVkQueryPool, static_cast<uint32_t>(iCounterIndex), 2, sizeof(puiResults), puiResults, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
			if (vkResultGetQueryPoolResults == VK_NOT_READY)
			{
				continue;
			}
			CHECK_VK(vkResultGetQueryPoolResults);

			int64_t iCurrentMicroseconds = static_cast<int64_t>(static_cast<float>(puiResults[1] - puiResults[0]) * gpInstanceManager->mVkPhysicalDeviceProperties.limits.timestampPeriod / 1'000.0f);
			mpGpuTimers[eGpuTimer].smoothedMicroseconds = iCurrentMicroseconds;
			sample.microseconds[i] = iCurrentMicroseconds;

			if (sample.submission.bWorld && eGpuTimer == kGpuTimerShadow)
			{
				++mGpuShadowSample.iSequence;
				mGpuShadowSample.iCurrentMicroseconds = iCurrentMicroseconds;
			}
		}
		if constexpr (kbProfilingFrameSpike)
		{
			if (sample.submission.expectedInterval == 0ns || mExpectedRenderedFrameInterval == 0ns)
			{
				return;
			}
			double fExpectedMicroseconds = std::chrono::duration<double, std::micro>(sample.submission.expectedInterval).count();
			std::optional<int64_t> optionalRootMicroseconds = sample.RootMicroseconds();
			bool bAnomaly = optionalRootMicroseconds && static_cast<double>(*optionalRootMicroseconds) > kfGpuAnomalyMultiplier * fExpectedMicroseconds;
			for (int64_t i = iStart; i < kGpuTimerCount; ++i)
			{
				if (!sample.microseconds[i])
				{
					continue;
				}
				common::Smoothed<int64_t>& rHistory = mGpuDiagnosticHistories[i];
				double fCurrent = static_cast<double>(*sample.microseconds[i]);
				if (rHistory.miCount == common::Smoothed<int64_t>::kiCapacity)
				{
					double fAverage = static_cast<double>(rHistory.Average());
					bAnomaly |= fCurrent > kfGpuAnomalyMultiplier * fAverage && fCurrent - fAverage >= 0.25 * fExpectedMicroseconds;
				}
				rHistory = *sample.microseconds[i];
			}
			mLatestGpuSample = sample;
			if (bAnomaly)
			{
				LogGpuSample(sample, "GPU anomaly");
			}
		}
	}
}

std::optional<int64_t> ProfileManager::GpuDiagnosticSample::RootMicroseconds() const
{
	int64_t iTotal = 0;
	for (GpuTimers eRoot : {kGpuTimerGlobal, kGpuTimerMain, kGpuTimerImage, kGpuTimerUiRender})
	{
		if (!submission.bWorld && eRoot != kGpuTimerUiRender)
		{
			continue;
		}
		if (!microseconds[eRoot])
		{
			return std::nullopt;
		}
		iTotal += *microseconds[eRoot];
	}
	return iTotal;
}

void ProfileManager::LogGpuSample(const GpuDiagnosticSample& rSample, std::string_view pcContext)
{
	float fAgeMilliseconds = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - rSample.submission.submitTime).count();
	double fExpectedMilliseconds = std::chrono::duration<double, std::milli>(rSample.submission.expectedInterval).count();
	LOG(kGraphics, kWarning, "{}: submission={} framebuffer={} mode={} age={}ms expected={}ms threshold={}ms", pcContext, rSample.submission.iSequence, rSample.iFramebuffer, rSample.submission.bWorld ? "world" : "UI", common::Wb(fAgeMilliseconds, 3), common::Wb(static_cast<float>(fExpectedMilliseconds), 3), common::Wb(static_cast<float>(kfGpuAnomalyMultiplier * fExpectedMilliseconds), 3));
	std::optional<int64_t> optionalRootMicroseconds = rSample.RootMicroseconds();
	if (optionalRootMicroseconds)
	{
		LOG(kGraphics, kWarning, "  Measured GPU regions: {}us", *optionalRootMicroseconds);
	}
	else
	{
		LOG(kGraphics, kWarning, "  Measured GPU regions: unavailable (missing root)");
	}
	int64_t iStart = rSample.submission.bWorld ? kGpuTimerGlobal : kGpuTimerUiRender;
	for (int64_t i = iStart; i < kGpuTimerCount; ++i)
	{
		if (rSample.microseconds[i])
		{
			LOG(kGraphics, kWarning, "  {}: {}us", kpGpuTimerNames[i], *rSample.microseconds[i]);
		}
		else
		{
			LOG(kGraphics, kWarning, "  {}: unavailable", kpGpuTimerNames[i]);
		}
	}
	LOG(kGraphics, kWarning, "  Child rows overlap roots; do not add them. Excludes uninstrumented upload/acquire work and queue/presentation waiting. Delayed readback is not same-frame CPU attribution.");
}

void ProfileManager::LogLatestGpuSample()
{
	if constexpr (kbProfilingFrameSpike)
	{
		if (mLatestGpuSample.submission.iSequence == 0)
		{
			LOG(kGraphics, kWarning, "  Older GPU context: unavailable (no eligible completed sample)");
			return;
		}
		LogGpuSample(mLatestGpuSample, "Older completed GPU context");
	}
}

void ProfileManager::SetClockCorrection(int64_t iOffset, int64_t iTargetBehind, int64_t iError)
{
	mSmoothedClockOffset = iOffset;
	mSmoothedClockTarget = iTargetBehind;
	mSmoothedClockError = iError;
	mSmoothedClockOffset.Update();
	mSmoothedClockTarget.Update();
	mSmoothedClockError.Update();
}
#endif // BT_CLIENT

void ProfileManager::BootStart(BootTimers eBootTimer)
{
	if constexpr (kbProfiling)
	{
		mpBootTimers[eBootTimer].startTimePoint = std::chrono::steady_clock::now();
	}
}

void ProfileManager::BootStop(BootTimers eBootTimer)
{
	if constexpr (kbProfiling)
	{
		BootTimer& rBootTimer = mpBootTimers[eBootTimer];
		rBootTimer.timeNanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - rBootTimer.startTimePoint);
	}
}

void ProfileManager::BootLog()
{
	if constexpr (kbProfiling)
	{
		BootStop(kBootTimerTotal);

		for (int64_t i = 0; i < kBootTimerCount; ++i)
		{
			std::chrono::milliseconds durationMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(mpBootTimers[i].timeNanoseconds);
			if (durationMilliseconds.count() > 10)
			{
				LOG(kDefault, kDebug, "{}: {} ms", kpBootTimerNames[i], durationMilliseconds.count());
			}
		}
		LOG(kDefault, kDebug, "\n");
	}
}

void ProfileManager::LogTimers()
{
	if constexpr (kbProfiling)
	{
		LOG(kDefault, kDebug, "");

		{
			std::lock_guard lock(mCpuTimerMutex);

			int64_t iCpuTimerCount = miCpuTimerCount;
			for (int64_t i = 0; i < iCpuTimerCount; ++i)
			{
				CpuTimer& rCpuTimer = GetCpuTimer(i);
				LOG(kDefault, kDebug, "{}: {} ({}, {}) [{}]", GetCpuTimerName(i), rCpuTimer.smoothedMicroseconds.Current(), rCpuTimer.smoothedMicroseconds.Average(), rCpuTimer.smoothedMicroseconds.Maximum(), rCpuTimer.smoothedAllocations.mSmoothedValue);
			}
		}

		LOG(kDefault, kDebug, "");

#if defined(BT_CLIENT)
		for (int64_t i = 0; i < kGpuTimerCount; ++i)
		{
			GpuTimer& rGpuTimer = mpGpuTimers[i];
			LOG(kDefault, kDebug, "{}: {} ({}, {})", kpGpuTimerNames[i], rGpuTimer.smoothedMicroseconds.Current(), rGpuTimer.smoothedMicroseconds.Average(), rGpuTimer.smoothedMicroseconds.Maximum());
		}

		LOG(kDefault, kDebug, "");
#endif // BT_CLIENT
	}
}

#if defined(BT_CLIENT)
void ProfileManager::DumpTimers()
{
	if constexpr (kbProfiling && kbProfilingDump)
	{
		if (mpDumpLog == nullptr)
		{
			return;
		}

		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		if (now - mLastDumpTime < kProfileDumpInterval)
		{
			return;
		}
		mLastDumpTime = now;

		int64_t iMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - mDumpStartTime).count();

		mpDumpLog->Write("{},meta,0,Fps,{},,,", iMilliseconds, gpGraphics->mRendersInTheLastSecond.Get());
		mpDumpLog->Write("{},meta,1,FullUpdates,{},,,", iMilliseconds, mFullUpdatesInTheLastSecond.Get());
		mpDumpLog->Write("{},meta,2,InterpolateUpdates,{},,,", iMilliseconds, mInterpolateUpdatesInTheLastSecond.Get());

		for (int64_t i = 0; i < kGpuTimerCount; ++i)
		{
			common::Smoothed<int64_t>& rMicroseconds = mpGpuTimers[i].smoothedMicroseconds;
			mpDumpLog->Write("{},gpu,{},{},{},{},{},", iMilliseconds, i, kpGpuTimerNames[i], rMicroseconds.Current(), rMicroseconds.Average(), rMicroseconds.Maximum());
		}

		// Snapshot under the lock, write after: DiagnosticLog flushes per line, and dozens of flushed lines under mCpuTimerMutex would block dispatch/submit/network CpuStart/CpuStop once per second, distorting the timers being measured.
		struct CpuTimerSample
		{
			int64_t iCurrentMicroseconds = 0;
			int64_t iAverageMicroseconds = 0;
			int64_t iMaximumMicroseconds = 0;
			int64_t iAllocations = 0;
		};
		int64_t iCpuTimerCount = miCpuTimerCount;
		auto samples = common::gpThreadLocal->mWorkbuffer.PushBuffer<CpuTimerSample*>(iCpuTimerCount * static_cast<int64_t>(sizeof(CpuTimerSample)));
		{
			std::lock_guard lock(mCpuTimerMutex);
			for (int64_t i = 0; i < iCpuTimerCount; ++i)
			{
				CpuTimer& rCpuTimer = GetCpuTimer(i);
				samples.mpData[i] = CpuTimerSample {.iCurrentMicroseconds = rCpuTimer.smoothedMicroseconds.Current(), .iAverageMicroseconds = rCpuTimer.smoothedMicroseconds.Average(), .iMaximumMicroseconds = rCpuTimer.smoothedMicroseconds.Maximum(), .iAllocations = rCpuTimer.smoothedAllocations.Current()};
			}
		}
		for (int64_t i = 0; i < iCpuTimerCount; ++i)
		{
			mpDumpLog->Write("{},cpu,{},{},{},{},{},{}", iMilliseconds, i, GetCpuTimerName(i), samples.mpData[i].iCurrentMicroseconds, samples.mpData[i].iAverageMicroseconds, samples.mpData[i].iMaximumMicroseconds, samples.mpData[i].iAllocations);
		}

		int64_t iCpuCounterCount = miCpuCounterCount;
		for (int64_t i = 0; i < iCpuCounterCount; ++i)
		{
			mpDumpLog->Write("{},counter,{},{},{},,,", iMilliseconds, i, GetCpuCounterName(i), GetCpuCounter(i).iCount);
		}
	}
}

void ProfileManager::RecordRenderedFrameInterval(std::chrono::nanoseconds interval)
{
	if constexpr (kbProfilingFrameSpike)
	{
		if (!(mDiagnosticFlags & DiagnosticFlags::kFocused))
		{
			return;
		}
		if (mDiagnosticFlags & DiagnosticFlags::kDiscardNextInterval)
		{
			mDiagnosticFlags.Set(DiagnosticFlags::kDiscardNextInterval, false);
			return;
		}
		mRenderedFrameIntervals = interval.count();
		if (mRenderedFrameIntervals.miCount > 200)
		{
			mExpectedRenderedFrameInterval = std::chrono::nanoseconds(mRenderedFrameIntervals.Average());
		}
	}
}

void ProfileManager::SetDiagnosticFocus(bool bHasFocus)
{
	if constexpr (kbProfilingFrameSpike)
	{
		mRenderedFrameIntervals = common::Smoothed<int64_t, 256> {};
		mNormalReconciliationTickNanoseconds = common::Smoothed<double> {};
		mExpectedRenderedFrameInterval = 0ns;
		mGpuDiagnosticHistories = {};
		mLatestGpuSample = {};
		for (GpuSubmission& rSubmission : mGpuSubmissions)
		{
			rSubmission.expectedInterval = 0ns;
		}
		mDiagnosticFlags.Set(DiagnosticFlags::kFocused, bHasFocus);
		mDiagnosticFlags.Set(DiagnosticFlags::kDiscardNextInterval);
	}
}

void ProfileManager::DiscardSkippedFrameSamples()
{
	if constexpr (kbProfiling)
	{
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

		std::lock_guard lock(mCpuTimerMutex);

		int64_t iCpuTimerCount = miCpuTimerCount;
		for (int64_t i = 0; i < iCpuTimerCount; ++i)
		{
			CpuTimer& rCpuTimer = GetCpuTimer(i);
			rCpuTimer.iTotalFrameTimeNanoseconds = 0;
			rCpuTimer.iAllocationsThisFrame = 0;
		}

		giAllocationsThisFrame.store(0, std::memory_order_relaxed);

		// AcquireToGlobal stays running across the skipped frames; restart it now. The boot render loop can resume before it ever started, so a missing state is valid.
		for (auto& [rThreadId, rStates] : mPerThreadTimerStates)
		{
			if (rStates.at(static_cast<size_t>(kCpuTimerAcquireToGlobal)).startTimePoint != std::chrono::steady_clock::time_point())
			{
				CpuTimerThreadState& rState = rStates.at(static_cast<size_t>(kCpuTimerAcquireToGlobal));
				rState.startTimePoint = now;
				rState.iStartAllocations = 0;
				break;
			}
		}
	}
}
#endif // BT_CLIENT

void ProfileManager::SmoothCpuTimers()
{
	if constexpr (kbProfiling)
	{
		std::lock_guard lock(mCpuTimerMutex);
		int64_t iCpuTimerCount = miCpuTimerCount;
		for (int64_t i = 0; i < iCpuTimerCount; ++i)
		{
			CpuTimer& rCpuTimer = GetCpuTimer(i);
			if (!(rCpuTimer.flags & ProfileRowFlags::kSmoothAtStop))
			{
				rCpuTimer.smoothedMicroseconds = rCpuTimer.iTotalFrameTimeNanoseconds / 1'000;
				rCpuTimer.iTotalFrameTimeNanoseconds = 0;
				rCpuTimer.smoothedAllocations = rCpuTimer.iAllocationsThisFrame;
				rCpuTimer.iAllocationsThisFrame = 0;
			}
			rCpuTimer.smoothedMicroseconds.Update();
			rCpuTimer.smoothedAllocations.Update();
		}
	}
}

void ProfileManager::UpdateProfileText()
{
	if constexpr (kbProfiling)
	{
		ScopedCpuProfile scopedCpuProfile(kCpuTimerUpdateProfileText);

		SmoothCpuTimers();

		mSmoothedAllocations = giAllocationsThisFrame.exchange(0, std::memory_order_relaxed);
		mSmoothedAllocations.Update();

#if defined(BT_CLIENT)
		for (GpuTimer& rGpuTimer : mpGpuTimers)
		{
			rGpuTimer.smoothedMicroseconds.Update();
		}

		// Before the kOff early-out so the dump runs with the overlay hidden.
		if constexpr (kbProfilingDump)
		{
			DumpTimers();
		}

		if constexpr (kbProfileScreens)
		{
			if (meProfileScreen == ProfileScreen::kOff)
			{
				return;
			}

			bool bReevaluate = TickVisibilityCadence();

			common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

			if (meProfileScreen == ProfileScreen::kCpu || meProfileScreen == ProfileScreen::kGpu)
			{
				std::chrono::microseconds elapsedCpuTime(GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.mSmoothedValue);
				FormatFramesPerSecondHeader(rWorkbuffer, elapsedCpuTime);
			}

			if (meProfileScreen == ProfileScreen::kCpu)
			{
				FormatCpuScreen(rWorkbuffer, bReevaluate);
			}

			if (meProfileScreen == ProfileScreen::kGpu)
			{
				FormatGpuScreen(rWorkbuffer, bReevaluate);
			}

			if (meProfileScreen == ProfileScreen::kNetwork)
			{
				FormatNetworkScreen(rWorkbuffer);
			}
		}
#endif // BT_CLIENT
	}
}

CpuCounter& ProfileManager::GetCpuCounter(int64_t iIndex)
{
	return mpCpuCounters[static_cast<size_t>(iIndex)];
}

CpuTimer& ProfileManager::GetCpuTimer(int64_t iIndex)
{
	return mpCpuTimers[static_cast<size_t>(iIndex)];
}

std::string_view ProfileManager::GetCpuCounterName(int64_t iIndex)
{
	return iIndex < kEngineCpuCounterCount ? kpEngineCpuCounterNames[iIndex] : game::kpGameCpuCounterNames[iIndex - kEngineCpuCounterCount];
}

std::string_view ProfileManager::GetCpuTimerName(int64_t iIndex)
{
	return iIndex < kEngineCpuTimerCount ? kpEngineCpuTimerNames[iIndex] : game::kpGameCpuTimerNames[iIndex - kEngineCpuTimerCount];
}

ScopedBootTimer::ScopedBootTimer(BootTimers eBootTimer)
{
	meBootTimer = eBootTimer;
	if constexpr (kbProfiling)
	{
		gpProfileManager->BootStart(meBootTimer);
	}
}

ScopedBootTimer::~ScopedBootTimer()
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->BootStop(meBootTimer);
	}
}

ScopedCpuProfile::ScopedCpuProfile(int64_t iCpuTimer, int64_t iThreads)
{
	miCpuTimer = iCpuTimer;
	if constexpr (kbProfiling)
	{
		gpProfileManager->CpuStart(miCpuTimer, iThreads);
	}
}

ScopedCpuProfile::~ScopedCpuProfile()
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->CpuStop(miCpuTimer);
	}
}

#if defined(BT_CLIENT)
static void LogReconciliationDiagnostics(const ReconciliationDiagnostics& rDetails, std::chrono::nanoseconds elapsed, const common::Smoothed<double>& rNormalTickNanoseconds)
{
	static constexpr double kfMinimumTimingFraction = 0.05;
	auto IsRelevantTiming = [](int64_t iNanoseconds, int64_t iParentNanoseconds)
	{
		return iNanoseconds > 0 && static_cast<double>(iNanoseconds) >= kfMinimumTimingFraction * static_cast<double>(iParentNanoseconds);
	};
	auto Milliseconds = [](int64_t iNanoseconds)
	{
		return static_cast<float>(iNanoseconds) / 1'000'000.0f;
	};

	if (rDetails.iDispatcherNanoseconds > 0)
	{
		LOG(kNetwork, kWarning, "  Dispatcher wall time: {}ms", common::Wb(Milliseconds(rDetails.iDispatcherNanoseconds), 3));
	}
	if (rDetails.iExecutedCoordTicks > 0)
	{
		if (rDetails.iTickExecutionNanoseconds > 0)
		{
			LOG(kNetwork, kWarning, "    Summed RunFrameTick wall time (parallel workers): {}ms", common::Wb(Milliseconds(rDetails.iTickExecutionNanoseconds), 3));
		}
		LOG(kNetwork, kWarning, "    Coords: {} Executed coord-ticks: {} New: {}", rDetails.iActiveCoords, rDetails.iExecutedCoordTicks, rDetails.iExecutedCoordTicks - rDetails.iRecalculatedCoordTicks);
		float fCurrentMicroseconds = static_cast<float>(rDetails.iTickExecutionNanoseconds) / static_cast<float>(rDetails.iExecutedCoordTicks) / 1'000.0f;
		if (rNormalTickNanoseconds.miCount > 0)
		{
			LOG(kNetwork, kWarning, "    RunFrameTick wall time per coord-tick: {}us, prior normal {}us", common::Wb(fCurrentMicroseconds, 3), common::Wb(static_cast<float>(rNormalTickNanoseconds.Average()) / 1'000.0f, 3));
		}
		else
		{
			LOG(kNetwork, kWarning, "    RunFrameTick wall time per coord-tick: {}us", common::Wb(fCurrentMicroseconds, 3));
		}
	}
	if (rDetails.iRecalculatedCoordTicks > 0)
	{
		LOG(kNetwork, kWarning, "    Recalculated coord-ticks: {}", rDetails.iRecalculatedCoordTicks);
	}
	if (rDetails.iStatusChangeReplayTicks > 0)
	{
		LOG(kNetwork, kWarning, "    Status-change replay ticks: {}", rDetails.iStatusChangeReplayTicks);
	}
	if (rDetails.iKnockOnReplayTicks > 0)
	{
		LOG(kNetwork, kWarning, "    Knock-on replay ticks: {}", rDetails.iKnockOnReplayTicks);
	}
	if (rDetails.bDesync)
	{
		LOG(kNetwork, kWarning, "    Desync");
	}

	if (rDetails.iExecutedCoordTicks > 0)
	{
		using Phase = FrameTickPhaseTimings::Phase;
		const std::array<int64_t, Phase::kCount>& rPhaseNanoseconds = rDetails.phaseTimings.nanoseconds;
		struct PhaseTiming
		{
			Phase ePhase = Phase::kCellSetup;
			std::string_view name;
			int64_t iNanoseconds = 0;
		};
		PhaseTiming pPhases[]
		{
			{.ePhase = Phase::kCellSetup, .name = "Cell setup", .iNanoseconds = rPhaseNanoseconds[Phase::kCellSetup]},
			{.ePhase = Phase::kInterpolateCopyUpdate, .name = "Interpolate copy/update", .iNanoseconds = rPhaseNanoseconds[Phase::kInterpolateCopyUpdate]},
			{.ePhase = Phase::kPostRenderCopyUpdate, .name = "PostRender copy/update", .iNanoseconds = rPhaseNanoseconds[Phase::kPostRenderCopyUpdate]},
			{.ePhase = Phase::kCollisionAreaDamage, .name = "Collision and area damage", .iNanoseconds = rPhaseNanoseconds[Phase::kCollisionAreaDamage]},
			{.ePhase = Phase::kTransferDestroySpawn, .name = "Transfer/destroy/spawn", .iNanoseconds = rPhaseNanoseconds[Phase::kTransferDestroySpawn]},
			{.ePhase = Phase::kSharedCrc, .name = "Shared CRC", .iNanoseconds = rPhaseNanoseconds[Phase::kSharedCrc]},
		};
		std::ranges::sort(pPhases, std::ranges::greater {}, &PhaseTiming::iNanoseconds);
		for (const PhaseTiming& rPhase : pPhases)
		{
			if (rPhase.iNanoseconds <= 0)
			{
				break;
			}
			if (&rPhase != pPhases && !IsRelevantTiming(rPhase.iNanoseconds, rDetails.iTickExecutionNanoseconds))
			{
				break;
			}
			LOG(kNetwork, kWarning, "      {} summed wall time: {}ms", rPhase.name, common::Wb(Milliseconds(rPhase.iNanoseconds), 3));
			if (rPhase.ePhase == Phase::kCellSetup)
			{
				int64_t iElevationGridNanoseconds = rPhaseNanoseconds[Phase::kElevationGridBuild];
				if (IsRelevantTiming(iElevationGridNanoseconds, rPhase.iNanoseconds))
				{
					LOG(kNetwork, kWarning, "        Elevation grid build summed wall time: {}ms", common::Wb(Milliseconds(iElevationGridNanoseconds), 3));
				}
				int64_t iRenderPlacementCacheNanoseconds = rPhaseNanoseconds[Phase::kRenderPlacementCacheBuild];
				if (IsRelevantTiming(iRenderPlacementCacheNanoseconds, rPhase.iNanoseconds))
				{
					LOG(kNetwork, kWarning, "        Render placement cache build summed wall time: {}ms", common::Wb(Milliseconds(iRenderPlacementCacheNanoseconds), 3));
				}
			}
		}
	}

	if (rDetails.iDispatcherNanoseconds > 0)
	{
		int64_t iRemainingNanoseconds = elapsed.count() - rDetails.iDispatcherNanoseconds;
		if (IsRelevantTiming(iRemainingNanoseconds, elapsed.count()))
		{
			LOG(kNetwork, kWarning, "  Remaining reconciliation wall time: {}ms", common::Wb(Milliseconds(iRemainingNanoseconds), 3));
		}
	}
}

ScopedSlowOperation::ScopedSlowOperation(common::LogCategory eCategory, const char* pcOperation, const ReconciliationDiagnostics* pReconciliationDiagnostics, const AcquisitionDiagnostics* pAcquisitionDiagnostics)
{
	if constexpr (kbProfilingFrameSpike)
	{
		meCategory = eCategory;
		mpcOperation = pcOperation;
		mpReconciliationDiagnostics = pReconciliationDiagnostics;
		mpAcquisitionDiagnostics = pAcquisitionDiagnostics;
		mExpectedRenderedFrameInterval = gpProfileManager->mExpectedRenderedFrameInterval;
		if (mExpectedRenderedFrameInterval != 0ns)
		{
			mStartTime = std::chrono::steady_clock::now();
		}
	}
}

ScopedSlowOperation::~ScopedSlowOperation()
{
	if constexpr (kbProfilingFrameSpike)
	{
		if (mExpectedRenderedFrameInterval == 0ns)
		{
			return;
		}
		if (gpProfileManager->mExpectedRenderedFrameInterval == 0ns)
		{
			return;
		}

		std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - mStartTime;
		static constexpr double kfSlowOperationFrameMultiplier = 1.9;
		std::chrono::nanoseconds threshold(static_cast<int64_t>(kfSlowOperationFrameMultiplier * static_cast<double>(mExpectedRenderedFrameInterval.count())));
		if (elapsed > threshold)
		{
			// The category is selected at runtime, so apply both LOG filters before formatting.
			if constexpr (kbLogging)
			{
				if (kWarning >= kpeLogLevels[static_cast<int64_t>(meCategory)] && kWarning >= common::gpeLogRuntimeLevels[static_cast<int64_t>(meCategory)].load(std::memory_order_relaxed))
				{
					float fElapsedMilliseconds = std::chrono::duration<float, std::milli>(elapsed).count();
					float fExpectedMilliseconds = std::chrono::duration<float, std::milli>(mExpectedRenderedFrameInterval).count();
					common::Log(meCategory, "{} took {}ms > {}ms", mpcOperation, common::Wb(fElapsedMilliseconds, 1), common::Wb(fExpectedMilliseconds, 1));
					if (mpAcquisitionDiagnostics != nullptr)
					{
						const AcquisitionDiagnostics& rDetails = *mpAcquisitionDiagnostics;
						common::Log(meCategory, "  Acquisition invocation={} graphicsFrame={} incomingFramebuffer={} total={}ns (includes setup and other scope work)", rDetails.uiSequence, rDetails.iFrameCounter, rDetails.iIncomingFramebuffer, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
						if (!rDetails.bFenceWaitRequired)
						{
							common::Log(meCategory, "  Prior acquisition fence wait: skipped");
						}
						else if (rDetails.optionalFenceNanoseconds)
						{
							common::Log(meCategory, "  Prior acquisition fence wait: {}ns", *rDetails.optionalFenceNanoseconds);
						}
						else
						{
							common::Log(meCategory, "  Prior acquisition fence wait: unavailable (call did not return)");
						}
						if (rDetails.optionalAcquireNanoseconds)
						{
							common::Log(meCategory, "  vkAcquireNextImageKHR: {}ns VkResult={}", *rDetails.optionalAcquireNanoseconds, static_cast<int32_t>(*rDetails.optionalResult));
						}
						else
						{
							common::Log(meCategory, "  vkAcquireNextImageKHR: unavailable (call did not return)");
						}
						if (rDetails.optionalImageIndex)
						{
							common::Log(meCategory, "  Returned image index: {}", *rDetails.optionalImageIndex);
						}
						else
						{
							common::Log(meCategory, "  Returned image index: unavailable");
						}
					}
					if (meCategory == kGraphics)
					{
						gpProfileManager->LogLatestGpuSample();
					}
					if (mpReconciliationDiagnostics != nullptr)
					{
						LogReconciliationDiagnostics(*mpReconciliationDiagnostics, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed), gpProfileManager->mNormalReconciliationTickNanoseconds);
					}
				}
			}
		}
		if (mpReconciliationDiagnostics != nullptr)
		{
			const ReconciliationDiagnostics& rDetails = *mpReconciliationDiagnostics;
			if (rDetails.iExecutedCoordTicks > 0 && rDetails.iRecalculatedCoordTicks == 0 && !rDetails.bDesync)
			{
				gpProfileManager->mNormalReconciliationTickNanoseconds = static_cast<double>(rDetails.iTickExecutionNanoseconds) / static_cast<double>(rDetails.iExecutedCoordTicks);
			}
		}
	}
}
#endif // BT_CLIENT

} // namespace engine
