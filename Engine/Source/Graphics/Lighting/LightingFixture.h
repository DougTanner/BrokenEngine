#pragma once

#if defined(BT_CLIENT)

#include "Lighting.h"

namespace engine
{

enum class LightingFixtureBlob
{
	kSources,
	kSamples,
	kGeometry,
	kCameras,
	kEvents,
	kAnimation,
};

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

struct LightingFixtureBlobIdentity
{
	uint64_t uiBytes = 0;
	std::array<uint8_t, 32> sha256 {};
};

struct LightingFixtureAnimation
{
	common::crc_t sceneCrc = 0;
	int64_t iClip = 0;
	int64_t iMaterialCount = 0;
	int64_t iJointsPerMaterial = 0;
	XMFLOAT3A f3Center {-160.0f, 160.0f, 16.0f, };
	float fScale = 8.0f;
	std::string path;
	std::string clip;
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
	static constexpr uint32_t kuiSchema = 1;
	static constexpr uint32_t kuiSeed = 0x51A7;

	explicit LightingFixture(GridCoord anchor);
	bool InitializeAnimation();
	const LightingFixtureAnimation& GetAnimation() const;
	void WriteAnimation(int64_t iEvent, std::span<common::MeshData> meshData, common::JointMatrix* pJointMatrices, int64_t iJointOffset) const;
	GridCoord GetAnchor() const;
	LightingCameraPose GetCamera(LightingComparisonCamera eCamera) const;
	const LightingFixtureEvent& GetEvent(int64_t iEvent) const;
	LightingFixtureSolid GetSolid(int64_t iSolid, int64_t iEvent, LightingFixtureScenario eScenario) const;
	static XMFLOAT3A GetAlbedo(int64_t iMaterial);
	static void WriteSolidGeometry(const LightingFixtureSolid& rSolid, std::span<common::ModelVertex> vertices, std::span<uint32_t> indices);
	void WriteSources(std::span<shaders::LightingSource> sources, int64_t iEvent, GridCoord cameraCoordinate, LightingFixtureScenario eScenario) const;
	void WriteSamples(std::span<XMFLOAT2> samples) const;
	std::span<const std::byte> GetBlob(LightingFixtureBlob eBlob) const;
	const LightingFixtureBlobIdentity& GetIdentity(LightingFixtureBlob eBlob) const;
	std::array<uint8_t, 32> GetInputHash(const LightingComparisonSettings& rSettings, LightingFixtureScenario eScenario, bool bEventCamera) const;

private:
	struct Source
	{
		XMFLOAT3 f3Center {};
		uint32_t uiType = 0;
	};

	LightingCameraPose MakeCamera(XMFLOAT3A f3Eye, XMFLOAT3A f3Target) const;
	void FreezeBlob(LightingFixtureBlob eBlob);
	GridCoord mAnchor {};
	std::array<Source, kiSourceCount> mSources {};
	std::array<XMFLOAT2, kiSampleCount> mSamples {};
	std::array<LightingFixtureSolid, kiSolidCount> mSolids {};
	std::array<LightingCameraPose, 2> mCameras {};
	std::array<LightingFixtureEvent, kiEventCount> mEvents {};
	LightingFixtureAnimation mAnimation;
	std::array<std::vector<std::byte>, 6> mBlobs;
	std::array<LightingFixtureBlobIdentity, 6> mIdentities {};
};

} // namespace engine

#endif // defined(BT_CLIENT)
