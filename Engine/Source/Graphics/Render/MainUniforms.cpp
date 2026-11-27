#if defined(BT_CLIENT)

#include "Graphics/Debug/DebugRender.h"
#include "Ui/HexShieldWrappersBase.h"
#include "Ui/WrapperBase.h"
#include "Render.h"

#include "Frame/TerrainUtils.h"
#include "Game.h"

namespace engine
{

static void DebugRenderCellEdges(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	float fZ = game::kfBaseHeightMeters;
	static constexpr XMFLOAT4A kf4EdgeColor = {0.0f, 1.0f, 1.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCells.find(rCoordinate);
		if (it == game::gpGame->mCells.end())
		{
			continue;
		}

		// Every cell has the same local edges; the basis offset is what places this one on screen. The vertex shared by
		// edges k and k+1 is the centroid of the cell center and those two neighbour centers.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;
		XMVECTOR vecOffset = XMVectorSet(f2Offset.x, f2Offset.y, fZ, 0.0f);
		auto Vertex = [&](int64_t k)
		{
			XMVECTOR vecFirst = XMLoadFloat2(&kCellDirections.pEntries[k % common::kiHexDirectionCount].f2CenterOffset);
			XMVECTOR vecSecond = XMLoadFloat2(&kCellDirections.pEntries[(k + 1) % common::kiHexDirectionCount].f2CenterOffset);
			XMFLOAT3A f3Vertex {};
			XMStoreFloat3A(&f3Vertex, XMVectorAdd(XMVectorScale(XMVectorAdd(vecFirst, vecSecond), 1.0f / 3.0f), vecOffset));
			return f3Vertex;
		};

		for (int64_t k = 0; k < common::kiHexDirectionCount; ++k)
		{
			DebugRender::Line(Vertex(k), Vertex(k + 1), kf4EdgeColor);
		}
	}
}

static void DebugRenderIslandBoundaries(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	float fZ = game::kfBaseHeightMeters;
	static constexpr XMFLOAT4A kf4BoundaryColor = {1.0f, 0.0f, 1.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCells.find(rCoordinate);
		if (it == game::gpGame->mCells.end())
		{
			continue;
		}

		// Placement positions are local to this cell; the Rotate helper is the one point that converts them.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;

		for (const IslandPlacement& rPlacement : it->second.staticData.islands)
		{
			const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
			float fHalfX = 0.5f * rTemplate.fQuadFootprintX;
			float fHalfY = 0.5f * rTemplate.fQuadFootprintY;
			float fCosine = std::cos(rPlacement.fRotation);
			float fSine = std::sin(rPlacement.fRotation);

			auto Rotate = [&](float fLocalX, float fLocalY)
			{
				return XMFLOAT3A {rPlacement.f2WorldPosition.x + f2Offset.x + fLocalX * fCosine - fLocalY * fSine, rPlacement.f2WorldPosition.y + f2Offset.y + fLocalX * fSine + fLocalY * fCosine, fZ};
			};

			XMFLOAT3A f3Corner0 = Rotate(-fHalfX, -fHalfY);
			XMFLOAT3A f3Corner1 = Rotate(fHalfX, -fHalfY);
			XMFLOAT3A f3Corner2 = Rotate(fHalfX,  fHalfY);
			XMFLOAT3A f3Corner3 = Rotate(-fHalfX,  fHalfY);

			DebugRender::Line(f3Corner0, f3Corner1, kf4BoundaryColor);
			DebugRender::Line(f3Corner1, f3Corner2, kf4BoundaryColor);
			DebugRender::Line(f3Corner2, f3Corner3, kf4BoundaryColor);
			DebugRender::Line(f3Corner3, f3Corner0, kf4BoundaryColor);
		}
	}
}

static void DebugRenderIslandValidArea(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	// Drawn at the underwater mask threshold depth (the depth that defines the hull boundary), below
	// the magenta boundary rectangle / cyan cell edges at the base height. Debug lines are an overlay
	// (no depth test), so the underwater Z is never occluded by terrain or water.
	float fZ = common::kfUnderwaterMaskThresholdMeters;
	static constexpr XMFLOAT4A kf4ValidAreaColor = {0.0f, 1.0f, 0.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCells.find(rCoordinate);
		if (it == game::gpGame->mCells.end())
		{
			continue;
		}

		// Placement positions are local to this cell; the Rotate helper is the one point that converts them.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;

		for (const IslandPlacement& rPlacement : it->second.staticData.islands)
		{
			const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);

			float fCosine = std::cos(rPlacement.fRotation);
			float fSine = std::sin(rPlacement.fRotation);

			auto Rotate = [&](const XMFLOAT2& rVertex)
			{
				return XMFLOAT3A {rPlacement.f2WorldPosition.x + f2Offset.x + rVertex.x * fCosine - rVertex.y * fSine, rPlacement.f2WorldPosition.y + f2Offset.y + rVertex.x * fSine + rVertex.y * fCosine, fZ};
			};

			int64_t iCount = rTemplate.iValidAreaVertexCount;
			for (int64_t i = 0; i < iCount; ++i)
			{
				const XMFLOAT2& rStartVertex = rTemplate.pf2ValidAreaVertices[i];
				const XMFLOAT2& rEndVertex = rTemplate.pf2ValidAreaVertices[(i + 1) % iCount];
				DebugRender::Line(Rotate(rStartVertex), Rotate(rEndVertex), kf4ValidAreaColor);
			}
		}
	}
}

static void DebugRenderNavigationData(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	static constexpr XMFLOAT4A kf4VertexColor = {1.0f, 0.5f, 0.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCells.find(rCoordinate);
		if (it == game::gpGame->mCells.end())
		{
			continue;
		}

		// Nav vertices are local to this cell; the two draw sites below are this helper's conversion point.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;

		// A cell whose static data has not arrived holds no per-graph entries.
		const std::vector<NavData>& rGraphNavigationData = it->second.staticData.navigationData;
		for (int64_t i = 0; i < std::ssize(rGraphNavigationData); ++i)
		{
			const NavData& rNavigationData = rGraphNavigationData.at(i);
			const NavigationGraphSpec& rGraph = game::kNavigationGraphSpecs.at(i);
			float fZ = rGraph.fDebugDrawZ;

			for (int64_t j = 0; j < std::ssize(rNavigationData.polygonOffsets); ++j)
			{
				int64_t iStart = rNavigationData.polygonOffsets.at(j);
				int64_t iEnd = (j + 1 < std::ssize(rNavigationData.polygonOffsets)) ? rNavigationData.polygonOffsets.at(j + 1) : std::ssize(rNavigationData.vertices);

				for (int64_t k = iStart; k < iEnd; ++k)
				{
					int64_t iNext = (k + 1 < iEnd) ? k + 1 : iStart;
					XMFLOAT3A f3StartVertex = {rNavigationData.vertices.at(k).x + f2Offset.x, rNavigationData.vertices.at(k).y + f2Offset.y, fZ};
					XMFLOAT3A f3EndVertex = {rNavigationData.vertices.at(iNext).x + f2Offset.x, rNavigationData.vertices.at(iNext).y + f2Offset.y, fZ};
					DebugRender::Line(f3StartVertex, f3EndVertex, rGraph.f4DebugColor);
				}
			}

			for (const XMFLOAT2& rVertex : rNavigationData.vertices)
			{
				DebugRender::Circle({rVertex.x + f2Offset.x, rVertex.y + f2Offset.y, fZ}, 0.75f, kf4VertexColor);
			}
		}
	}
}

static void PopulateHexShield(shaders::MainLayout& rMainLayout)
{
	rMainLayout.fHexShieldGrow = engine::gHexShieldGrow.mfCurrent;
	rMainLayout.fHexShieldEdgeDistance = engine::gHexShieldEdgeDistance.mfCurrent;
	rMainLayout.fHexShieldEdgeMultiplier = engine::gHexShieldEdgeMultiplier.mfCurrent;

	rMainLayout.fHexShieldWaveMultiplier = engine::gHexShieldWaveMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveDotMultiplier = engine::gHexShieldWaveDotMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveIntensityMultiplier = engine::gHexShieldWaveIntensityMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveIntensityPower = engine::gHexShieldWaveIntensityPower.mfCurrent;
	rMainLayout.fHexShieldWaveFalloffPower = engine::gHexShieldWaveFalloffPower.mfCurrent;

	rMainLayout.fHexShieldDirectionFalloffPower = engine::gHexShieldDirectionFalloffPower.mfCurrent;
	rMainLayout.fHexShieldDirectionMultiplier = engine::gHexShieldDirectionMultiplier.mfCurrent;
}

// Population counters: one sum over the cells this transaction renders, published once so a render with no renderable
// cell reads zero.
static void PublishPopulationCounters(const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	if constexpr (!kbProfiling)
	{
		return;
	}

	int64_t iTotalExplosions = 0;
	int64_t iTotalPushers = 0;
	int64_t iTotalBillboards = 0;
	int64_t iTotalPuffs = 0;
	int64_t iTotalSmokeTrails = 0;
	int64_t iTotalPointLights = 0;
	int64_t iTotalAreaLights = 0;
	int64_t iTotalHexShields = 0;
	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it != rRenderInterpolates.end())
		{
			iTotalExplosions += it->second.explosions.iCount;
			iTotalPushers += it->second.pushers.iCount;
			iTotalBillboards += it->second.billboards.iCount;
			iTotalPuffs += it->second.puffs.iCount;
			iTotalSmokeTrails += it->second.smokeTrails.iCount;
			iTotalPointLights += it->second.pointLights.iCount;
			iTotalAreaLights += it->second.areaLights.iCount;
			iTotalHexShields += it->second.hexShields.iCount;
		}
	}
	gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount = iTotalExplosions;
	gpProfileManager->GetCpuCounter(kCpuCounterPushers).iCount = iTotalPushers;
	gpProfileManager->GetCpuCounter(kCpuCounterBillboards).iCount = iTotalBillboards;
	gpProfileManager->GetCpuCounter(kCpuCounterPuffs).iCount = iTotalPuffs;
	gpProfileManager->GetCpuCounter(kCpuCounterSmokeTrails).iCount = iTotalSmokeTrails;
	gpProfileManager->GetCpuCounter(kCpuCounterPointLights).iCount = iTotalPointLights;
	gpProfileManager->GetCpuCounter(kCpuCounterAreaLights).iCount = iTotalAreaLights;
	gpProfileManager->GetCpuCounter(kCpuCounterHexShields).iCount = iTotalHexShields;
}

// The engine collections begin and end each render transaction ahead of the game's.
static void BeginFrameInterpolateRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	PublishPopulationCounters(rRenderInterpolates, rActiveCoordinates);
	ForEachBeginRender(InterpolateTypes {}, iCommandBuffer, rRenderInterpolates, rActiveCoordinates);
	game::FrameInterpolate::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);
}

static void EndFrameInterpolateRender(int64_t iCommandBuffer)
{
	ForEachEndRender(InterpolateTypes {}, iCommandBuffer);
	game::FrameInterpolate::EndRender(iCommandBuffer);
}

void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, std::chrono::duration<float> currentTime)
{
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	RenderWaterMain(iCommandBuffer, currentTime);
	RenderLightingMain(iCommandBuffer);

	if (!rRenderInterpolates.contains(cameraCoordinate))
	{
		gpPipelineManager->mpPipelines[kPipelineWater].WriteIndirectBuffer(iCommandBuffer, 0);
	}

	gpBufferManager->ResetSkinningAllocations(iCommandBuffer);

	BeginFrameInterpolateRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);

	// Render the camera cell first to keep its index at zero.
	auto RenderFrame = [&](const GridCoord& rCoordinate)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it == rRenderInterpolates.end())
		{
			return;
		}
		const game::FrameInterpolate& rFrameInterpolate = it->second;
		// Automatically dispatched main collections
		ForEachInterpolateRender(InterpolateTypes {}, rFrameInterpolate, iCommandBuffer);
		game::FrameInterpolate::Render(rFrameInterpolate, iCommandBuffer);
		// Manually rendered trail collections
		SmokeTrailsInterpolate::Render(rFrameInterpolate, iCommandBuffer);
		WindTrailsInterpolate::Render(rFrameInterpolate, iCommandBuffer);
	};
	RenderFrame(cameraCoordinate);
	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		if (rCoordinate == cameraCoordinate)
		{
			continue;
		}
		RenderFrame(rCoordinate);
	}

	EndFrameInterpolateRender(iCommandBuffer);

	// EndRender publishes deposit indirect counts before spread indirect parameters are published.
	RenderLightingSpreadIndirect(iCommandBuffer);

	DebugRenderNavigationData(rActiveCoordinates, cameraCoordinate);
	DebugRenderCellEdges(rActiveCoordinates, cameraCoordinate);
	DebugRenderIslandBoundaries(rActiveCoordinates, cameraCoordinate);
	DebugRenderIslandValidArea(rActiveCoordinates, cameraCoordinate);

	DebugRender::BeginRender(iCommandBuffer);
	DebugRender::EndRender(iCommandBuffer);

	rMainLayout.iFrameNumber = static_cast<int>(engine::gpCamera->miFrame);
	rMainLayout.iRenderNumber = static_cast<int32_t>(gpGraphics->miFrameCounter);

	float fCameraShake = engine::gpCamera->mfShake;
	static constexpr float kfMaxRoll = 0.005f;
	static constexpr float kfMaxPitch = 0.005f;
	static constexpr float kfMaxYaw = 0.01f;
	// Fixed-seed permutation tables are identical every frame — construct once (each ctor reshuffles a 256-entry table).
	static siv::BasicPerlinNoise<float> sPerlinRoll(0);
	static siv::BasicPerlinNoise<float> sPerlinPitch(1);
	static siv::BasicPerlinNoise<float> sPerlinYaw(2);
	float fShakeTime = common::NanosecondsToFloatSeconds<float>(TickToSimulationTime(engine::gpCamera->miFrame).time_since_epoch());
	auto matCameraShake = XMMatrixRotationRollPitchYaw(kfMaxRoll * fCameraShake * (-1.0f + 2.0f * sPerlinRoll.octave1D_01(8.0f * fShakeTime, 4)), kfMaxPitch * fCameraShake * (-1.0f + 2.0f * sPerlinPitch.octave1D_01(8.0f * fShakeTime, 4)), kfMaxYaw * fCameraShake * (-1.0f + 2.0f * sPerlinYaw.octave1D_01(8.0f * fShakeTime, 4)));

	XMMATRIX matViewProjection = XMMatrixTranspose(XMMatrixMultiply(engine::gpCamera->mMatView, XMMatrixMultiply(matCameraShake, engine::gpCamera->mMatProjection)));
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ViewProjection[0]), matViewProjection);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ModelViewProjection[0]), matViewProjection);

	XMStoreFloat4(&rMainLayout.f4EyePosition, engine::gpCamera->mVecEyePosition);
	XMStoreFloat4(&rMainLayout.f4WaterEyePosition, engine::gpCamera->mVecWaterEyePosition);
	XMStoreFloat4(&rMainLayout.f4ModelEyePosition, engine::gpCamera->mVecEyePosition);
	XMVECTOR vecToEyeNormal = engine::gpCamera->mVecToEyeNormal;
	XMStoreFloat4(&rMainLayout.f4ToEyeNormal, XMVectorSetW(vecToEyeNormal, engine::gpCamera->mbOrthographicProjection ? 1.0f : 0.0f));
	// DebugRenderBillboard.vert consumes this basis for every vertex; forward remains f4ToEyeNormal.
	// The reference axis switches from +Z to +X at |forward.z| = 0.999; right is normalized and up = cross(forward, right) is not.
	XMVECTOR vecBillboardWorldUpNormal = std::abs(XMVectorGetZ(vecToEyeNormal)) < 0.999f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR vecBillboardRightNormal = XMVector3Normalize(XMVector3Cross(vecBillboardWorldUpNormal, vecToEyeNormal));
	XMStoreFloat4(&rMainLayout.f4BillboardRight, vecBillboardRightNormal);
	XMStoreFloat4(&rMainLayout.f4BillboardUp, XMVector3Cross(vecToEyeNormal, vecBillboardRightNormal));

	PopulateHexShield(rMainLayout);
}

} // namespace engine

#endif // defined(BT_CLIENT)
