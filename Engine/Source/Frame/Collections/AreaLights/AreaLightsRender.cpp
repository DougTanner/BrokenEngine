#include "AreaLights.h"

#if defined(BT_CLIENT)

#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/WrapperBase.h"

#include "Profile/ProfileRows.h"

namespace engine
{

constexpr int64_t kiDynamicBufferInitialCapacity = 8'192;

void AreaLightsInterpolate::GraphicsResources()
{
	Buffer* pVisibleLightsBuffers = gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout), kiDynamicBufferInitialCapacity);
	gpPipelineManager->mDynamicPipelines.CreatePipelineVisibleLights(kuiCrc, kpcName, pVisibleLightsBuffers);
}

static int64_t siRendered = 0;
static int64_t siTotalCount = 0;

void AreaLightsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const AreaLightsInterpolate&
	{
		return rInterpolate.areaLights;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineVisibleLights].at(kuiCrc), iCommandBuffer, 2, pBuffer);
	}
}

void AreaLightsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const AreaLightsInterpolate& rCurrent = rFrameInterpolate.areaLights;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	siTotalCount += rCurrent.iCount;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	auto [pVisibleLightsLayouts, iVisibleLightsBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::VisibleLightQuadLayout>(kuiCrc, kBufferVisibleLights, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iVisibleLightsBufferCapacity);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Every position below stays local to the rendered cell until it reaches the GPU
		// layout or the frustum test.
		XMVECTOR vecVisiblePosition0 = rCurrent.pVecVisiblePositions[0][i];
		XMVECTOR vecVisiblePosition1 = rCurrent.pVecVisiblePositions[1][i];
		XMVECTOR vecVisiblePosition2 = rCurrent.pVecVisiblePositions[2][i];
		XMVECTOR vecVisiblePosition3 = rCurrent.pVecVisiblePositions[3][i];

		const AreaLightsType& rType = AreaLightsInterpolate::sTypes.at(rCurrent.puiTypeIndices[i]);
		int64_t iTextureIndex = gpTextureManager->mTextureDescriptors.CrcToIndex(rType.uiCrc);
		float fIntensityMultiplier = rCurrent.pfIntensityMultipliers[i];
		float fVisibleIntensity = rType.pVisibleIntensityWrapper != nullptr ? rType.pVisibleIntensityWrapper->mfCurrent : rType.fVisibleIntensity;

		// Rebasing translates the bounding box, so converting its two corners preserves the rectangle.
		auto [vecMinimum, vecMaximum] = common::ComputeAxisAlignedBoundingBox(vecVisiblePosition0, vecVisiblePosition1, vecVisiblePosition2, vecVisiblePosition3);
		if (!common::AxisAlignedBoundingBoxIntersectsArea(engine::gpCamera->mf4RenderVisibleArea, Rebase(rBasis, vecMinimum), Rebase(rBasis, vecMaximum)))
		{
			continue;
		}

		shaders::VisibleLightQuadLayout& rVisibleLayout = pVisibleLightsLayouts[siRendered];
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[0], Rebase(rBasis, vecVisiblePosition0));
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[1], Rebase(rBasis, vecVisiblePosition1));
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[2], Rebase(rBasis, vecVisiblePosition2));
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[3], Rebase(rBasis, vecVisiblePosition3));

		rVisibleLayout.pf4TextureCoordinates[0] = {rType.pf2TextureCoordinates[0].x, rType.pf2TextureCoordinates[0].y, 0.0f, 0.0f};
		rVisibleLayout.pf4TextureCoordinates[1] = {rType.pf2TextureCoordinates[1].x, rType.pf2TextureCoordinates[1].y, 0.0f, 0.0f};
		rVisibleLayout.pf4TextureCoordinates[2] = {rType.pf2TextureCoordinates[2].x, rType.pf2TextureCoordinates[2].y, 0.0f, 0.0f};
		rVisibleLayout.pf4TextureCoordinates[3] = {rType.pf2TextureCoordinates[3].x, rType.pf2TextureCoordinates[3].y, 0.0f, 0.0f};

		rVisibleLayout.puiColors[0] = rType.puiColors[0];
		rVisibleLayout.puiColors[1] = rType.puiColors[1];
		rVisibleLayout.puiColors[2] = rType.puiColors[2];
		rVisibleLayout.puiColors[3] = rType.puiColors[3];

		rVisibleLayout.fIntensity = fVisibleIntensity * fIntensityMultiplier;
		rVisibleLayout.fRotation = 0.0f;
		rVisibleLayout.uiTextureIndex = static_cast<uint32_t>(iTextureIndex);

		++siRendered;
	}
}

void AreaLightsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterAreaLights).iCount = siTotalCount;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterAreaLightsRendered).iCount = siRendered;
	}
	gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineVisibleLights].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

} // namespace engine

#endif // BT_CLIENT
