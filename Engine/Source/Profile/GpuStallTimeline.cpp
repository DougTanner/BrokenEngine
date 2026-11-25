#include "Pch.h"

#if defined(BT_CLIENT)

#include "ProfileManager.h"

namespace engine
{

constexpr GpuTimers kpeGpuTimelineRoots[] {kGpuTimerGlobal, kGpuTimerMain, kGpuTimerImage, kGpuTimerUiRender};
enum GpuTimelineTick : size_t
{
	kTimelineGlobalStart,
	kTimelineGlobalEnd,
	kTimelineMainStart,
	kTimelineMainEnd,
	kTimelineImageStart,
	kTimelineImageEnd,
	kTimelineUiRenderStart,
	kTimelineUiRenderEnd,
};

void GpuStallTimeline::Clear()
{
	mGpuTimeline.fill({});
	miPendingStallSequence = 0;
}

void GpuStallTimeline::RecordTicks(int64_t iSequence, GpuTimers eGpuTimer, uint64_t uiStartTick, uint64_t uiEndTick)
{
	GpuFrameTimeline& rTimeline = mGpuTimeline[static_cast<size_t>(iSequence % kiGpuTimelineCapacity)];
	const GpuTimers* peRoot = std::ranges::find(kpeGpuTimelineRoots, eGpuTimer);
	if (rTimeline.iSequence == iSequence && peRoot != std::end(kpeGpuTimelineRoots))
	{
		int64_t iTick = 2 * (peRoot - std::begin(kpeGpuTimelineRoots));
		rTimeline.rootTicks[static_cast<size_t>(iTick)] = uiStartTick;
		rTimeline.rootTicks[static_cast<size_t>(iTick + 1)] = uiEndTick;
	}
}

void GpuStallTimeline::FenceObserved(int64_t iSequence)
{
	GpuFrameTimeline& rTimeline = mGpuTimeline[static_cast<size_t>(iSequence % kiGpuTimelineCapacity)];
	if (rTimeline.iSequence != iSequence)
	{
		return;
	}
	if (!rTimeline.optionalFenceObservedTime)
	{
		rTimeline.optionalFenceObservedTime = std::chrono::steady_clock::now();
	}
}

void GpuStallTimeline::ImageWait(int64_t iNewestSequence, std::chrono::steady_clock::time_point waitStart, std::chrono::steady_clock::time_point waitEnd)
{
	GpuFrameTimeline& rTimeline = mGpuTimeline[static_cast<size_t>(iNewestSequence % kiGpuTimelineCapacity)];
	if (rTimeline.iSequence != iNewestSequence)
	{
		return;
	}
	if (rTimeline.optionalImageWaitStart)
	{
		return;
	}
	rTimeline.optionalImageWaitStart = waitStart;
	rTimeline.optionalImageWaitEnd = waitEnd;
}

void GpuStallTimeline::LogGraphicsStall(const char* pcOperation, const AcquisitionDiagnostics* pAcquisitionDiagnostics, std::chrono::steady_clock::time_point stallStart, int64_t iNewestSequence, const GpuDiagnosticSample& rLatestSample)
{
	// A pending dump at least kiGpuTimelineCapacity submissions old has lost its ring entries, so the new stall replaces it.
	// Destroy clears the ring without resetting the sequence, so the newest submission's slot no longer matches until the next GpuSubmitted.
	bool bSetPending = mGpuTimeline[static_cast<size_t>(iNewestSequence % kiGpuTimelineCapacity)].iSequence == iNewestSequence && (miPendingStallSequence == 0 || iNewestSequence - miPendingStallSequence >= kiGpuTimelineCapacity);
	if (bSetPending)
	{
		miPendingStallSequence = iNewestSequence;
		mpcPendingStallOperation = pcOperation;
		mPendingStallStart = stallStart;
	}

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena summary = rWorkbuffer.Push();
	rWorkbuffer.Append("  ");
	if (pAcquisitionDiagnostics != nullptr)
	{
		const AcquisitionDiagnostics& rDetails = *pAcquisitionDiagnostics;
		rWorkbuffer.Append("Acquisition ");
		rWorkbuffer.Append(static_cast<int64_t>(rDetails.uiSequence));
		rWorkbuffer.Append(" frame=");
		rWorkbuffer.Append(rDetails.iFrameCounter);
		rWorkbuffer.Append(" fb=");
		rWorkbuffer.Append(rDetails.iIncomingFramebuffer);
		rWorkbuffer.Append(" fence=");
		if (!rDetails.bFenceWaitRequired)
		{
			rWorkbuffer.Append("skipped");
		}
		else if (rDetails.optionalFenceNanoseconds)
		{
			rWorkbuffer.AppendFloat(static_cast<float>(*rDetails.optionalFenceNanoseconds) / 1'000'000.0f, 3);
			rWorkbuffer.Append("ms");
		}
		else
		{
			rWorkbuffer.Append("-");
		}
		rWorkbuffer.Append(" acquire=");
		if (rDetails.optionalAcquireCall)
		{
			rWorkbuffer.AppendFloat(static_cast<float>(rDetails.optionalAcquireCall->iNanoseconds) / 1'000'000.0f, 3);
			rWorkbuffer.Append("ms result=");
			rWorkbuffer.Append(static_cast<int64_t>(rDetails.optionalAcquireCall->vkResult));
		}
		else
		{
			rWorkbuffer.Append("- result=-");
		}
		rWorkbuffer.Append(" index=");
		if (rDetails.optionalImageIndex)
		{
			rWorkbuffer.Append(static_cast<int64_t>(*rDetails.optionalImageIndex));
		}
		else
		{
			rWorkbuffer.Append("-");
		}
		rWorkbuffer.Append(" | ");
	}
	if (rLatestSample.submission.iSequence == 0)
	{
		rWorkbuffer.Append("newest completed unavailable");
	}
	else
	{
		rWorkbuffer.Append("newest completed ");
		rWorkbuffer.Append(rLatestSample.submission.iSequence);
		rWorkbuffer.Append(" fb=");
		rWorkbuffer.Append(rLatestSample.iFramebuffer);
		rWorkbuffer.Append(rLatestSample.submission.bWorld ? " world GPU=" : " UI GPU=");
		std::optional<int64_t> optionalRootMicroseconds = rLatestSample.RootMicroseconds();
		if (optionalRootMicroseconds)
		{
			rWorkbuffer.AppendFloat(static_cast<float>(*optionalRootMicroseconds) / 1'000.0f, 1);
			rWorkbuffer.Append("ms");
		}
		else
		{
			rWorkbuffer.Append("-");
		}
		rWorkbuffer.Append(" age=");
		rWorkbuffer.AppendFloat(std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - rLatestSample.submission.submitTime).count(), 1);
		rWorkbuffer.Append("ms");
	}
	if (bSetPending)
	{
		rWorkbuffer.Append(" | timeline after ");
		rWorkbuffer.Append(miPendingStallSequence);
		rWorkbuffer.Append(" retires");
	}
	else if (miPendingStallSequence != 0)
	{
		rWorkbuffer.Append(" | timeline pending ");
		rWorkbuffer.Append(miPendingStallSequence);
	}
	LOG(kGraphics, kWarning, "{}", summary);
}

void GpuStallTimeline::Retired(const GpuDiagnosticSample& rSample, bool bAnomaly)
{
	if (rSample.submission.iSequence == miPendingStallSequence)
	{
		// Skipped after an anomaly, which has just printed the same breakdown. A zero interval is a submission stamped before the rendered-interval warmup completes, at launch or after a focus change.
		bool bExpectedInterval = rSample.submission.expectedInterval != 0ns;
		std::optional<int64_t> optionalRootMicroseconds = rSample.RootMicroseconds();
		if (!bAnomaly && bExpectedInterval && optionalRootMicroseconds && std::chrono::microseconds(*optionalRootMicroseconds) > rSample.submission.expectedInterval)
		{
			rSample.Log("Stalled submission");
		}

		// One calibration pairs device tick uiCalibrationTick with steady_clock time calibrationTime through the QPC domain.
		bool bCalibrated = gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kCalibratedTimestampsEnabled;
		uint64_t uiCalibrationTick = 0;
		std::chrono::steady_clock::time_point calibrationTime {};
		if (bCalibrated)
		{
			VkCalibratedTimestampInfoKHR pVkCalibratedTimestampInfos[2]
			{
				{.sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, .pNext = nullptr, .timeDomain = VK_TIME_DOMAIN_DEVICE_KHR},
				{.sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, .pNext = nullptr, .timeDomain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR},
			};
			uint64_t puiTimestamps[2] {};
			uint64_t uiMaxDeviation = 0;
			CHECK_VK(gpDeviceManager->mpVkGetCalibratedTimestamps(gpDeviceManager->mVkDevice, 2, pVkCalibratedTimestampInfos, puiTimestamps, &uiMaxDeviation));
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			LARGE_INTEGER performanceCounter {};
			QueryPerformanceCounter(&performanceCounter);
			LARGE_INTEGER performanceFrequency {};
			QueryPerformanceFrequency(&performanceFrequency);
			uiCalibrationTick = puiTimestamps[0];
			calibrationTime = now - std::chrono::nanoseconds((performanceCounter.QuadPart - static_cast<int64_t>(puiTimestamps[1])) * 1'000'000'000 / performanceFrequency.QuadPart);
			LOG(kGraphics, kWarning, "  Stall timeline ({}, {}): maxDeviation={}ns", mpcPendingStallOperation, miPendingStallSequence, uiMaxDeviation);
		}
		else
		{
			LOG(kGraphics, kWarning, "  Stall timeline ({}, {}): calibration unavailable", mpcPendingStallOperation, miPendingStallSequence);
		}

		int64_t iTimestampValidBits = gpInstanceManager->mVkQueueFamilyProperties.at(gpInstanceManager->miGraphicsQueueFamilyIndex).timestampValidBits;
		double fTimestampPeriod = static_cast<double>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.timestampPeriod);
		// GPU-clock milliseconds from uiFrom to uiTo; the tick difference wraps at timestampValidBits and is sign-extended.
		auto TickMilliseconds = [&](uint64_t uiFrom, uint64_t uiTo) -> double
		{
			uint64_t uiDelta = uiTo - uiFrom;
			if (iTimestampValidBits < 64)
			{
				uint64_t uiSignBit = 1ui64 << (iTimestampValidBits - 1);
				uiDelta = ((uiDelta & ((uiSignBit << 1) - 1)) ^ uiSignBit) - uiSignBit;
			}
			return static_cast<double>(static_cast<int64_t>(uiDelta)) * fTimestampPeriod / 1'000'000.0;
		};
		auto CpuMilliseconds = [](std::chrono::steady_clock::time_point from, std::chrono::steady_clock::time_point to) -> double
		{
			return std::chrono::duration<double, std::milli>(to - from).count();
		};
		// CPU time of a device tick relative to the stall start.
		auto TickStallMilliseconds = [&](uint64_t uiTick) -> double
		{
			return CpuMilliseconds(mPendingStallStart, calibrationTime) + TickMilliseconds(uiCalibrationTick, uiTick);
		};

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		auto AppendColumn = [&](std::string_view name, std::optional<double> optionalMilliseconds)
		{
			rWorkbuffer.Append(name);
			if (optionalMilliseconds)
			{
				rWorkbuffer.AppendFloat(static_cast<float>(*optionalMilliseconds), 1);
			}
			else
			{
				rWorkbuffer.Append("-");
			}
		};

		double fQuarterIntervalMilliseconds = 0.25 * std::chrono::duration<double, std::milli>(rSample.submission.expectedInterval).count();
		int64_t iHintSequence = 0;
		double fHintIdleMilliseconds = 0.0;
		const GpuFrameTimeline* pPrevious = nullptr;
		for (int64_t i = std::max(1i64, miPendingStallSequence - kiGpuTimelineCapacity + 1); i <= miPendingStallSequence; ++i)
		{
			const GpuFrameTimeline& rRow = mGpuTimeline[static_cast<size_t>(i % kiGpuTimelineCapacity)];
			if (rRow.iSequence != i)
			{
				pPrevious = nullptr;
				continue;
			}
			const std::array<std::optional<uint64_t>, 8>& rTicks = rRow.rootTicks;
			std::optional<uint64_t> optionalStartTick = rTicks[rRow.bWorld ? kTimelineGlobalStart : kTimelineUiRenderStart];
			std::optional<uint64_t> optionalEndTick = rTicks[kTimelineUiRenderEnd];

			std::optional<double> optionalIdle;
			std::optional<double> optionalQueued;
			std::optional<double> optionalGlobalToMain;
			std::optional<double> optionalImage;
			std::optional<double> optionalSpan;
			std::optional<double> optionalLate;
			if (bCalibrated && optionalStartTick && pPrevious != nullptr && pPrevious->rootTicks[kTimelineUiRenderEnd])
			{
				optionalIdle = TickMilliseconds(*pPrevious->rootTicks[kTimelineUiRenderEnd], *optionalStartTick);
			}
			if (bCalibrated && optionalStartTick)
			{
				optionalQueued = TickStallMilliseconds(*optionalStartTick) - CpuMilliseconds(mPendingStallStart, rRow.submitTime);
			}
			const std::optional<uint64_t>& rOptionalGlobalEnd = rTicks[kTimelineGlobalEnd];
			const std::optional<uint64_t>& rOptionalMainStart = rTicks[kTimelineMainStart];
			if (bCalibrated && rOptionalGlobalEnd && rOptionalMainStart)
			{
				optionalGlobalToMain = TickMilliseconds(*rOptionalGlobalEnd, *rOptionalMainStart);
			}
			const std::optional<uint64_t>& rOptionalImageStart = rTicks[kTimelineImageStart];
			const std::optional<uint64_t>& rOptionalImageEnd = rTicks[kTimelineImageEnd];
			if (rOptionalImageStart && rOptionalImageEnd)
			{
				optionalImage = TickMilliseconds(*rOptionalImageStart, *rOptionalImageEnd);
			}
			// A world row spans the Global, Main, and UI submissions; a UI-only row lies inside one submission.
			if ((bCalibrated || !rRow.bWorld) && optionalStartTick && optionalEndTick)
			{
				optionalSpan = TickMilliseconds(*optionalStartTick, *optionalEndTick);
			}
			if (bCalibrated && optionalEndTick && rRow.optionalFenceObservedTime)
			{
				optionalLate = CpuMilliseconds(mPendingStallStart, *rRow.optionalFenceObservedTime) - TickStallMilliseconds(*optionalEndTick);
			}

			{
				common::ScopedWorkbufferArena row = rWorkbuffer.Push();
				rWorkbuffer.Append("  ");
				rWorkbuffer.Append(rRow.iSequence);
				rWorkbuffer.Append(" fb=");
				rWorkbuffer.Append(rRow.iFramebuffer);
				rWorkbuffer.Append(rRow.bWorld ? " world" : " UI");
				if (rRow.bAcquireBarriers)
				{
					rWorkbuffer.Append(" A");
				}
				AppendColumn(" submit=", CpuMilliseconds(mPendingStallStart, rRow.submitTime));
				AppendColumn(" idle=", optionalIdle);
				AppendColumn(" queued=", optionalQueued);
				AppendColumn(" g2m=", optionalGlobalToMain);
				AppendColumn(" image=", optionalImage);
				AppendColumn(" span=", optionalSpan);
				AppendColumn(" fence=", rRow.optionalFenceObservedTime ? std::optional<double>(CpuMilliseconds(mPendingStallStart, *rRow.optionalFenceObservedTime)) : std::nullopt);
				AppendColumn(" late=", optionalLate);
				if (rRow.optionalImageWaitStart)
				{
					AppendColumn(" imgwait=", CpuMilliseconds(mPendingStallStart, *rRow.optionalImageWaitStart));
					AppendColumn("..", CpuMilliseconds(mPendingStallStart, *rRow.optionalImageWaitEnd));
				}
				else
				{
					rWorkbuffer.Append(" imgwait=-");
				}
				LOG(kGraphics, kWarning, "{}", row);
			}

			// UI-only rows may start after their image wait, and the acquire command buffer runs unmeasured before the first tick.
			if (bExpectedInterval && iHintSequence == 0 && rRow.bWorld && !rRow.bAcquireBarriers && optionalIdle && optionalQueued && *optionalIdle > fQuarterIntervalMilliseconds && *optionalQueued > fQuarterIntervalMilliseconds)
			{
				iHintSequence = rRow.iSequence;
				fHintIdleMilliseconds = *optionalIdle;
			}
			pPrevious = &rRow;
		}
		if (iHintSequence != 0)
		{
			LOG(kGraphics, kWarning, "  no measured GPU region ran for {}ms before submission {} started, with its work already queued; run /analyze-gpu-stall", common::Wb(static_cast<float>(fHintIdleMilliseconds), 1), iHintSequence);
		}
		miPendingStallSequence = 0;
	}
}

} // namespace engine

#endif // BT_CLIENT
