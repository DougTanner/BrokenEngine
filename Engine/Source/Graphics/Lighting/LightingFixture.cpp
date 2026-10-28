#if defined(BT_CLIENT)

#include "LightingFixture.h"

#include "File/PackChunks.h"
#include "Graphics/AnimationData.h"

namespace engine
{
static void AppendU32(std::vector<std::byte>& rBytes, uint32_t uiValue)
{
	for (int64_t i = 0; i < 4; ++i)
	{
		rBytes.push_back(static_cast<std::byte>((uiValue >> (8 * i)) & 0xFF));
	}
}

static void AppendU64(std::vector<std::byte>& rBytes, uint64_t uiValue)
{
	AppendU32(rBytes, static_cast<uint32_t>(uiValue));
	AppendU32(rBytes, static_cast<uint32_t>(uiValue >> 32));
}

static void AppendFloat(std::vector<std::byte>& rBytes, float fValue)
{
	AppendU32(rBytes, std::bit_cast<uint32_t>(fValue));
}

static void AppendVector(std::vector<std::byte>& rBytes, const XMFLOAT3A& rValue)
{
	AppendFloat(rBytes, rValue.x);
	AppendFloat(rBytes, rValue.y);
	AppendFloat(rBytes, rValue.z);
}

static void AppendCamera(std::vector<std::byte>& rBytes, const LightingCameraPose& rCamera)
{
	AppendU32(rBytes, static_cast<uint32_t>(rCamera.coordinate.iQ));
	AppendU32(rBytes, static_cast<uint32_t>(rCamera.coordinate.iR));
	AppendVector(rBytes, rCamera.f3Eye);
	AppendVector(rBytes, rCamera.f3Target);
	AppendVector(rBytes, rCamera.f3Up);
	AppendFloat(rBytes, rCamera.fVerticalFieldOfView);
	AppendFloat(rBytes, rCamera.fNear);
	AppendFloat(rBytes, rCamera.fFar);
}

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

static std::array<uint8_t, 32> HashBytes(std::span<const std::byte> bytes)
{
	common::Sha256Hasher hasher;
	// CNG's input pointer is mutable in its API; hashing does not write the bytes.
	hasher.Update(std::span(const_cast<std::byte*>(bytes.data()), bytes.size()));
	std::array<uint8_t, 32> digest {};
	if (!hasher.Finish(digest))
	{
		throw std::runtime_error("Lighting fixture SHA-256 failed");
	}
	return digest;
}

static void AppendString(std::vector<std::byte>& rBytes, std::string_view text)
{
	AppendU32(rBytes, static_cast<uint32_t>(text.size()));
	for (char cChar : text)
	{
		rBytes.push_back(static_cast<std::byte>(cChar));
	}
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
	const std::array<size_t, 6> blobSizes
	{
		4 * (17 + 4 * kiSourceCount),
		4 * (3 + 2 * kiSampleCount),
		4 * (13 + kiSolidCount * (17 + 25 * kiVerticesPerSolid + kiIndicesPerSolid)),
		4 * (4 + 14 * mCameras.size()),
		4 * (3 + 18 * kiEventCount),
		8,
	};
	for (int64_t i = 0; i < 6; ++i)
	{
		std::vector<std::byte>& rBytes = mBlobs.at(i);
		rBytes.reserve(blobSizes.at(i));
		AppendU32(rBytes, kuiSchema);
		AppendU32(rBytes, static_cast<uint32_t>(i));
	}
	std::vector<std::byte>& rSources = mBlobs.at(0);
	AppendU32(rSources, kuiSeed);
	AppendU32(rSources, kiSourceCount);
	for (const Source& rSource : mSources)
	{
		AppendFloat(rSources, rSource.f3Center.x);
		AppendFloat(rSources, rSource.f3Center.y);
		AppendFloat(rSources, rSource.f3Center.z);
		AppendU32(rSources, rSource.uiType);
	}
	AppendFloat(rSources, 1'000.0f);
	AppendFloat(rSources, shaders::kfLightingSourceRange);
	AppendVector(rSources, {2.0f, 0.0f, 0.0f, });
	AppendVector(rSources, {0.0f, -2.0f, 0.0f, });
	AppendFloat(rSources, shaders::kfLightingDistanceFloor);
	AppendFloat(rSources, shaders::kfLightingRayOffset);
	AppendVector(rSources, {-200.0f, -180.0f, 6.0f, });
	std::vector<std::byte>& rSamples = mBlobs.at(1);
	AppendU32(rSamples, kiSampleCount);
	for (const XMFLOAT2& rSample : mSamples)
	{
		AppendFloat(rSamples, rSample.x);
		AppendFloat(rSamples, rSample.y);
	}
	std::vector<std::byte>& rGeometry = mBlobs.at(2);
	AppendU32(rGeometry, kiSolidCount);
	for (const LightingFixtureSolid& rSolid : mSolids)
	{
		AppendVector(rGeometry, rSolid.f3Center);
		AppendVector(rGeometry, rSolid.f3HalfExtent);
		AppendVector(rGeometry, rSolid.f3AxisX);
		AppendVector(rGeometry, rSolid.f3AxisY);
		AppendVector(rGeometry, rSolid.f3AxisZ);
		AppendU32(rGeometry, rSolid.uiMaterial);
		AppendU32(rGeometry, 0);
		std::array<common::ModelVertex, kiVerticesPerSolid> vertices {};
		std::array<uint32_t, kiIndicesPerSolid> indices {};
		WriteSolidGeometry(rSolid, vertices, indices);
		for (const common::ModelVertex& rVertex : vertices)
		{
			AppendFloat(rGeometry, rVertex.f3Pos.x);
			AppendFloat(rGeometry, rVertex.f3Pos.y);
			AppendFloat(rGeometry, rVertex.f3Pos.z);
			AppendFloat(rGeometry, rVertex.f3Normal.x);
			AppendFloat(rGeometry, rVertex.f3Normal.y);
			AppendFloat(rGeometry, rVertex.f3Normal.z);
			AppendFloat(rGeometry, rVertex.f2Uv.x);
			AppendFloat(rGeometry, rVertex.f2Uv.y);
			for (int64_t i = 0; i < 17; ++i)
			{
				AppendU32(rGeometry, 0);
			}
		}
		for (uint32_t uiIndex : indices)
		{
			AppendU32(rGeometry, uiIndex);
		}
	}
	AppendVector(rGeometry, GetAlbedo(0));
	AppendVector(rGeometry, GetAlbedo(1));
	AppendU32(rGeometry, kiMovingPanel);
	AppendU32(rGeometry, kiMovingOccluder);
	AppendFloat(rGeometry, -4.0f);
	AppendFloat(rGeometry, -2.0f);
	std::vector<std::byte>& rCameras = mBlobs.at(3);
	AppendU32(rCameras, static_cast<uint32_t>(anchor.iQ));
	AppendU32(rCameras, static_cast<uint32_t>(anchor.iR));
	for (const LightingCameraPose& rCamera : mCameras)
	{
		AppendCamera(rCameras, rCamera);
	}
	std::vector<std::byte>& rEvents = mBlobs.at(4);
	AppendU32(rEvents, kiEventCount);
	for (const LightingFixtureEvent& rEvent : mEvents)
	{
		AppendFloat(rEvents, rEvent.fTime);
		AppendFloat(rEvents, rEvent.fSourceHeight);
		AppendFloat(rEvents, rEvent.fOccluderOffset);
		AppendU32(rEvents, 0);
		AppendCamera(rEvents, rEvent.camera);
	}
	for (int64_t i = 0; i < 5; ++i)
	{
		FreezeBlob(static_cast<LightingFixtureBlob>(i));
	}
}

bool LightingFixture::InitializeAnimation()
{
	// Heap: the fixed asset identity is serialized before fixture publication.
	ScopedSuppressAllocationTracking scopedSuppressAllocationTracking;
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
	mAnimation.iMaterialCount = rData.miMaterialCount;
	mAnimation.iJointsPerMaterial = rData.mHeader.skeleton.uiSkinJointCount;
	mAnimation.path = kPath;
	mAnimation.clip = kClip;
	std::vector<std::byte>& rBytes = mBlobs.at(5);
	rBytes.reserve(128 + mAnimation.path.size() + mAnimation.clip.size());
	rBytes.resize(8);
	AppendU64(rBytes, kSceneCrc);
	AppendU64(rBytes, modelCrc);
	AppendU32(rBytes, 0);
	AppendU32(rBytes, static_cast<uint32_t>(mAnimation.iMaterialCount));
	AppendU32(rBytes, static_cast<uint32_t>(mAnimation.iJointsPerMaterial));
	AppendU32(rBytes, 0);
	AppendVector(rBytes, mAnimation.f3Center);
	AppendFloat(rBytes, mAnimation.fScale);
	AppendString(rBytes, mAnimation.path);
	AppendString(rBytes, mAnimation.clip);
	const EagerChunk& rModel = rChunks.at(modelCrc);
	for (const EagerChunk* pChunk : {&rScene, &rModel, })
	{
		std::array<uint8_t, 32> digest = HashBytes(std::span(pChunk->pData, static_cast<size_t>(pChunk->iDataSize)));
		for (uint8_t uiByte : digest)
		{
			rBytes.push_back(static_cast<std::byte>(uiByte));
		}
	}
	FreezeBlob(LightingFixtureBlob::kAnimation);
	return true;
}

const LightingFixtureAnimation& LightingFixture::GetAnimation() const
{
	return mAnimation;
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

GridCoord LightingFixture::GetAnchor() const
{
	return mAnchor;
}

LightingCameraPose LightingFixture::GetCamera(LightingComparisonCamera eCamera) const
{
	return mCameras.at(static_cast<int64_t>(eCamera));
}

const LightingFixtureEvent& LightingFixture::GetEvent(int64_t iEvent) const
{
	return mEvents.at(iEvent);
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

void LightingFixture::FreezeBlob(LightingFixtureBlob eBlob)
{
	int64_t i = static_cast<int64_t>(eBlob);
	mIdentities.at(i) = {.uiBytes = static_cast<uint64_t>(std::ssize(mBlobs.at(i))), .sha256 = HashBytes(mBlobs.at(i)), };
}

std::span<const std::byte> LightingFixture::GetBlob(LightingFixtureBlob eBlob) const
{
	return mBlobs.at(static_cast<int64_t>(eBlob));
}

const LightingFixtureBlobIdentity& LightingFixture::GetIdentity(LightingFixtureBlob eBlob) const
{
	return mIdentities.at(static_cast<int64_t>(eBlob));
}

std::array<uint8_t, 32> LightingFixture::GetInputHash(const LightingComparisonSettings& rSettings, LightingFixtureScenario eScenario, bool bEventCamera) const
{
	std::array<std::byte, 232> bytes {};
	int64_t iOffset = 0;
	for (const LightingFixtureBlobIdentity& rIdentity : mIdentities)
	{
		std::memcpy(bytes.data() + iOffset, rIdentity.sha256.data(), rIdentity.sha256.size());
		iOffset += 32;
	}
	uint32_t uiSources = eScenario == LightingFixtureScenario::kDensity ? static_cast<uint32_t>(rSettings.iSourceCount) : 1;
	uint32_t puiFields[10] {kuiSchema, static_cast<uint32_t>(eScenario), uiSources, static_cast<uint32_t>(rSettings.eCamera), static_cast<uint32_t>(rSettings.iEvent), static_cast<uint32_t>(rSettings.iAreaSamples), static_cast<uint32_t>(bEventCamera), static_cast<uint32_t>(rSettings.eMode), 0, 0, };
	for (uint32_t uiField : puiFields)
	{
		for (int64_t i = 0; i < 4; ++i)
		{
			bytes.at(iOffset++) = static_cast<std::byte>((uiField >> (8 * i)) & 0xFF);
		}
	}
	return HashBytes(bytes);
}

} // namespace engine

#endif // defined(BT_CLIENT)
