#pragma once

#if defined(BT_CLIENT)

#include "Lighting.h"
#include "LightingAccelerationStructure.h"
#include "LightingFixture.h"
#include "LightingModels.h"
#include "Terrain.h"

namespace engine
{

struct LightingModelRange
{
	common::crc_t modelCrc = 0;
	common::crc_t instanceBufferCrc = 0;
	int64_t iFirstInstance = 0;
	int64_t iInstanceCount = 0;
};

struct LightingFramebuffer
{
	Buffer sources;
	Buffer publication;
	Buffer instances;
	Buffer geometries;
	Buffer accelerationInstances;
	Buffer samples;
	Buffer fixtureMaterials;
	LightingAccelerationStructure scene;
	uint32_t uiInstanceCount = 0;
	uint64_t uiGeneration = 0;
	uint64_t uiPendingPublication = 0;
	uint64_t uiRecordedPublication = 0;
	uint64_t uiBuiltSceneGeneration = std::numeric_limits<uint64_t>::max();
	uint32_t uiFixtureFirstInstance = 0;
	uint32_t uiFixtureFirstGeometry = 0;
	uint32_t uiFixtureAnimationInstance = std::numeric_limits<uint32_t>::max();
	std::vector<LightingModelRange> modelRanges;
	LightingModelCounts modelCounts;
	LightingTerrainCounts terrainCounts;
	// Counts only; BeginRender latches them once the covering fence has completed.
	LightingDiagnostics pendingDiagnostics;
};

struct Lighting::State
{
	struct Comparison
	{
		static constexpr VkExtent3D kVkExtent {1'920, 1'080, 1};
		// One reference dispatch traces a 4x4-pixel tile; 120 per frame is a quarter tile row, so a capture traces over 1080 frames.
		static constexpr int64_t kiTileColumns = kVkExtent.width / 4;
		static constexpr int64_t kiTileCount = kiTileColumns * (kVkExtent.height / 4);
		static constexpr int64_t kiTilesPerFrame = 120;
		std::array<Texture, 4> receivers;
		Texture depth;
		Texture reference;
		VkRenderPass vkRenderPass = VK_NULL_HANDLE;
		VkFramebuffer vkFramebuffer = VK_NULL_HANDLE;
		Pipeline compute;
		Pipeline terrain;
		Pipeline water;
		Pipeline solidReceiver;
		Pipeline solidForward;
		Buffer solidDraws;
		using ModelKey = std::tuple<common::crc_t, common::crc_t, int64_t>;
		struct ModelKeyHash
		{
			std::size_t operator()(const ModelKey& rKey) const
			{
				return std::hash<common::crc_t> {}(std::get<0>(rKey)) ^ (std::hash<common::crc_t> {}(std::get<1>(rKey)) << 1) ^ (std::hash<int64_t> {}(std::get<2>(rKey)) << 2);
			}
		};
		std::unordered_map<ModelKey, std::unique_ptr<ModelPipeline>, ModelKeyHash> models;
		std::unique_ptr<ModelPipeline> animationForward;
		TextureLayout eReferenceLayout = TextureLayout::kGeneral;
	};
	std::unique_ptr<Comparison> comparison;
	struct FixtureSolid
	{
		Buffer vertices;
		Buffer indices;
		LightingAccelerationStructure acceleration;
		bool bBuilt = false;
	};
	static constexpr common::crc_t kFixtureAnimationBuffer = common::CrcConsteval("LightingFixtureAnimation");
	std::vector<std::unique_ptr<FixtureSolid>> fixtureSolids;
	LightingTerrain terrain;
	std::unique_ptr<LightingModels> models;
	std::unique_ptr<LightingFixture> fixture;
	std::vector<std::unique_ptr<LightingFramebuffer>> framebuffers;
	int64_t iFramebuffer = 0;
	LightingComparisonSettings settings;
	uint64_t uiPublication = 0;
	uint64_t uiResourceGeneration = 0;
	uint64_t uiServicedCapture = 0;
	std::chrono::steady_clock::time_point captureStart;
	// Written by the receivers frame; every tracing frame must publish the same counts.
	shaders::LightingPublication capturePublication {};
};

} // namespace engine

#endif // defined(BT_CLIENT)
