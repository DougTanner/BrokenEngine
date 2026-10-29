#if defined(BT_CLIENT)

#include "LightingFixture.h"

#include "File/PackChunks.h"
#include "Graphics/AnimationData.h"

namespace engine
{
static float RadicalInverse(int64_t iValue, int64_t iBase)
{
	float fValue = 0.0f;
	float fWeight = 1.0f / static_cast<float>(iBase);
	while (iValue != 0)
	{
		fValue += static_cast<float>(iValue % iBase) * fWeight;
		iValue /= iBase;
		fWeight /= static_cast<float>(iBase);
	}
	return fValue;
}

LightingFixture::LightingFixture(GridCoord anchor)
: mAnchor(anchor)
{
	// Heap: immutable comparison inputs are created only at a drained fixture activation.
	ScopedSuppressAllocationTracking scopedSuppressAllocationTracking;
	common::RandomEngine random(kuiSeed);
	for (int64_t i = 0; i < kiSourceCount; ++i)
	{
		Source& rSource = mSources.at(i);
		rSource.f3Center = {common::Random(512.0f, random) - 256.0f, common::Random(512.0f, random) - 256.0f, std::array {6.0f, 50.0f, 100.0f, }.at(i % 3), };
		rSource.uiType = (i % 5 == 4) ? shaders::kiLightingSourceRectangle : shaders::kiLightingSourcePoint;
	}
	for (int64_t i = 0; i < kiSampleCount; ++i)
	{
		mSamples.at(i) = {RadicalInverse(i + 1, 2), RadicalInverse(i + 1, 3), };
	}
	mSolids =
	{{
		{.f3Center = {0.0f, 0.0f, -1.0f, }, .f3HalfExtent = {256.0f, 256.0f, 1.0f, }, },
		{.f3Center = {-80.0f, 0.0f, 6.0f, }, .f3HalfExtent = {8.0f, 8.0f, 0.5f, }, },
		{.f3Center = {0.0f, 0.0f, 50.0f, }, .f3HalfExtent = {8.0f, 8.0f, 0.5f, }, },
		{.f3Center = {80.0f, 0.0f, 100.0f, }, .f3HalfExtent = {8.0f, 8.0f, 0.5f, }, },
		{.f3Center = {0.0f, 40.0f, 40.0f, }, .f3HalfExtent = {80.0f, 1.0f, 40.0f, }, },
		{.f3Center = {-80.0f, 0.0f, 40.0f, }, .f3HalfExtent = {0.5f, 40.0f, 40.0f, }, },
		{.f3Center = {80.0f, 0.0f, 80.0f, }, .f3HalfExtent = {40.0f, 40.0f, 1.0f, }, },
		{.f3Center = {120.0f, 40.0f, 40.0f, }, .f3HalfExtent = {1.0f, 40.0f, 40.0f, }, .uiMaterial = 1, },
		{.f3Center = {-180.0f, 100.0f, 12.0f, }, .f3HalfExtent = {4.0f, 0.5f, 4.0f, }, },
		{.f3Center = {-160.0f, 100.0f, 12.0f, }, .f3HalfExtent = {4.0f, 4.0f, 0.5f, }, .f3AxisY = {0.0f, 0.70710677f, 0.70710677f, }, .f3AxisZ = {0.0f, -0.70710677f, 0.70710677f, }, },
		{.f3Center = {-140.0f, 100.0f, 12.0f, }, .f3HalfExtent = {4.0f, 4.0f, 0.5f, }, },
		{.f3Center = {-200.0f, -180.0f, 2.0f, }, .f3HalfExtent = {2.0f, 2.0f, 0.25f, }, },
		{.f3Center = {-220.0f, -180.0f, 4.0f, }, .f3HalfExtent = {1.0f, 2.0f, 0.25f, }, },
	}};
	mCameras.at(0) = MakeCamera({0.0f, -600.0f, 900.0f, }, {0.0f, 0.0f, 6.0f, });
	mCameras.at(1) = MakeCamera({0.0f, -4'000.0f, 6'000.0f, }, {0.0f, 0.0f, 6.0f, });
	for (int64_t i = 0; i < kiEventCount; ++i)
	{
		float fTime = static_cast<float>(i) / 119.0f;
		float fPan = i < 60 ? 0.0f : 900.0f * static_cast<float>(i - 59) / 60.0f;
		float fZoom = i < 60 ? static_cast<float>(i) / 59.0f : 1.0f;
		mEvents.at(i) =
		{
			.fTime = fTime,
			.fSourceHeight = 6.0f + 94.0f * fTime,
			.fOccluderOffset = -20.0f + 40.0f * fTime,
			.camera = MakeCamera({fPan, -600.0f - 3'400.0f * fZoom, 900.0f + 5'100.0f * fZoom, }, {fPan, 0.0f, 6.0f, }),
		};
	}
}

bool LightingFixture::InitializeAnimation()
{
	static constexpr std::string_view kPath = R"(Models\AlienCreatureTake3\scene.gltf)";
	static constexpr std::string_view kClip = "Animation.001";
	static constexpr common::crc_t kSceneCrc = common::CrcConsteval(kPath);
	const std::unordered_map<common::crc_t, EagerChunk>& rChunks = gpFileManager->mpPackChunks->GetEagerChunkMap();
	if (!gAnimationDataMap.contains(kSceneCrc) || !rChunks.contains(kSceneCrc))
	{
		return false;
	}
	const AnimationData& rData = gAnimationDataMap.at(kSceneCrc);
	const EagerChunk& rScene = rChunks.at(kSceneCrc);
	common::crc_t modelCrc = rScene.pHeader->sceneHeader.modelCrc;
	if (!gpBufferManager->mModelMap.contains(modelCrc) || !rChunks.contains(modelCrc))
	{
		return false;
	}
	if (rData.mHeader.skeleton.uiSkinJointCount == 0)
	{
		return false;
	}
	if (rData.mHeader.uiAnimationCount == 0)
	{
		return false;
	}
	if (std::strncmp(rData.mpAnimations[0].pcName, kClip.data(), common::AnimationClip::kiMaxNameLength) != 0)
	{
		return false;
	}
	mAnimation.sceneCrc = kSceneCrc;
	mAnimation.iClip = 0;
	return true;
}

void LightingFixture::WriteAnimation(int64_t iEvent, std::span<common::MeshData> meshData, common::JointMatrix* pJointMatrices, int64_t iJointOffset) const
{
	gAnimationDataMap.at(mAnimation.sceneCrc).EvaluateAnimation(mAnimation.iClip, mEvents.at(iEvent).fTime, meshData, pJointMatrices, iJointOffset);
}

LightingCameraPose LightingFixture::MakeCamera(XMFLOAT3A f3Eye, XMFLOAT3A f3Target) const
{
	GridCoord coordinate = mAnchor;
	XMVECTOR vecTarget = XMVectorSetW(XMLoadFloat3A(&f3Target), 1.0f);
	while (!IsInsideCell(vecTarget))
	{
		const common::HexDirection& rDirection = kCellDirections.pEntries[CellExitDirection(vecTarget)];
		coordinate.iQ += rDirection.iDeltaQ;
		coordinate.iR += rDirection.iDeltaR;
		vecTarget = XMVectorSubtract(vecTarget, XMVectorSet(rDirection.f2CenterOffset.x, rDirection.f2CenterOffset.y, 0.0f, 0.0f));
	}
	LightingCameraPose pose;
	pose.coordinate = coordinate;
	XMStoreFloat3A(&pose.f3Eye, Rebase(MakeRenderBasis(mAnchor, coordinate), XMVectorSetW(XMLoadFloat3A(&f3Eye), 1.0f)));
	XMStoreFloat3A(&pose.f3Target, vecTarget);
	pose.fVerticalFieldOfView = XM_PIDIV4;
	pose.fNear = 0.1f;
	pose.fFar = 20'000.0f;
	return pose;
}

LightingFixtureSolid LightingFixture::GetSolid(int64_t iSolid, int64_t iEvent, LightingFixtureScenario eScenario) const
{
	LightingFixtureSolid solid = mSolids.at(iSolid);
	if (iSolid == kiMovingPanel || iSolid == kiMovingOccluder)
	{
		const LightingFixtureEvent& rEvent = mEvents.at(iEvent);
		XMFLOAT3A f3Source {};
		static_cast<XMFLOAT3&>(f3Source) = eScenario == LightingFixtureScenario::kDensity ? mSources.at(0).f3Center : XMFLOAT3 {-200.0f, -180.0f, 6.0f, };
		solid.f3Center = {f3Source.x, f3Source.y, rEvent.fSourceHeight - (iSolid == kiMovingPanel ? 4.0f : 2.0f), };
		if (iSolid == kiMovingOccluder)
		{
			solid.f3Center.x += rEvent.fOccluderOffset;
		}
	}
	return solid;
}

XMFLOAT3A LightingFixture::GetAlbedo(int64_t iMaterial)
{
	return iMaterial == 0 ? XMFLOAT3A {0.5f, 0.5f, 0.5f, } : XMFLOAT3A {0.8f, 0.1f, 0.1f, };
}

void LightingFixture::WriteSolidGeometry(const LightingFixtureSolid& rSolid, std::span<common::ModelVertex> vertices, std::span<uint32_t> indices)
{
	static constexpr int64_t kpiAxes[6][3] {{1, 2, 0, }, {2, 1, 0, }, {2, 0, 1, }, {0, 2, 1, }, {0, 1, 2, }, {1, 0, 2, }, };
	static constexpr float kpfCorners[4][2] {{-1.0f, -1.0f, }, {1.0f, -1.0f, }, {1.0f, 1.0f, }, {-1.0f, 1.0f, }, };
	static constexpr uint32_t kpuiIndices[6] {0, 1, 2, 0, 2, 3, };
	float pfExtent[3] {rSolid.f3HalfExtent.x, rSolid.f3HalfExtent.y, rSolid.f3HalfExtent.z, };
	for (int64_t i = 0; i < 6; ++i)
	{
		float fSign = i % 2 == 0 ? 1.0f : -1.0f;
		for (int64_t j = 0; j < 4; ++j)
		{
			common::ModelVertex vertex;
			float pfPosition[3] {};
			float pfNormal[3] {};
			pfPosition[kpiAxes[i][0]] = kpfCorners[j][0] * pfExtent[kpiAxes[i][0]];
			pfPosition[kpiAxes[i][1]] = kpfCorners[j][1] * pfExtent[kpiAxes[i][1]];
			pfPosition[kpiAxes[i][2]] = fSign * pfExtent[kpiAxes[i][2]];
			pfNormal[kpiAxes[i][2]] = fSign;
			vertex.f3Pos = {pfPosition[0], pfPosition[1], pfPosition[2], };
			vertex.f3Normal = {pfNormal[0], pfNormal[1], pfNormal[2], };
			vertex.f2Uv = {(kpfCorners[j][0] + 1.0f) * 0.5f, (kpfCorners[j][1] + 1.0f) * 0.5f, };
			vertices[i * 4 + j] = vertex;
		}
		for (int64_t j = 0; j < 6; ++j)
		{
			indices[i * 6 + j] = static_cast<uint32_t>(i * 4) + kpuiIndices[j];
		}
	}
}

void LightingFixture::WriteSources(std::span<shaders::LightingSource> sources, int64_t iEvent, GridCoord cameraCoordinate, LightingFixtureScenario eScenario) const
{
	RenderBasis basis = MakeRenderBasis(mAnchor, cameraCoordinate);
	for (int64_t i = 0; i < std::ssize(sources); ++i)
	{
		const Source& rSource = mSources.at(i);
		XMFLOAT3A f3Center {};
		static_cast<XMFLOAT3&>(f3Center) = eScenario == LightingFixtureScenario::kDensity ? rSource.f3Center : XMFLOAT3 {-200.0f, -180.0f, 6.0f, };
		if (i == 0)
		{
			f3Center.z = mEvents.at(iEvent).fSourceHeight;
		}
		uint32_t uiType = eScenario == LightingFixtureScenario::kDensity ? rSource.uiType : (eScenario == LightingFixtureScenario::kIsolatedPoint ? shaders::kiLightingSourcePoint : shaders::kiLightingSourceRectangle);
		shaders::LightingSource source {};
		static_cast<XMFLOAT3&>(source.f3Center) = {f3Center.x + basis.f2Offset.x, f3Center.y + basis.f2Offset.y, f3Center.z, };
		source.fRange = shaders::kfLightingSourceRange;
		source.uiType = uiType;
		if (uiType == shaders::kiLightingSourceRectangle)
		{
			static_cast<XMFLOAT3&>(source.f3HalfEdgeU) = {2.0f, 0.0f, 0.0f, };
			static_cast<XMFLOAT3&>(source.f3HalfEdgeV) = {0.0f, -2.0f, 0.0f, };
		}
		static_cast<XMFLOAT3&>(source.f3Power) = {1'000.0f, 1'000.0f, 1'000.0f, };
		sources[i] = source;
	}
}

void LightingFixture::WriteSamples(std::span<XMFLOAT2> samples) const
{
	std::copy_n(mSamples.begin(), std::ssize(samples), samples.begin());
}

} // namespace engine

#endif // defined(BT_CLIENT)
