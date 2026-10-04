#include "Agent/Commands/ClientDesyncProbe.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"

#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

static int64_t DesynchronizationProbeCountParameter(const nlohmann::json& rParameters, std::string_view parameterName)
{
	std::string parameterNameString(parameterName);
	if (!rParameters.contains(parameterNameString))
	{
		throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
	}
	if (!rParameters.at(parameterNameString).is_number_integer())
	{
		throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
	}

	if (rParameters.at(parameterNameString).is_number_unsigned())
	{
		uint64_t uiCount = rParameters.at(parameterNameString).get<uint64_t>();
		if (uiCount > 8)
		{
			throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
		}
		return static_cast<int64_t>(uiCount);
	}

	int64_t iCount = rParameters.at(parameterNameString).get<int64_t>();
	if (iCount < 0)
	{
		throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
	}
	if (iCount > 8)
	{
		throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
	}
	return iCount;
}

void CommandDesynchronizationProbe(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Heap: validation errors, JSON result, and triggerRecovery's serialized Frame snapshot
	ScopedSuppressAllocationTracking suppress;

	if (!rParameters.is_object())
	{
		throw std::runtime_error("desync_probe params must be an object");
	}

	size_t uiKnownParameterCount = static_cast<size_t>(rParameters.contains("desyncReports")) + static_cast<size_t>(rParameters.contains("debugFrameRequests")) + static_cast<size_t>(rParameters.contains("triggerRecovery"));
	if (rParameters.size() != uiKnownParameterCount)
	{
		throw std::runtime_error("desync_probe accepts only 'desyncReports', 'debugFrameRequests', and 'triggerRecovery'");
	}

	int64_t iDesynchronizationReportCount = rParameters.contains("desyncReports") ? DesynchronizationProbeCountParameter(rParameters, "desyncReports") : 0;
	int64_t iDebugFrameRequestCount = rParameters.contains("debugFrameRequests") ? DesynchronizationProbeCountParameter(rParameters, "debugFrameRequests") : 0;
	bool bTriggerRecovery = false;
	if (rParameters.contains("triggerRecovery"))
	{
		if (!rParameters.at("triggerRecovery").is_boolean())
		{
			throw std::runtime_error("desync_probe 'triggerRecovery' must be a boolean");
		}
		bTriggerRecovery = rParameters.at("triggerRecovery").get<bool>();
		if (!bTriggerRecovery)
		{
			throw std::runtime_error("desync_probe 'triggerRecovery' must be true when present");
		}
	}
	if (bTriggerRecovery)
	{
		if (rParameters.contains("desyncReports"))
		{
			throw std::runtime_error("desync_probe 'triggerRecovery' is mutually exclusive with packet counts");
		}
		if (rParameters.contains("debugFrameRequests"))
		{
			throw std::runtime_error("desync_probe 'triggerRecovery' is mutually exclusive with packet counts");
		}
	}

	if (gpGame == nullptr)
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}
	if (gpClientSession == nullptr)
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}
	if (gpClientSession->mpRuntime->mpClient == nullptr)
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}
	if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}
	if (gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr)
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}
	if ((gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}

	auto it = gpGame->mCoordinateFrames.find(gpGame->mClientGridCoordinate);
	if (it == gpGame->mCoordinateFrames.end())
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}
	if (it->second.iSnapshotCount <= 0)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}
	int64_t iSnapshotIndex = engine::SnapshotIndex(it->second.iSnapshotHead, it->second.iSnapshotCount - 1);
	const std::unique_ptr<Frame>& rpCurrentFrame = it->second.snapshots[iSnapshotIndex];
	if (rpCurrentFrame == nullptr)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}

	int64_t iTick = rpCurrentFrame->interpolate.iTick;
	engine::GridCoord coordinate = gpGame->mClientGridCoordinate;
	common::crc_t uiExpectedCrc = rpCurrentFrame->postRender.uiSharedCrc;
	common::crc_t uiActualCrc = uiExpectedCrc ^ static_cast<common::crc_t>(1);

	for (int64_t i = 0; i < iDesynchronizationReportCount; ++i)
	{
		gpClientSession->mpRuntime->mpClient->SendDesynchronizationReport(iTick, coordinate, uiExpectedCrc, uiActualCrc);
	}
	for (int64_t i = 0; i < iDebugFrameRequestCount; ++i)
	{
		gpClientSession->mpRuntime->mpClient->SendDebugFrameRequest(iTick, coordinate);
	}

	if (bTriggerRecovery)
	{
		if (gpClientSession->mpDesynchronizationCore->IsStalled())
		{
			throw std::runtime_error("desync_probe cannot trigger recovery while desync debug mode is already stalled");
		}

		std::unique_ptr<Frame> pSnapshot = std::make_unique<Frame>();
		engine::TransferViaStream(*rpCurrentFrame, *pSnapshot);

		engine::ReconcileDesyncInfo desynchronizationInformation
		{
			.bDesync = true,
			.iDesyncTick = iTick,
			.desyncCoord = coordinate,
			.desyncExpectedCrc = uiExpectedCrc,
			.desyncActualCrc = uiActualCrc,
			.pDesyncClientFrame = std::move(pSnapshot),
		};
		gpClientSession->mpDesynchronizationCore->OnDesyncDetected(std::move(desynchronizationInformation));
	}

	rResult["tick"] = iTick;
	rResult["coord"] = {coordinate.iX, coordinate.iY};
	rResult["desyncDebugFrames"] = kbDesynchronizationDebugFrames;
	rResult["stalled"] = gpClientSession->mpDesynchronizationCore->IsStalled();
	rResult["desyncReports"] = iDesynchronizationReportCount;
	rResult["debugFrameRequests"] = iDebugFrameRequestCount;
	rResult["triggerRecovery"] = bTriggerRecovery;
}

} // namespace game

#endif // BT_CLIENT
