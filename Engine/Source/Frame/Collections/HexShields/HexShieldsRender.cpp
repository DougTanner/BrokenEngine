#include "HexShields.h"

#if defined(BT_CLIENT)

#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileRows.h"

namespace engine
{

constexpr float kfAdjust = 20.0f;
constexpr int64_t kiDynamicBufferInitialCapacity = 256;

void HexShieldsInterpolate::GraphicsResources()
{
	// One buffer per lighting layer, shared by that layer's main and lighting pipelines.
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		common::crc_t uiLayerKey = DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i);
		gpBufferManager->CreateDynamicBuffer(uiLayerKey, kBufferMain, kpcName, sizeof(shaders::HexShieldLayout), kiDynamicBufferInitialCapacity);
		gpPipelineManager->mDynamicPipelines.CreatePipelineHexShields(uiLayerKey, kpcName);
		gpPipelineManager->mDynamicPipelines.CreatePipelineHexShieldsLighting(uiLayerKey, kpcName);
	}
}

static int64_t spiRendered[shaders::kiLightingLayers] {}; // Cursor per lighting layer
static int64_t siTotalCount = 0;

void HexShieldsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	std::ranges::fill(spiRendered, 0);
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const HexShieldsInterpolate&
	{
		return rInterpolate.hexShields;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	// Each shield goes to one layer, and any layer may receive every shield, so every layer's buffer takes the full capacity.
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		common::crc_t uiLayerKey = DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i);
		if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(uiLayerKey, kBufferMain, kpcName, sizeof(shaders::HexShieldLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
		{
			PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineHexShields].at(uiLayerKey), iCommandBuffer, 2, pBuffer);
			PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineHexShieldsLighting].at(uiLayerKey), iCommandBuffer, 2, pBuffer);
		}
	}
}

void HexShieldsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const HexShieldsInterpolate& rCurrent = rFrameInterpolate.hexShields;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	siTotalCount += rCurrent.iCount;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	shaders::HexShieldLayout* ppLayouts[shaders::kiLightingLayers] {};
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		auto [pLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::HexShieldLayout>(DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i), kBufferMain, iCommandBuffer);
		ASSERT(spiRendered[i] + rCurrent.iCount <= iBufferCapacity);
		ppLayouts[i] = pLayouts;
	}

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Rebase the cell-local position once into the camera cell for all subsequent uses.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[i];
		const HexShieldsType& rType = sTypes.at(rCurrent.puiTypeIndices[i]);

		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, Rebase(rBasis, vecLocalPosition));
		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, f4Position, kfAdjust, kfAdjust, kfAdjust, kfAdjust))
		{
			continue;
		}

		// The shield deposits into the lighting layer of the flight plane nearest it.
		int64_t iLayer = gpFlightPlanes->NearestPlaneIndex(XMVectorGetZ(vecLocalPosition));
		shaders::HexShieldLayout& rLayout = ppLayouts[iLayer][spiRendered[iLayer]];
		rLayout.f4Position = f4Position;
		rLayout.f3x4Transform[0] = rCurrent.pf4Transforms[0][i];
		rLayout.f3x4Transform[1] = rCurrent.pf4Transforms[1][i];
		rLayout.f3x4Transform[2] = rCurrent.pf4Transforms[2][i];
		rLayout.f3x4TransformNormal[0] = rCurrent.pf4TransformNormals[0][i];
		rLayout.f3x4TransformNormal[1] = rCurrent.pf4TransformNormals[1][i];
		rLayout.f3x4TransformNormal[2] = rCurrent.pf4TransformNormals[2][i];

		// Convert packed color to vec4 (ABGR to RGBA float)
		uint32_t uiColor = rType.uiColor;
		rLayout.f4Color.x = static_cast<float>((uiColor >> 0) & 0xFF) / 255.0f;
		rLayout.f4Color.y = static_cast<float>((uiColor >> 8) & 0xFF) / 255.0f;
		rLayout.f4Color.z = static_cast<float>((uiColor >> 16) & 0xFF) / 255.0f;
		rLayout.f4Color.w = static_cast<float>((uiColor >> 24) & 0xFF) / 255.0f;

		uint32_t uiLightingColor = rType.uiLightingColor;
		rLayout.f4LightingColor.x = static_cast<float>((uiLightingColor >> 0) & 0xFF) / 255.0f;
		rLayout.f4LightingColor.y = static_cast<float>((uiLightingColor >> 8) & 0xFF) / 255.0f;
		rLayout.f4LightingColor.z = static_cast<float>((uiLightingColor >> 16) & 0xFF) / 255.0f;
		rLayout.f4LightingColor.w = static_cast<float>((uiLightingColor >> 24) & 0xFF) / 255.0f;

		for (int64_t j = 0; j < shaders::kiHexShieldDirections; ++j)
		{
			rLayout.pf4Directions[j] = rCurrent.pf4Directions[j][i];
			rLayout.pfVertexIntensities[j] = rCurrent.pfVertexIntensities[j][i];
			rLayout.pfFragmentIntensities[j] = rCurrent.pfFragmentIntensities[j][i];
		}

		rLayout.fLightingIntensity = rCurrent.pfLightingIntensities[i];
		rLayout.fSize = rCurrent.pfSizes[i];
		rLayout.fColorMix = rCurrent.pfColorMixes[i];
		rLayout.fMinimumIntensity = rType.fMinimumIntensity;

		++spiRendered[iLayer];
	}
}

void HexShieldsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterHexShields).iCount = siTotalCount;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterHexShieldsRendered).iCount = std::ranges::fold_left(spiRendered, 0i64, std::plus {});
	}
	// Every layer's shields draw in the main pass; only the refreshing lighting layer deposits.
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		common::crc_t uiLayerKey = DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i);
		gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineHexShields].at(uiLayerKey)->WriteIndirectBuffer(iCommandBuffer, spiRendered[i]);
		gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineHexShieldsLighting].at(uiLayerKey)->WriteIndirectBuffer(iCommandBuffer, gLightingEnabled.Get<bool>() && i == giLightingRefreshLayer ? spiRendered[i] : 0);
	}
}

} // namespace engine

#endif // BT_CLIENT
