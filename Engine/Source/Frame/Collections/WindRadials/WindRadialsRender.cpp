#include "WindRadials.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

void WindRadialsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout));
	gpPipelineManager->mDynamicPipelines.CreateDepositPipeline(kDynamicPipelineWindDepositAxisAlignedA, kuiCrc, kpcName, data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc, data::kShadersWindWindDepositfragCrc, gpTextureManager->mRenderTargetTextures.mWindTextureOne, {.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .textureCrc = data::kTexturesParticlesBC4Square24pngCrc}, &gpBufferManager->mWindOccupancyVkBuffers[0], sizeof(shaders::AxisAlignedQuadLayout));
	gpPipelineManager->mDynamicPipelines.CreateDepositPipeline(kDynamicPipelineWindDepositAxisAlignedB, kuiCrc, kpcName, data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc, data::kShadersWindWindDepositfragCrc, gpTextureManager->mRenderTargetTextures.mWindTextureTwo, {.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .textureCrc = data::kTexturesParticlesBC4Square24pngCrc}, &gpBufferManager->mWindOccupancyVkBuffers[1], 0);
}

static int64_t siRendered = 0;

void WindRadialsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	if (!gWindEnabled.Get<bool>())
	{
		return;
	}

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const WindRadialsInterpolate&
	{
		return rInterpolate.windRadials;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositAxisAlignedA].at(kuiCrc), iCommandBuffer, 1, pBuffer);
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositAxisAlignedB].at(kuiCrc), iCommandBuffer, 1, pBuffer);
	}
}

void WindRadialsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const WindRadialsInterpolate& rCurrent = rFrameInterpolate.windRadials;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (!gWindEnabled.Get<bool>())
	{
		return;
	}

	if (rCurrent.iCount == 0)
	{
		return;
	}

	auto [pLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::AxisAlignedQuadLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Positions are local to the rendered cell; the basis converts them into the camera cell's frame.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[i];
		float fIntensity = rCurrent.pfIntensities[i];
		float fSize = rCurrent.pfSizes[i];

		XMFLOAT4A f4Position {};
		if (!IsPointVisible(Rebase(rBasis, vecLocalPosition), f4Position))
		{
			continue;
		}

		XMVECTOR vecBasePosition = ProjectToBaseHeight(vecLocalPosition, rBasis);
		XMStoreFloat4A(&f4Position, vecBasePosition);

		// WindDeposit.frag interprets .w = 1.0 as radial wind.
		XMFLOAT4A f4Parameters = {fIntensity, 0.0f, 0.0f, 1.0f};
		BuildAxisAlignedQuad(pLayouts[siRendered], f4Position, fSize, f4Parameters, 0xFFFFFFFF);

		++siRendered;
	}
}

void WindRadialsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositAxisAlignedA].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, giWindTextureIndex == 0 ? siRendered : 0);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositAxisAlignedB].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, giWindTextureIndex == 1 ? siRendered : 0);
}

} // namespace engine

#endif // BT_CLIENT
