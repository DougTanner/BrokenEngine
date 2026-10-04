#include "HexShields.h"

#if defined(BT_CLIENT)

#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

constexpr float kfAdjust = 20.0f;

void HexShieldsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::HexShieldLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineHexShields(kuiCrc, kpcName, sizeof(shaders::HexShieldLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineHexShieldsLighting(kuiCrc, kpcName);
}

static int64_t siRendered = 0;
static int64_t siTotalCount = 0;

void HexShieldsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const HexShieldsInterpolate&
	{
		return rInterpolate.hexShields;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferMain, kpcName, sizeof(shaders::HexShieldLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineHexShields].at(kuiCrc), iCommandBuffer, 2, pBuffer);
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineHexShieldsLighting].at(kuiCrc), iCommandBuffer, 2, pBuffer);
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

	auto [pLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::HexShieldLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

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

		shaders::HexShieldLayout& rLayout = pLayouts[siRendered];
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

		++siRendered;
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
		gpProfileManager->GetCpuCounter(kCpuCounterHexShieldsRendered).iCount = siRendered;
	}
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineHexShields].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineHexShieldsLighting].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, gLightingEnabled.Get<bool>() ? siRendered : 0);
}

} // namespace engine

#endif // BT_CLIENT
