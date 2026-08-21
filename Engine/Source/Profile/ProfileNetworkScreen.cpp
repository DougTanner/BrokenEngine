#include "Pch.h"

#if defined(BT_CLIENT)

#include "ProfileManagerBase.h"

#include "Game.h"
#include "Network/Client/ClientSession.h"

namespace engine
{

namespace
{

void AppendBytes(common::Workbuffer& rWorkbuffer, int64_t iBytes)
{
	if (iBytes >= 1024 * 1024)
	{
		rWorkbuffer.AppendFloat(static_cast<float>(iBytes) / (1024.0f * 1024.0f), 1);
		rWorkbuffer.Append(" MB/s");
	}
	else
	{
		rWorkbuffer.AppendFloat(static_cast<float>(iBytes) / 1024.0f, 1);
		rWorkbuffer.Append(" KB/s");
	}
}

} // namespace

void ProfileManagerBase::FormatNetworkScreen(common::Workbuffer& rWorkbuffer)
{
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	// Header with simulation level info
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		constexpr engine::NetworkSimulationConfig kSimConfig = engine::GetNetworkSimulationConfig(keNetworkSimulation);
		rWorkbuffer.Append("Network (Sim: ");
		if (game::gpGame->mTimeStep.miTimeMultiply > 1)
		{
			rWorkbuffer.Append("BYPASS ");
			rWorkbuffer.Append(engine::GetNetworkSimulationName(keNetworkSimulation));
			rWorkbuffer.Append(")");
		}
		else
		{
			rWorkbuffer.Append(engine::GetNetworkSimulationName(keNetworkSimulation));
			rWorkbuffer.Append(" ");
			rWorkbuffer.Append(kSimConfig.iPingMinMs);
			rWorkbuffer.Append("-");
			rWorkbuffer.Append(kSimConfig.iPingMaxMs);
			rWorkbuffer.Append("ms)");
		}
	}
	else
	{
		rWorkbuffer.Append("Network (Sim: Off)");
	}

	if (engine::gpClient == nullptr)
	{
		rWorkbuffer.Append("\n(Offline)");
	}
	else
	{
		FormatNetworkTransport(rWorkbuffer);
		FormatNetworkSync(rWorkbuffer);
		FormatNetworkPrediction(rWorkbuffer);
		FormatNetworkClock(rWorkbuffer);
		FormatNetworkReconciliation(rWorkbuffer);
	}

	engine::gpImGuiManager->UpdateTextArea(engine::kTextProfileFps, rWorkbuffer.View());
}

void ProfileManagerBase::FormatNetworkTransport(common::Workbuffer& rWorkbuffer)
{
	// -- Transport --
	rWorkbuffer.Append("\n-- Transport --\n");
	ENetPeer* pPeer = engine::gpClient->mpServerPeer;
	if (pPeer != nullptr)
	{
		FormatNetworkPeerMetrics(rWorkbuffer, *pPeer);
	}
	FormatNetworkTraffic(rWorkbuffer);
}

void ProfileManagerBase::FormatNetworkPeerMetrics(common::Workbuffer& rWorkbuffer, const ENetPeer& rPeer)
{
	[[maybe_unused]] constexpr engine::NetworkSimulationConfig kSimConfig = engine::GetNetworkSimulationConfig(keNetworkSimulation);
	int64_t iRtt = static_cast<int64_t>(rPeer.roundTripTime);
	rWorkbuffer.Append("RTT: ");
	rWorkbuffer.Append(iRtt);
	rWorkbuffer.Append(" ms");
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		if (iRtt > kSimConfig.iPingMaxMs * 3 / 2)
		{
			rWorkbuffer.Append("!");
		}
	}

	rWorkbuffer.Append("  Pipe: ");
	rWorkbuffer.AppendFloat(engine::gpClient->mSmoothedPipelineRttUs.Get() / 1000.0f, 1);
	rWorkbuffer.Append(" ms\n");

	float fLoss = rPeer.packetLoss * 100.0f / 65536.0f;
	rWorkbuffer.Append("Loss: ");
	rWorkbuffer.AppendFloat(fLoss, 1);
	rWorkbuffer.Append("%");
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		if (game::gpGame->mTimeStep.miTimeMultiply > 1)
		{
			rWorkbuffer.Append(" (sim: BYPASS)");
		}
		else
		{
			if (fLoss > kSimConfig.fPacketLossPercent * 2.0f)
			{
				rWorkbuffer.Append("!");
			}
			rWorkbuffer.Append(" (sim: ");
			rWorkbuffer.AppendFloat(kSimConfig.fPacketLossPercent, 1);
			rWorkbuffer.Append("%)");
		}
	}
	rWorkbuffer.Append("\n");

	int64_t iActiveSlotCount = 0;
	for (const engine::ClientCoordSlot& rSlot : engine::gpClient->mCoordSlots)
	{
		if (rSlot.eState == engine::CoordSubscriptionState::kActive)
		{
			++iActiveSlotCount;
		}
	}
	int64_t iExpectedFrames = engine::gpClient->mTimeState.iExpectedUpdatesPerSecond * iActiveSlotCount;
	int64_t iLostFrames = iExpectedFrames - engine::gpClient->mFramesReceived.Get();
	float fPacketLossPercent = iExpectedFrames > 0 && iLostFrames > 0 ? static_cast<float>(iLostFrames) * 100.0f / static_cast<float>(iExpectedFrames) : 0.0f;
	rWorkbuffer.Append("Pkt Loss: ");
	rWorkbuffer.AppendFloat(fPacketLossPercent, 1);
	rWorkbuffer.Append("%  Jitter: ");
	rWorkbuffer.AppendFloat(engine::gpClient->mSmoothedJitterUs.Get() / 1000.0f, 1);
	rWorkbuffer.Append(" ms\n");

	mSmoothedRtt = iRtt;
	mSmoothedRtt.Update();
	mSmoothedJitter = engine::gpClient->mSmoothedJitterUs.Get() / 1000;
	mSmoothedJitter.Update();
}

void ProfileManagerBase::FormatNetworkTraffic(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("In: ");
	AppendBytes(rWorkbuffer, engine::gpClient->mBytesInPerSecond.Get());
	rWorkbuffer.Append("  Out: ");
	AppendBytes(rWorkbuffer, engine::gpClient->mBytesOutPerSecond.Get());
}

void ProfileManagerBase::FormatNetworkSync(common::Workbuffer& rWorkbuffer)
{
	// -- Sync --
	rWorkbuffer.Append("\n-- Sync --\n");
	{
		int64_t iMinAckFloor = -1;
		int64_t iTotalRecv = 0;
		for (const engine::ClientCoordSlot& rSlot : engine::gpClient->mCoordSlots)
		{
			if (rSlot.eState == engine::CoordSubscriptionState::kActive)
			{
				if (iMinAckFloor < 0 || rSlot.ackState.iAckFloor < iMinAckFloor)
				{
					iMinAckFloor = rSlot.ackState.iAckFloor;
				}
				iTotalRecv += std::popcount(rSlot.ackState.uiReceivedBitfieldLow) + std::popcount(rSlot.ackState.uiReceivedBitfieldHigh);
			}
		}
		rWorkbuffer.Append("Ack: ");
		rWorkbuffer.Append(iMinAckFloor);
		rWorkbuffer.Append("  Conf: ");
		rWorkbuffer.Append(game::gpClientSession->mpRuntime->GetConfirmedTick());
		mSmoothedRecv = iTotalRecv;
	}
	mSmoothedRecv.Update();
	rWorkbuffer.Append("\nRecv: ");
	rWorkbuffer.Append(mSmoothedRecv.Get());
	rWorkbuffer.Append("/128");
}

void ProfileManagerBase::FormatNetworkPrediction(common::Workbuffer& rWorkbuffer)
{
	// -- Prediction --
	rWorkbuffer.Append("\n-- Prediction --\n");
	auto predCoordIt = game::gpGame->mCoordFrames.find(game::gpGame->mClientGridCoord);
	if (predCoordIt != game::gpGame->mCoordFrames.end() && predCoordIt->second.iSnapshotCount > 0)
	{
		mSmoothedRollback = game::gpGame->RenderFrame(game::gpGame->mClientGridCoord).interpolate.iTick - game::gpClientSession->mpRuntime->GetClientConfirmedTick();
	}
	mSmoothedRollback.Update();
	mSmoothedBuffer = game::gpClientSession->mpRuntime->GetServerUpdateBufferSize();
	mSmoothedBuffer.Update();
	int64_t iRollbackValue = mSmoothedRollback.Get();
	rWorkbuffer.Append("Rollback: ");
	rWorkbuffer.Append(iRollbackValue);
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		if (iRollbackValue > 8)
		{
			rWorkbuffer.Append("!");
		}
	}
	rWorkbuffer.Append("  Buffer: ");
	rWorkbuffer.Append(mSmoothedBuffer.Get());
	rWorkbuffer.Append("\nDesync: ");
	bool bDesync = game::gpClientSession->mpDesyncManager->GetDesyncTick() >= 0;
	if (bDesync)
	{
		rWorkbuffer.Append("Yes (");
		rWorkbuffer.Append(game::gpClientSession->mpDesyncManager->GetDesyncTick());
		rWorkbuffer.Append(")");
		if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
		{
			rWorkbuffer.Append("!");
		}
	}
	else
	{
		rWorkbuffer.Append("No");
	}
}

void ProfileManagerBase::FormatNetworkClock(common::Workbuffer& rWorkbuffer)
{
	// -- Clock --
	rWorkbuffer.Append("\n-- Clock --\n");
	rWorkbuffer.Append("Offset: ");
	rWorkbuffer.Append(mSmoothedClockOffset.Get());
	rWorkbuffer.Append("  Target: -");
	rWorkbuffer.Append(mSmoothedClockTarget.Get());
	rWorkbuffer.Append("  Err: ");
	rWorkbuffer.Append(mSmoothedClockError.Get());
}

void ProfileManagerBase::FormatNetworkReconciliation(common::Workbuffer& rWorkbuffer)
{
	// -- Reconciliation --
	rWorkbuffer.Append("\n-- Reconciliation --\n");
	int64_t iCrc = mCrcValidatedTicksPerSecond.Get();
	int64_t iAssumed = mAssumedTicksPerSecond.Get();
	int64_t iFast = mCrcFastPathEventsPerSecond.Get();
	int64_t iStatus = mStatusChangeReplayTicksPerSecond.Get();
	int64_t iKnockOn = mKnockOnReplayTicksPerSecond.Get();

	bool bCrcFlag = false;
	bool bAssumedFlag = false;
	bool bFastFlag = false;
	bool bStatusFlag = false;
	bool bKnockOnFlag = false;
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		constexpr engine::NetworkSimulationBounds kBounds = engine::GetNetworkSimulationBounds(keNetworkSimulation);
		bCrcFlag = iCrc < kBounds.iCrcMin;
		bAssumedFlag = iAssumed > kBounds.iAssumedMax;
		bFastFlag = iFast > kBounds.iFastReplayMax;
		bStatusFlag = iStatus > kBounds.iStatusReplayMax;
		bKnockOnFlag = iKnockOn > kBounds.iKnockOnReplayMax;
	}

	rWorkbuffer.Append("CRC: ");
	rWorkbuffer.Append(iCrc);
	rWorkbuffer.Append(bCrcFlag ? "/s!" : "/s");
	rWorkbuffer.Append("  Assumed: ");
	rWorkbuffer.Append(iAssumed);
	rWorkbuffer.Append(bAssumedFlag ? "/s!" : "/s");
	rWorkbuffer.Append("\nReplay: ");
	rWorkbuffer.Append(iFast);
	rWorkbuffer.Append(bFastFlag ? " fast!" : " fast");
	rWorkbuffer.Append("  ");
	rWorkbuffer.Append(iStatus);
	rWorkbuffer.Append(bStatusFlag ? " status!" : " status");
	rWorkbuffer.Append("  ");
	rWorkbuffer.Append(iKnockOn);
	rWorkbuffer.Append(bKnockOnFlag ? " knock-on!" : " knock-on");
}

void ProfileManagerBase::SetClockCorrection(int64_t iOffset, int64_t iTargetBehind, int64_t iError)
{
	mSmoothedClockOffset = iOffset;
	mSmoothedClockTarget = iTargetBehind;
	mSmoothedClockError = iError;
	mSmoothedClockOffset.Update();
	mSmoothedClockTarget.Update();
	mSmoothedClockError.Update();
}

void ProfileManagerBase::SetReconcileCounters(int64_t iCrcValidated, int64_t iAssumed, int64_t iCrcFastPath, int64_t iStatusChangeReplay, int64_t iKnockOnReplay)
{
	mCrcValidatedTicksPerSecond.Set(iCrcValidated);
	mAssumedTicksPerSecond.Set(iAssumed);
	mCrcFastPathEventsPerSecond.Set(iCrcFastPath);
	mStatusChangeReplayTicksPerSecond.Set(iStatusChangeReplay);
	mKnockOnReplayTicksPerSecond.Set(iKnockOnReplay);
}

} // namespace engine

#endif // BT_CLIENT
