#pragma once

#if defined(BT_CLIENT)

#include "Lighting.h"

namespace engine
{

struct LightingFixtureSolid
{
	XMFLOAT3A f3Center {};
	XMFLOAT3A f3HalfExtent {};
	XMFLOAT3A f3AxisX {1.0f, 0.0f, 0.0f, };
	XMFLOAT3A f3AxisY {0.0f, 1.0f, 0.0f, };
	XMFLOAT3A f3AxisZ {0.0f, 0.0f, 1.0f, };
	uint32_t uiMaterial = 0;
};

struct LightingFixtureEvent
{
	float fTime = 0.0f;
	float fSourceHeight = 6.0f;
	float fOccluderOffset = -20.0f;
	LightingCameraPose camera;
};

struct LightingFixtureAnimation
{
	common::crc_t sceneCrc = 0;
	int64_t iClip = 0;
	XMFLOAT3A f3Center {-160.0f, 160.0f, 16.0f, };
	float fScale = 8.0f;
};

class LightingFixture
{
public:
	static constexpr int64_t kiSourceCount = 1'000;
	static constexpr int64_t kiSampleCount = 512;
	static constexpr int64_t kiEventCount = 120;
	static constexpr int64_t kiSolidCount = 13;
	static constexpr int64_t kiVerticesPerSolid = 24;
	static constexpr int64_t kiIndicesPerSolid = 36;
	static constexpr int64_t kiMovingPanel = 11;
	static constexpr int64_t kiMovingOccluder = 12;
	static constexpr uint32_t kuiSeed = 0x51A7;

	explicit LightingFixture(GridCoord anchor);
	bool InitializeAnimation();
	void WriteAnimation(int64_t iEvent, std::span<common::MeshData> meshData, common::JointMatrix* pJointMatrices, int64_t iJointOffset) const;
	LightingFixtureSolid GetSolid(int64_t iSolid, int64_t iEvent, LightingFixtureScenario eScenario) const;
	static XMFLOAT3A GetAlbedo(int64_t iMaterial);
	static void WriteSolidGeometry(const LightingFixtureSolid& rSolid, std::span<common::ModelVertex> vertices, std::span<uint32_t> indices);
	void WriteSources(std::span<shaders::LightingSource> sources, int64_t iEvent, GridCoord cameraCoordinate, LightingFixtureScenario eScenario) const;
	void WriteSamples(std::span<XMFLOAT2> samples) const;

	GridCoord mAnchor {};
	std::array<LightingCameraPose, 2> mCameras {};
	std::array<LightingFixtureEvent, kiEventCount> mEvents {};
	LightingFixtureAnimation mAnimation;

private:
	struct Source
	{
		XMFLOAT3 f3Center {};
		uint32_t uiType = 0;
	};

	LightingCameraPose MakeCamera(XMFLOAT3A f3Eye, XMFLOAT3A f3Target) const;
	std::array<Source, kiSourceCount> mSources {};
	std::array<XMFLOAT2, kiSampleCount> mSamples {};
	std::array<LightingFixtureSolid, kiSolidCount> mSolids {};
};

} // namespace engine

#endif // defined(BT_CLIENT)
