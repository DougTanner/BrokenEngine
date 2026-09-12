#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/PresentationContinuityProbe.h"

#include "Agent/AgentCommandsShared.h"

namespace engine
{

namespace
{

nlohmann::json OriginJson(XMFLOAT2 f2Origin)
{
	return nlohmann::json::array({f2Origin.x, f2Origin.y});
}

nlohmann::json AreaJson(const XMFLOAT4& rf4Area)
{
	return nlohmann::json::array({rf4Area.x, rf4Area.y, rf4Area.z, rf4Area.w});
}

nlohmann::json RetainedAreaJson(const RetainedAreaReport& rReport)
{
	return nlohmann::json
	{
		{"currentArea", AreaJson(rReport.f4CurrentArea)},
		{"previousArea", AreaJson(rReport.f4PreviousArea)},
		{"historyResets", rReport.iHistoryResets},
	};
}

} // namespace

// presentation_continuity_probe: report what the last published render frame sent for water phase reduction and for
// each retained-history owner, so the harness can observe continuity across a camera cell change. Schema: {}.
void CommandPresentationContinuityProbe(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.is_object() || !rParams.empty())
	{
		throw std::runtime_error("presentation_continuity_probe requires empty params");
	}

	const PresentationContinuitySnapshot& rSnapshot = gPresentationContinuity;
	rResult["publishedFrames"] = rSnapshot.iPublishedFrames;
	rResult["basisCoord"] = AgentCoordJson(rSnapshot.cameraBasisCoord);
	rResult["water"] =
	{
		{"origin", OriginJson(rSnapshot.f2WaterOrigin)},
		{"reducedNoiseOrigin", OriginJson(rSnapshot.f2ReducedNoiseOrigin)},
		{"noiseFrequency", rSnapshot.fNoiseFrequency},
	};
	rResult["shadow"] = RetainedAreaJson(rSnapshot.shadow);
	rResult["lighting"] = RetainedAreaJson(rSnapshot.lighting);
	rResult["smoke"] = RetainedAreaJson(rSnapshot.smoke);
}

} // namespace engine

#endif // defined(BT_CLIENT)
