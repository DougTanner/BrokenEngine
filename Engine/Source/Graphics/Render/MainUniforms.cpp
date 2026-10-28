#if defined(BT_CLIENT)

#include "Graphics/Debug/DebugRender.h"
#include "Ui/HexShieldWrappersBase.h"
#include "Ui/WrapperBase.h"
#include "Render.h"

#include "Profile/ProfileRows.h"
#include "Game.h"

namespace engine
{

static void DebugRenderCellEdges(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	float fZ = gpFlightPlanes->Lowest().fHeightMeters;
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

	float fZ = gpFlightPlanes->Lowest().fHeightMeters;
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
			XMFLOAT3A f3Corner1 = Rotate( fHalfX, -fHalfY);
			XMFLOAT3A f3Corner2 = Rotate( fHalfX,  fHalfY);
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
	// the magenta boundary rectangle / cyan cell edges at the lowest flight plane. Debug lines are an overlay
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

	static constexpr XMFLOAT4A kf4PolygonColor = {1.0f, 1.0f, 0.0f, 1.0f};
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

		// A cell whose static data has not arrived holds no per-plane entries.
		const std::vector<NavData>& rPlaneNavigationData = it->second.staticData.navigationData;
		for (int64_t i = 0; i < std::ssize(rPlaneNavigationData); ++i)
		{
			const NavData& rNavigationData = rPlaneNavigationData.at(i);
			float fZ = gpFlightPlanes->Plane(i).fHeightMeters;

			for (int64_t j = 0; j < std::ssize(rNavigationData.polygonOffsets); ++j)
			{
				int64_t iStart = rNavigationData.polygonOffsets.at(j);
				int64_t iEnd = (j + 1 < std::ssize(rNavigationData.polygonOffsets)) ? rNavigationData.polygonOffsets.at(j + 1) : std::ssize(rNavigationData.vertices);

				for (int64_t k = iStart; k < iEnd; ++k)
				{
					int64_t iNext = (k + 1 < iEnd) ? k + 1 : iStart;
					XMFLOAT3A f3StartVertex = {rNavigationData.vertices.at(k).x + f2Offset.x, rNavigationData.vertices.at(k).y + f2Offset.y, fZ};
					XMFLOAT3A f3EndVertex = {rNavigationData.vertices.at(iNext).x + f2Offset.x, rNavigationData.vertices.at(iNext).y + f2Offset.y, fZ};
					DebugRender::Line(f3StartVertex, f3EndVertex, kf4PolygonColor);
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
	rMainLayout.fHexShieldEdgePower = engine::gHexShieldEdgePower.mfCurrent;
	rMainLayout.fHexShieldEdgeMultiplier = engine::gHexShieldEdgeMultiplier.mfCurrent;

	rMainLayout.fHexShieldWaveMultiplier = engine::gHexShieldWaveMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveDotMultiplier = engine::gHexShieldWaveDotMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveIntensityMultiplier = engine::gHexShieldWaveIntensityMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveIntensityPower = engine::gHexShieldWaveIntensityPower.mfCurrent;
	rMainLayout.fHexShieldWaveFalloffPower = engine::gHexShieldWaveFalloffPower.mfCurrent;

	rMainLayout.fHexShieldDirectionFalloffPower = engine::gHexShieldDirectionFalloffPower.mfCurrent;
	rMainLayout.fHexShieldDirectionMultiplier = engine::gHexShieldDirectionMultiplier.mfCurrent;
}

// The engine collections begin and end each render transaction ahead of the game's.
static void BeginFrameInterpolateRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	ExplosionsInterpolate::siTotalCount = 0;
	ForEachBeginRender(InterpolateTypes {}, iCommandBuffer, rRenderInterpolates, rActiveCoordinates);
	game::FrameInterpolate::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);
}

static void EndFrameInterpolateRender(int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount = ExplosionsInterpolate::siTotalCount;
	}
	ForEachEndRender(InterpolateTypes {}, iCommandBuffer);
	game::FrameInterpolate::EndRender(iCommandBuffer);
}

static void PopulateMainCamera(shaders::MainLayout& rMainLayout, int64_t iTick)
{
	rMainLayout.iFrameNumber = static_cast<int>(engine::gpCamera->miFrame);
	rMainLayout.iRenderNumber = static_cast<int32_t>(gpGraphics->miFrameCounter);

	LightingCameraPose comparisonPose;
	float fCameraShake = gpGraphics->mpLighting->GetComparisonCamera(comparisonPose) ? 0.0f : engine::gpCamera->mfShake;
	static constexpr float kfMaxRoll = 0.005f;
	static constexpr float kfMaxPitch = 0.005f;
	static constexpr float kfMaxYaw = 0.01f;
	// Fixed-seed permutation tables are identical every frame — construct once (each ctor reshuffles a 256-entry table).
	static const siv::BasicPerlinNoise<float> sPerlinRoll(0);
	static const siv::BasicPerlinNoise<float> sPerlinPitch(1);
	static const siv::BasicPerlinNoise<float> sPerlinYaw(2);
	float fShakeTime = common::NanosecondsToFloatSeconds<float>(TickToSimulationTime(iTick).time_since_epoch());
	auto matCameraShake = XMMatrixRotationRollPitchYaw(kfMaxRoll * fCameraShake * (-1.0f + 2.0f * sPerlinRoll.octave1D_01(8.0f * fShakeTime, 4)), kfMaxPitch * fCameraShake * (-1.0f + 2.0f * sPerlinPitch.octave1D_01(8.0f * fShakeTime, 4)), kfMaxYaw * fCameraShake * (-1.0f + 2.0f * sPerlinYaw.octave1D_01(8.0f * fShakeTime, 4)));

	XMMATRIX matViewProjection = XMMatrixTranspose(XMMatrixMultiply(engine::gpCamera->mMatView, XMMatrixMultiply(matCameraShake, engine::gpCamera->mMatPerspective)));
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ViewProjection[0]), matViewProjection);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ModelViewProjection[0]), matViewProjection);

	XMStoreFloat4(&rMainLayout.f4EyePosition, engine::gpCamera->mVecEyePosition);
	XMStoreFloat4(&rMainLayout.f4ModelEyePosition, engine::gpCamera->mVecEyePosition);
	XMVECTOR vecToEyeNormal = engine::gpCamera->mVecToEyeNormal;
	XMStoreFloat4(&rMainLayout.f4ToEyeNormal, vecToEyeNormal);
	// DebugRenderBillboard.vert consumes this basis for every vertex; forward remains f4ToEyeNormal.
	// The reference axis switches from +Z to +X at |forward.z| = 0.999; right is normalized and up = cross(forward, right) is not.
	XMVECTOR vecBillboardWorldUpNormal = std::abs(XMVectorGetZ(vecToEyeNormal)) < 0.999f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR vecBillboardRightNormal = XMVector3Normalize(XMVector3Cross(vecBillboardWorldUpNormal, vecToEyeNormal));
	XMStoreFloat4(&rMainLayout.f4BillboardRight, vecBillboardRightNormal);
	XMStoreFloat4(&rMainLayout.f4BillboardUp, XMVector3Cross(vecToEyeNormal, vecBillboardRightNormal));
}

void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, std::chrono::duration<float> currentTime)
{
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	RenderWaterMain(iCommandBuffer, currentTime);
	RenderLightingMain(iCommandBuffer);
	gpBufferManager->ResetSkinningAllocations(iCommandBuffer);

	if (rActiveCoordinates.empty() || !rRenderInterpolates.contains(cameraCoordinate))
	{
		gpPipelineManager->mpPipelines[kPipelineWater].WriteIndirectBuffer(iCommandBuffer, 0);
		// Active-set computation and reset include the client cell, and boot prerender seeds the origin; the empty-set check is defensive.
		// A missing camera interpolate can coexist with other renderable cells. Begin/end publication without per-cell Render clears indirect counts and per-transaction profile counters; collection BeginRender skips missing interpolates.
		// Return before accessing the absent camera interpolate.
		BeginFrameInterpolateRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);
		EndFrameInterpolateRender(iCommandBuffer);
		if constexpr (kbDebugRender)
		{
			DebugRender::BeginRender(iCommandBuffer);
			DebugRender::EndRender(iCommandBuffer);
		}
		PopulateMainCamera(rMainLayout, 0);
		return;
	}

	const game::FrameInterpolate& rCameraInterpolate = rRenderInterpolates.at(cameraCoordinate);

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
		gpGraphics->mpLighting->AddFrame(rFrameInterpolate);
		// Automatically dispatched main collections
		ExplosionsInterpolate::siTotalCount += rFrameInterpolate.explosions.iCount;
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

	// Phase 4: Game-specific debug rendering (positions from fully-interpolated frame)
	if constexpr (kbDebugRender)
	{
		game::FrameInterpolate::DebugRender(rRenderInterpolates, rActiveCoordinates);
	}

	DebugRenderNavigationData(rActiveCoordinates, cameraCoordinate);
	DebugRenderCellEdges(rActiveCoordinates, cameraCoordinate);
	DebugRenderIslandBoundaries(rActiveCoordinates, cameraCoordinate);
	DebugRenderIslandValidArea(rActiveCoordinates, cameraCoordinate);

	DebugRender::BeginRender(iCommandBuffer);
	DebugRender::EndRender(iCommandBuffer);

	PopulateMainCamera(rMainLayout, rCameraInterpolate.iTick);

	PopulateHexShield(rMainLayout);
}

} // namespace engine

#endif // defined(BT_CLIENT)
