#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/PresentationContinuityProbe.h"

#include "Agent/AgentCommandsShared.h"

namespace engine
{

static nlohmann::json OriginJson(XMFLOAT2 f2Origin)
{
	return nlohmann::json::array({f2Origin.x, f2Origin.y});
}

static nlohmann::json AreaJson(const XMFLOAT4& rf4Area)
{
	return nlohmann::json::array({rf4Area.x, rf4Area.y, rf4Area.z, rf4Area.w});
}

static nlohmann::json RetainedAreaJson(const RetainedAreaReport& rReport)
{
	return nlohmann::json
	{
		{"currentArea", AreaJson(rReport.f4CurrentArea)},
		{"previousArea", AreaJson(rReport.f4PreviousArea)},
	};
}

static nlohmann::json LatticeJson(const LatticeReport& rReport)
{
	return nlohmann::json
	{
		{"exponent", rReport.iExponent},
		{"anchor", nlohmann::json::array({rReport.puiAnchor[0], rReport.puiAnchor[1]})},
		{"remainder", OriginJson(rReport.f2Remainder)},
	};
}

// presentation_continuity_probe: report what the last published render frame sent for water phase reduction, for each
// retained-history owner, and for each patch lattice, so the harness can observe continuity across a camera cell change.
void CommandPresentationContinuityProbe(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.empty())
	{
		throw std::runtime_error("presentation_continuity_probe requires empty params");
	}

	const PresentationContinuitySnapshot& rSnapshot = gPresentationContinuity;
	rResult["publishedFrames"] = rSnapshot.iPublishedFrames;
	rResult["basisCoord"] = AgentCoordinateJson(rSnapshot.cameraBasisCoordinate);
	rResult["water"] =
	{
		{"origin", OriginJson(rSnapshot.f2WaterOrigin)},
		{"reducedNoiseOrigin", OriginJson(rSnapshot.f2ReducedNoiseOrigin)},
		{"noiseFrequency", rSnapshot.fNoiseFrequency},
	};
	rResult["shadow"] = RetainedAreaJson(rSnapshot.shadow);
	nlohmann::json& rLighting = rResult["lighting"] = nlohmann::json::array();
	for (const RetainedAreaReport& rLightingLayer : rSnapshot.pLighting)
	{
		rLighting.push_back(RetainedAreaJson(rLightingLayer));
	}
	rResult["smoke"] = RetainedAreaJson(rSnapshot.smoke);
	rResult["lattices"] =
	{
		{"waterNormalOneOctaveOne", LatticeJson(rSnapshot.pWaterNormalLattices[0])},
		{"waterNormalTwoOctaveOne", LatticeJson(rSnapshot.pWaterNormalLattices[1])},
		{"waterNormalThreeOctaveOne", LatticeJson(rSnapshot.pWaterNormalLattices[2])},
		{"waterColorNoiseOne", LatticeJson(rSnapshot.pWaterColorNoiseLattices[0])},
		{"waterColorNoiseTwo", LatticeJson(rSnapshot.pWaterColorNoiseLattices[1])},
		{"foamColor", LatticeJson(rSnapshot.waterFoamTextureLattice)},
		{"foamBreakup", LatticeJson(rSnapshot.waterFoamBreakupLattice)},
		{"foamSpawn", LatticeJson(rSnapshot.waterFoamSpawnNoiseLattice)},
	};
}

} // namespace engine

#endif // defined(BT_CLIENT)
