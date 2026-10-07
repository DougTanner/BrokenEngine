#include "Pch.h"

#include "File/PackChunks.h"
#include "Graphics/EngineCamera.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "ProfileManagerBase.h"

#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

// The caller owns the workbuffer frame's Push/Pop.
void FormatCpuTimersText(common::Workbuffer& rWorkbuffer, bool bReevaluate)
{
	// Submit-worker threads write timer fields under mCpuTimerMutex, so formatting takes the same lock. Callers must release the mutex before formatting; SmoothCpuTimers releases it before this call.
	std::lock_guard lock(gpProfileManager->mCpuTimerMutex);

	int64_t iCpuTimerCount = gpProfileManager->miCpuTimerCount;

	rWorkbuffer.Append("\n\n");

	for (int64_t i = 0; i < iCpuTimerCount; ++i)
	{
		CpuTimer& rCpuTimer = gpProfileManager->GetCpuTimer(i);

		std::chrono::microseconds elapsedMicroseconds(rCpuTimer.smoothedMicroseconds.mSmoothedValue);
		if (bReevaluate)
		{
			rCpuTimer.flags.Set(ProfileRowFlags::kVisible, elapsedMicroseconds != std::chrono::microseconds::zero());
		}

		if (!(rCpuTimer.flags & ProfileRowFlags::kVisible))
		{
			continue;
		}

		rWorkbuffer.Append(gpProfileManager->GetCpuTimerName(i));
		rWorkbuffer.Append(": ");
		rWorkbuffer.Append(elapsedMicroseconds.count());
		rWorkbuffer.Append(" us");
		if (rCpuTimer.iThreads > 1)
		{
			rWorkbuffer.Append(" (");
			rWorkbuffer.Append(rCpuTimer.iThreads);
			rWorkbuffer.Append(")");
		}
		if (rCpuTimer.smoothedAllocations.mSmoothedValue > 0)
		{
			rWorkbuffer.Append(" [");
			rWorkbuffer.Append(rCpuTimer.smoothedAllocations.mSmoothedValue);
			rWorkbuffer.Append("]");
		}
		rWorkbuffer.Append("\n");

		if (i == kCpuTimerAcquireToGlobal)
		{
			rWorkbuffer.Append("\n");
		}
	}
}

// The caller owns the workbuffer frame's Push/Pop.
void FormatCpuCountersText(common::Workbuffer& rWorkbuffer, bool bReevaluate)
{
	int64_t iCpuCounterCount = gpProfileManager->miCpuCounterCount;

	for (int64_t i = 0; i < iCpuCounterCount; ++i)
	{
		CpuCounter& rCpuCounter = gpProfileManager->GetCpuCounter(i);
		if (bReevaluate)
		{
			rCpuCounter.flags.Set(ProfileRowFlags::kVisible, rCpuCounter.iCount != 0);
		}

		if (!(rCpuCounter.flags & ProfileRowFlags::kVisible))
		{
			continue;
		}

		rWorkbuffer.Append(gpProfileManager->GetCpuCounterName(i));
		rWorkbuffer.Append(": ");
		rWorkbuffer.Append(rCpuCounter.iCount);
		rWorkbuffer.Append("\n");
	}
}

#if defined(BT_CLIENT)
static void AppendMemoryStatistics(common::Workbuffer& rWorkbuffer, bool bEager)
{
	for (int64_t i = 0; i < data::kDataTypeCount; ++i)
	{
		if (IsEagerChunk(static_cast<data::DataTypes>(i)) == bEager)
		{
			MemoryStats statistics = gpFileManager->mpPackChunks->GetMemoryStatistics(static_cast<data::DataTypes>(i));
			rWorkbuffer.Append("  ");
			rWorkbuffer.Append(data::kpcDataTypeNames[i]);
			rWorkbuffer.Append(": ");
			rWorkbuffer.AppendFloat(static_cast<float>(statistics.iBytes) / (1'024.0f * 1'024.0f), 1);
			rWorkbuffer.Append(" MB (");
			rWorkbuffer.Append(statistics.iCount);
			rWorkbuffer.Append(")\n");
		}
	}
}

static void FormatGpuGraphicsInfo(common::Workbuffer& rWorkbuffer)
{
	auto [iX, iY] = FullDetail();
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.Append(static_cast<int64_t>(gpGraphics->mFramebufferVkExtent2D.width));
	rWorkbuffer.Append(" x ");
	rWorkbuffer.Append(static_cast<int64_t>(gpGraphics->mFramebufferVkExtent2D.height));
	rWorkbuffer.Append("\n");
	rWorkbuffer.Append(iX);
	rWorkbuffer.Append(" x ");
	rWorkbuffer.Append(iY);
	rWorkbuffer.Append("\n");
	rWorkbuffer.Append(gpGraphics->miMonitorRefreshRate);
	rWorkbuffer.Append(" Hz\n");
	rWorkbuffer.Append(gMultisampling.Get<bool>() ? "On" : "Off");
	rWorkbuffer.Append(" - ");
	rWorkbuffer.Append(gPresentMode.Get<VkPresentModeKHR>() == VK_PRESENT_MODE_FIFO_KHR ? "Fifo" : (gPresentMode.Get<VkPresentModeKHR>() == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "Immediate"));
	gpImGuiManager->UpdateTextArea(kTextGraphics, rWorkbuffer.View());
}

static void FormatGpuTimerRows(common::Workbuffer& rWorkbuffer, bool bReevaluate)
{
	GpuTimer* pGpuTimers = gpProfileManager->mGpuTimers;

	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.Append("\n\n");

	// Active visible-area LOD vertex grid (quads + 1), shared by the water displacement compute pre-pass
	// (writes the top-left rectangle) and the water mesh draw. Mirrors the LOD pick in RenderFrameMain.
	int64_t iWaterLevelOfDetail = std::clamp<int64_t>(engine::gpCamera->miVisibleAreaLevelOfDetail, 0, BufferManager::kiVisibleAreaLodCount - 1);
	const BufferManager::VisibleAreaMeshLod& rWaterLevelOfDetail = gpBufferManager->mWaterMeshLods[iWaterLevelOfDetail];
	int64_t iWaterGridX = rWaterLevelOfDetail.iQuadCountX + 1;
	int64_t iWaterGridY = rWaterLevelOfDetail.iQuadCountY + 1;

	for (int64_t i = 0; i < kGpuTimerCount; ++i)
	{
		std::chrono::microseconds elapsedMicroseconds(pGpuTimers[i].smoothedMicroseconds.mSmoothedValue);
		std::chrono::microseconds maximumMicroseconds(pGpuTimers[i].smoothedMicroseconds.Maximum());
		if (bReevaluate)
		{
			pGpuTimers[i].flags.Set(ProfileRowFlags::kVisible, !(elapsedMicroseconds < std::chrono::microseconds(10) || (elapsedMicroseconds < std::chrono::microseconds(200) && !(maximumMicroseconds > 2 * elapsedMicroseconds))));
		}

		if (!(pGpuTimers[i].flags & ProfileRowFlags::kVisible))
		{
			continue;
		}

		rWorkbuffer.Append(kGpuTimerNames[i]);
		rWorkbuffer.Append(": ");
		rWorkbuffer.Append(elapsedMicroseconds.count());
		rWorkbuffer.Append(" us");
		if (maximumMicroseconds > 2 * elapsedMicroseconds)
		{
			rWorkbuffer.Append(" (");
			rWorkbuffer.Append(maximumMicroseconds.count());
			rWorkbuffer.Append(")");
		}

		// Terrain Elevation's render target follows the snap grid.
		if (i == kGpuTimerTerrainElevation)
		{
			rWorkbuffer.Append("  ");
			rWorkbuffer.Append(static_cast<int64_t>(gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture.mInfo.vkExtent3D.width));
			rWorkbuffer.Append("x");
			rWorkbuffer.Append(static_cast<int64_t>(gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture.mInfo.vkExtent3D.height));
		}
		else if (i == kGpuTimerWaterDisplacement || i == kGpuTimerWater)
		{
			rWorkbuffer.Append("  ");
			rWorkbuffer.Append(iWaterGridX);
			rWorkbuffer.Append("x");
			rWorkbuffer.Append(iWaterGridY);
		}

		rWorkbuffer.Append("\n");
	}

	gpImGuiManager->UpdateTextArea(kTextProfileGpuTimers, rWorkbuffer.View());
}

static void FormatCellReadout(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("Cell: [");
	rWorkbuffer.Append(static_cast<int64_t>(game::gpGame->mClientGridCoordinate.iX));
	rWorkbuffer.Append(",");
	rWorkbuffer.Append(static_cast<int64_t>(game::gpGame->mClientGridCoordinate.iY));
	rWorkbuffer.Append("]\nActive: ");
	rWorkbuffer.Append(std::ssize(game::gpGame->mActiveCoordinates));
	rWorkbuffer.Append("\n");

	auto it = game::gpGame->mCells.find(game::gpGame->mClientGridCoordinate);
	if (it != game::gpGame->mCells.end() && it->second.iSnapshotCount > 0)
	{
		const game::Frame& rRenderFrame = game::gpGame->RenderFrame(game::gpGame->mClientGridCoordinate);
		rWorkbuffer.Append("Tick: ");
		rWorkbuffer.Append(rRenderFrame.interpolate.iTick);
		rWorkbuffer.Append("\nTime: ");
		rWorkbuffer.AppendFloat(rRenderFrame.interpolate.fCurrentTime, 1);
		rWorkbuffer.Append("s\n");
	}
	rWorkbuffer.Append("\n");
}

static void FormatGpuMemoryStatistics(common::Workbuffer& rWorkbuffer)
{
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	FormatCellReadout(rWorkbuffer);
	rWorkbuffer.Append("GPU Memory\n");

	VmaTotalStatistics statistics {};
	vmaCalculateStatistics(gpDeviceManager->mpAllocator, &statistics);

	rWorkbuffer.Append("Allocated: ");
	rWorkbuffer.AppendFloat(static_cast<float>(statistics.total.statistics.blockBytes) / (1'024.0f * 1'024.0f), 1);
	rWorkbuffer.Append(" MB\nUsed: ");
	rWorkbuffer.AppendFloat(static_cast<float>(statistics.total.statistics.allocationBytes) / (1'024.0f * 1'024.0f), 1);
	rWorkbuffer.Append(" MB\nUnused: ");
	rWorkbuffer.AppendFloat(static_cast<float>(statistics.total.statistics.blockBytes - statistics.total.statistics.allocationBytes) / (1'024.0f * 1'024.0f), 1);
	rWorkbuffer.Append(" MB\nAllocations: ");
	rWorkbuffer.Append(static_cast<int64_t>(statistics.total.statistics.allocationCount));
	rWorkbuffer.Append("  Blocks: ");
	rWorkbuffer.Append(static_cast<int64_t>(statistics.total.statistics.blockCount));

	if (gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kMemoryBudgetAvailable)
	{
		int64_t iHeapCount = gpInstanceManager->mVkPhysicalDeviceMemoryProperties.memoryHeapCount;
		VmaBudget budgets[VK_MAX_MEMORY_HEAPS] {};
		vmaGetHeapBudgets(gpDeviceManager->mpAllocator, budgets);

		for (int64_t i = 0; i < iHeapCount; ++i)
		{
			VkMemoryHeapFlags vkMemoryHeapFlags = gpInstanceManager->mVkPhysicalDeviceMemoryProperties.memoryHeaps[i].flags;
			bool bDeviceLocal = (vkMemoryHeapFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;

			rWorkbuffer.Append("\nHeap ");
			rWorkbuffer.Append(static_cast<int64_t>(i));
			rWorkbuffer.Append(bDeviceLocal ? " (Device Local)\n" : " (Host)\n");

			rWorkbuffer.Append("  Budget: ");
			rWorkbuffer.AppendFloat(static_cast<float>(budgets[i].budget) / (1'024.0f * 1'024.0f), 1);
			rWorkbuffer.Append(" MB  Usage: ");
			rWorkbuffer.AppendFloat(static_cast<float>(budgets[i].usage) / (1'024.0f * 1'024.0f), 1);
			rWorkbuffer.Append(" MB");

			if (budgets[i].budget > 0)
			{
				float fPercent = static_cast<float>(static_cast<double>(budgets[i].usage) / static_cast<double>(budgets[i].budget)) * 100.0f;
				rWorkbuffer.Append(" (");
				rWorkbuffer.AppendFloat(fPercent, 1);
				rWorkbuffer.Append("%)");
			}
		}
	}

	gpImGuiManager->UpdateTextArea(kTextProfileMemory, rWorkbuffer.View());
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)

void FormatFramesPerSecondHeader(common::Workbuffer& rWorkbuffer, std::chrono::microseconds elapsedCpuTime)
{
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.Append(static_cast<int64_t>(gpGraphics->mRendersInTheLastSecond.Get()));
	rWorkbuffer.Append(" fps");

	if (elapsedCpuTime > std::chrono::microseconds(100))
	{
		rWorkbuffer.Append(" (Cpu: ");
		rWorkbuffer.Append(1'000'000 / elapsedCpuTime.count());
		rWorkbuffer.Append(" fps, ");
	}
	else
	{
		rWorkbuffer.Append(" (Cpu: >9000 fps, ");
	}

	GpuTimer* pGpuTimers = gpProfileManager->mGpuTimers;
	std::chrono::microseconds elapsedGpuTime(pGpuTimers[kGpuTimerGlobal].smoothedMicroseconds.mSmoothedValue + pGpuTimers[kGpuTimerMain].smoothedMicroseconds.mSmoothedValue + pGpuTimers[kGpuTimerImage].smoothedMicroseconds.mSmoothedValue);
	if (elapsedGpuTime > std::chrono::microseconds::zero())
	{
		rWorkbuffer.Append("Gpu: ");
		rWorkbuffer.Append(1'000'000 / elapsedGpuTime.count());
		rWorkbuffer.Append(" fps)");
	}

	rWorkbuffer.Append("   Frame updates: ");
	rWorkbuffer.Append(static_cast<int64_t>(gpProfileManager->mFullUpdatesInTheLastSecond.Get()));
	rWorkbuffer.Append(" full ");
	rWorkbuffer.Append(static_cast<int64_t>(gpProfileManager->mInterpolateUpdatesInTheLastSecond.Get()));
	rWorkbuffer.Append(" interpolate");
	rWorkbuffer.Append("   Camera height: ");
	rWorkbuffer.Append(static_cast<int64_t>(engine::gpCamera->mfCameraEyeHeight));
	rWorkbuffer.Append(" m");
	gpImGuiManager->UpdateTextArea(kTextProfileFps, rWorkbuffer.View());
}

void FormatCpuScreen(common::Workbuffer& rWorkbuffer, bool bReevaluate)
{
	{
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		FormatCpuTimersText(rWorkbuffer, bReevaluate);
		gpImGuiManager->UpdateTextArea(kTextProfileCpuTimers, rWorkbuffer.View());
	}

	{
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		FormatCpuCountersText(rWorkbuffer, bReevaluate);
		gpImGuiManager->UpdateTextArea(kTextProfileCpuCounters, rWorkbuffer.View());
	}

	MemoryStats eagerStatistics = gpFileManager->mpPackChunks->GetEagerStatistics();
	MemoryStats lazyStatistics = gpFileManager->mpPackChunks->GetLazyStatistics();
	int64_t iTotalBytes = eagerStatistics.iBytes + lazyStatistics.iBytes;
	int64_t iTotalCount = eagerStatistics.iCount + lazyStatistics.iCount;

	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	FormatCellReadout(rWorkbuffer);
	rWorkbuffer.Append("Data Memory\n");
	rWorkbuffer.Append("Eager: ");
	rWorkbuffer.AppendFloat(static_cast<float>(eagerStatistics.iBytes) / (1'024.0f * 1'024.0f), 1);
	rWorkbuffer.Append(" MB (");
	rWorkbuffer.Append(eagerStatistics.iCount);
	rWorkbuffer.Append(")\n");
	AppendMemoryStatistics(rWorkbuffer, true);
	rWorkbuffer.Append("Lazy: ");
	rWorkbuffer.AppendFloat(static_cast<float>(lazyStatistics.iBytes) / (1'024.0f * 1'024.0f), 1);
	rWorkbuffer.Append(" MB (");
	rWorkbuffer.Append(lazyStatistics.iCount);
	rWorkbuffer.Append(")\n");
	AppendMemoryStatistics(rWorkbuffer, false);
	rWorkbuffer.Append("Total: ");
	rWorkbuffer.AppendFloat(static_cast<float>(iTotalBytes) / (1'024.0f * 1'024.0f), 1);
	rWorkbuffer.Append(" MB (");
	rWorkbuffer.Append(iTotalCount);
	rWorkbuffer.Append(")");
	rWorkbuffer.Append("\nAllocations: ");
	rWorkbuffer.Append(gpProfileManager->mSmoothedAllocations.mSmoothedValue);
	gpImGuiManager->UpdateTextArea(kTextProfileMemory, rWorkbuffer.View());
}

void FormatGpuScreen(common::Workbuffer& rWorkbuffer, bool bReevaluate)
{
	FormatGpuGraphicsInfo(rWorkbuffer);
	FormatGpuTimerRows(rWorkbuffer, bReevaluate);
	FormatGpuMemoryStatistics(rWorkbuffer);
}

#endif // BT_CLIENT

} // namespace engine
