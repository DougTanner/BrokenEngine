#include "Agent/Commands/ClientDesyncProbe.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSession.h"
#include "Network/Client/ClientSessionRuntime.h"

namespace game
{

namespace
{

int64_t DesyncProbeCountParameter(const nlohmann::json& rParameters, std::string_view parameterName)
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

} // namespace

void CommandDesyncProbe(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Heap: validation errors, JSON result, and triggerRecovery's serialized Frame snapshot
	ScopedSuppressAllocationTracking suppress;

	if (!rParameters.is_object())
	{
		throw std::runtime_error("desync_probe params must be an object");
	}

	size_t iKnownParameterCount = static_cast<size_t>(rParameters.contains("desyncReports")) + static_cast<size_t>(rParameters.contains("debugFrameRequests")) + static_cast<size_t>(rParameters.contains("triggerRecovery"));
	if (rParameters.size() != iKnownParameterCount)
	{
		throw std::runtime_error("desync_probe accepts only 'desyncReports', 'debugFrameRequests', and 'triggerRecovery'");
	}

	int64_t iDesyncReportCount = rParameters.contains("desyncReports") ? DesyncProbeCountParameter(rParameters, "desyncReports") : 0;
	int64_t iDebugFrameRequestCount = rParameters.contains("debugFrameRequests") ? DesyncProbeCountParameter(rParameters, "debugFrameRequests") : 0;
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
	if (gpGame->InMainMenu())
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}

	auto coordIt = gpGame->mCoordFrames.find(gpGame->mClientGridCoord);
	if (coordIt == gpGame->mCoordFrames.end())
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}
	if (coordIt->second.iSnapshotCount <= 0)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}
	int64_t iSnapshotIndex = engine::SnapshotIndex(coordIt->second.iSnapshotHead, coordIt->second.iSnapshotCount - 1);
	const std::unique_ptr<Frame>& pCurrentFrame = coordIt->second.snapshots[iSnapshotIndex];
	if (pCurrentFrame == nullptr)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}

	const int64_t iTick = pCurrentFrame->interpolate.iTick;
	const engine::GridCoord coord = gpGame->mClientGridCoord;
	const common::crc_t uiExpectedCrc = pCurrentFrame->postRender.sharedCrc;
	const common::crc_t uiActualCrc = uiExpectedCrc ^ static_cast<common::crc_t>(1);

	for (int64_t i = 0; i < iDesyncReportCount; ++i)
	{
		gpClientSession->mpRuntime->mpClient->SendDesyncReport(iTick, coord, uiExpectedCrc, uiActualCrc);
	}
	for (int64_t i = 0; i < iDebugFrameRequestCount; ++i)
	{
		gpClientSession->mpRuntime->mpClient->SendDebugFrameRequest(iTick, coord);
	}

	if (bTriggerRecovery)
	{
		if (gpClientSession->mpDesyncCore->IsStalled())
		{
			throw std::runtime_error("desync_probe cannot trigger recovery while desync debug mode is already stalled");
		}

		std::ostringstream outputStream(std::ios::binary);
		outputStream << *pCurrentFrame;
		std::istringstream inputStream(outputStream.str(), std::ios::binary);
		std::unique_ptr<Frame> pSnapshot = std::make_unique<Frame>();
		inputStream >> *pSnapshot;

		engine::ReconcileDesyncInfo desyncInfo
		{
			.bDesync = true,
			.iDesyncTick = iTick,
			.desyncCoord = coord,
			.desyncExpectedCrc = uiExpectedCrc,
			.desyncActualCrc = uiActualCrc,
			.pDesyncClientFrame = std::move(pSnapshot),
		};
		gpClientSession->mpDesyncCore->OnDesyncDetected(std::move(desyncInfo));
	}

	rResult["tick"] = iTick;
	rResult["coord"] = {coord.x, coord.y};
	rResult["desyncDebugFrames"] = kbDesyncDebugFrames;
	rResult["stalled"] = gpClientSession->mpDesyncCore->IsStalled();
	rResult["desyncReports"] = iDesyncReportCount;
	rResult["debugFrameRequests"] = iDebugFrameRequestCount;
	rResult["triggerRecovery"] = bTriggerRecovery;
}

} // namespace game

#endif // BT_CLIENT
