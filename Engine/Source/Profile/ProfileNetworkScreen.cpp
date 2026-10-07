#include "Pch.h"

#if defined(BT_CLIENT)

#include "ProfileManagerBase.h"

#include "Network/Client/ClientSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

static void AppendBytes(common::Workbuffer& rWorkbuffer, int64_t iBytes)
{
	if (iBytes >= 1'024 * 1'024)
	{
		rWorkbuffer.AppendFloat(static_cast<float>(iBytes) / (1'024.0f * 1'024.0f), 1);
		rWorkbuffer.Append(" MB/s");
	}
	else
	{
		rWorkbuffer.AppendFloat(static_cast<float>(iBytes) / 1'024.0f, 1);
		rWorkbuffer.Append(" KB/s");
	}
}

static void FormatNetworkPeerMetrics(common::Workbuffer& rWorkbuffer, const ENetPeer& rPeer)
{
	[[maybe_unused]] static constexpr engine::NetworkSimulationConfig kSimulationConfiguration = engine::GetNetworkSimulationConfiguration(keNetworkSimulation);
	int64_t iRtt = static_cast<int64_t>(rPeer.roundTripTime);
	rWorkbuffer.Append("RTT: ");
	rWorkbuffer.Append(iRtt);
	rWorkbuffer.Append(" ms");
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		if (iRtt > kSimulationConfiguration.iPingMaximumMilliseconds * 3 / 2)
		{
			rWorkbuffer.Append("!");
		}
	}

	rWorkbuffer.Append("  Pipe: ");
	rWorkbuffer.AppendFloat(engine::gpClient->mSmoothedPipelineRoundTripTimeMicroseconds.mSmoothedValue / 1'000.0f, 1);
	rWorkbuffer.Append(" ms\n");

	float fLoss = rPeer.packetLoss * 100.0f / 65'536.0f;
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
			if (fLoss > kSimulationConfiguration.fPacketLossPercent * 2.0f)
			{
				rWorkbuffer.Append("!");
			}
			rWorkbuffer.Append(" (sim: ");
			rWorkbuffer.AppendFloat(kSimulationConfiguration.fPacketLossPercent, 1);
			rWorkbuffer.Append("%)");
		}
	}
	rWorkbuffer.Append("\n");

	int64_t iActiveSlotCount = 0;
	for (const engine::ClientCoordSlot& rSlot : engine::gpClient->mSubscriptions.mCoordinateSlots)
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
	rWorkbuffer.AppendFloat(engine::gpClient->mSmoothedJitterMicroseconds.mSmoothedValue / 1'000.0f, 1);
	rWorkbuffer.Append(" ms\n");

	gpProfileManager->mSmoothedRoundTripTime = iRtt;
	gpProfileManager->mSmoothedRoundTripTime.Update();
	gpProfileManager->mSmoothedJitter = engine::gpClient->mSmoothedJitterMicroseconds.mSmoothedValue / 1'000;
	gpProfileManager->mSmoothedJitter.Update();
}

static void FormatNetworkTraffic(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("In: ");
	AppendBytes(rWorkbuffer, engine::gpClient->mBytesInPerSecond.Get());
	rWorkbuffer.Append("  Out: ");
	AppendBytes(rWorkbuffer, engine::gpClient->mBytesOutPerSecond.Get());
}

static void FormatNetworkTransport(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("\n-- Transport --\n");
	ENetPeer* pPeer = engine::gpClient->mpServerPeer;
	if (pPeer != nullptr)
	{
		FormatNetworkPeerMetrics(rWorkbuffer, *pPeer);
	}
	FormatNetworkTraffic(rWorkbuffer);
}

static void FormatNetworkSynchronization(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("\n-- Sync --\n");
	{
		int64_t iMinimumAcknowledgmentFloor = -1;
		int64_t iTotalReceived = 0;
		for (const engine::ClientCoordSlot& rSlot : engine::gpClient->mSubscriptions.mCoordinateSlots)
		{
			if (rSlot.eState == engine::CoordSubscriptionState::kActive)
			{
				if (iMinimumAcknowledgmentFloor < 0 || rSlot.acknowledgementState.iAcknowledgmentFloor < iMinimumAcknowledgmentFloor)
				{
					iMinimumAcknowledgmentFloor = rSlot.acknowledgementState.iAcknowledgmentFloor;
				}
				iTotalReceived += std::popcount(rSlot.acknowledgementState.uiReceivedBitfieldLow) + std::popcount(rSlot.acknowledgementState.uiReceivedBitfieldHigh);
			}
		}
		rWorkbuffer.Append("Ack: ");
		rWorkbuffer.Append(iMinimumAcknowledgmentFloor);
		rWorkbuffer.Append("  Conf: ");
		rWorkbuffer.Append(game::gpClientSession->mpRuntime->GetConfirmedTick());
		gpProfileManager->mSmoothedReceived = iTotalReceived;
	}
	gpProfileManager->mSmoothedReceived.Update();
	rWorkbuffer.Append("\nRecv: ");
	rWorkbuffer.Append(gpProfileManager->mSmoothedReceived.mSmoothedValue);
	rWorkbuffer.Append("/128");
}

static void FormatNetworkPrediction(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("\n-- Prediction --\n");
	auto it = game::gpGame->mCells.find(game::gpGame->mClientGridCoordinate);
	if (it != game::gpGame->mCells.end() && it->second.iSnapshotCount > 0)
	{
		gpProfileManager->mSmoothedRollback = game::gpGame->RenderFrame(game::gpGame->mClientGridCoordinate).interpolate.iTick - game::gpClientSession->mpRuntime->GetClientConfirmedTick();
	}
	gpProfileManager->mSmoothedRollback.Update();
	gpProfileManager->mSmoothedBuffer = game::gpClientSession->mpRuntime->GetServerUpdateBufferSize();
	gpProfileManager->mSmoothedBuffer.Update();
	int64_t iRollbackValue = gpProfileManager->mSmoothedRollback.mSmoothedValue;
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
	rWorkbuffer.Append(gpProfileManager->mSmoothedBuffer.mSmoothedValue);
	rWorkbuffer.Append("\nDesync: ");
	bool bDesynchronization = game::gpClientSession->mpDesynchronizationCore->mDesyncDebugState.iTick >= 0;
	if (bDesynchronization)
	{
		rWorkbuffer.Append("Yes (");
		rWorkbuffer.Append(game::gpClientSession->mpDesynchronizationCore->mDesyncDebugState.iTick);
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

static void FormatNetworkClock(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("\n-- Clock --\n");
	rWorkbuffer.Append("Offset: ");
	rWorkbuffer.Append(gpProfileManager->mSmoothedClockOffset.mSmoothedValue);
	rWorkbuffer.Append("  Target: -");
	rWorkbuffer.Append(gpProfileManager->mSmoothedClockTarget.mSmoothedValue);
	rWorkbuffer.Append("  Err: ");
	rWorkbuffer.Append(gpProfileManager->mSmoothedClockError.mSmoothedValue);
}

static void FormatNetworkReconciliation(common::Workbuffer& rWorkbuffer)
{
	rWorkbuffer.Append("\n-- Reconciliation --\n");
	int64_t iCrc = gpProfileManager->mCrcValidatedTicksPerSecond.Get();
	int64_t iAssumed = gpProfileManager->mAssumedTicksPerSecond.Get();
	int64_t iFast = gpProfileManager->mCrcFastPathEventsPerSecond.Get();
	int64_t iStatus = gpProfileManager->mStatusChangeReplayTicksPerSecond.Get();
	int64_t iKnockOn = gpProfileManager->mKnockOnReplayTicksPerSecond.Get();

	bool bCrcFlag = false;
	bool bAssumedFlag = false;
	bool bFastFlag = false;
	bool bStatusFlag = false;
	bool bKnockOnFlag = false;
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		static constexpr engine::NetworkSimulationBounds kBounds = engine::GetNetworkSimulationBounds(keNetworkSimulation);
		bCrcFlag = iCrc < kBounds.iCrcMinimum;
		bAssumedFlag = iAssumed > kBounds.iAssumedMaximum;
		bFastFlag = iFast > kBounds.iFastReplayMaximum;
		bStatusFlag = iStatus > kBounds.iStatusReplayMaximum;
		bKnockOnFlag = iKnockOn > kBounds.iKnockOnReplayMaximum;
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

void FormatNetworkScreen(common::Workbuffer& rWorkbuffer)
{
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		static constexpr engine::NetworkSimulationConfig kSimulationConfiguration = engine::GetNetworkSimulationConfiguration(keNetworkSimulation);
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
			rWorkbuffer.Append(kSimulationConfiguration.iPingMinimumMilliseconds);
			rWorkbuffer.Append("-");
			rWorkbuffer.Append(kSimulationConfiguration.iPingMaximumMilliseconds);
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
		FormatNetworkSynchronization(rWorkbuffer);
		FormatNetworkPrediction(rWorkbuffer);
		FormatNetworkClock(rWorkbuffer);
		FormatNetworkReconciliation(rWorkbuffer);
	}

	engine::gpImGuiManager->UpdateTextArea(engine::kTextProfileFps, rWorkbuffer.View());
}

} // namespace engine

#endif // BT_CLIENT
