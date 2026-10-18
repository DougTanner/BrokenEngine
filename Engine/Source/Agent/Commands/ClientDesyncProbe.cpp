#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/ClientDesyncProbe.h"

#include "Network/Client/ReconcileReplay.h"

#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace engine
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
		if (rParameters.at(parameterNameString) > 8ui64)
		{
			throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
		}
		int64_t iCount = rParameters.at(parameterNameString).get<int64_t>();
		return iCount;
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

	int64_t iKnownParameterCount = static_cast<int64_t>(rParameters.contains("desyncReports")) + static_cast<int64_t>(rParameters.contains("debugFrameRequests")) + static_cast<int64_t>(rParameters.contains("triggerRecovery"));
	if (std::ssize(rParameters) != iKnownParameterCount)
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

	Client& rClient = RequireAcceptedClient("desync_probe");
	if ((game::gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}

	auto it = game::gpGame->mCells.find(game::gpGame->mClientGridCoordinate);
	if (it == game::gpGame->mCells.end())
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}
	const game::Frame* pCurrentFrame = it->second.TailFrame();
	if (pCurrentFrame == nullptr)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}

	int64_t iTick = pCurrentFrame->interpolate.iTick;
	engine::GridCoord coordinate = game::gpGame->mClientGridCoordinate;
	common::crc_t uiExpectedCrc = pCurrentFrame->postRender.uiSharedCrc;
	common::crc_t uiActualCrc = uiExpectedCrc ^ static_cast<common::crc_t>(1);

	for (int64_t i = 0; i < iDesynchronizationReportCount; ++i)
	{
		rClient.SendDesynchronizationReport(iTick, coordinate, uiExpectedCrc, uiActualCrc);
	}
	for (int64_t i = 0; i < iDebugFrameRequestCount; ++i)
	{
		rClient.SendDebugFrameRequest(iTick, coordinate);
	}

	if (bTriggerRecovery)
	{
		if (game::gpClientSession->mpDesynchronizationCore->IsStalled())
		{
			throw std::runtime_error("desync_probe cannot trigger recovery while desync debug mode is already stalled");
		}

		std::unique_ptr<game::Frame> pSnapshot = std::make_unique<game::Frame>();
		engine::TransferViaStream(*pCurrentFrame, *pSnapshot);

		engine::ReconcileDesyncInfo desynchronizationInformation
		{
			.bDesync = true,
			.iDesyncTick = iTick,
			.desyncCoord = coordinate,
			.desyncExpectedCrc = uiExpectedCrc,
			.desyncActualCrc = uiActualCrc,
			.pDesyncClientFrame = std::move(pSnapshot),
		};
		game::gpClientSession->mpDesynchronizationCore->OnDesyncDetected(std::move(desynchronizationInformation));
	}

	rResult["tick"] = iTick;
	rResult["coord"] = {coordinate.iX, coordinate.iY};
	rResult["desyncDebugFrames"] = kbDesynchronizationDebugFrames;
	rResult["stalled"] = game::gpClientSession->mpDesynchronizationCore->IsStalled();
	rResult["desyncReports"] = iDesynchronizationReportCount;
	rResult["debugFrameRequests"] = iDebugFrameRequestCount;
	rResult["triggerRecovery"] = bTriggerRecovery;
}

} // namespace engine

#endif // BT_CLIENT
