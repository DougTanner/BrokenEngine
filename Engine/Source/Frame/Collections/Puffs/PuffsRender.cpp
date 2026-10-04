#include "Puffs.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

void PuffsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kCrc, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout));
	gpPipelineManager->mDynamicPipelines.CreateDepositPipeline(kDynamicPipelineSmokeAxisAligned, kCrc, kpcName, data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc, data::kShadersSmokeSmokefragCrc, gpTextureManager->mRenderTargetTextures.mSmokeTextureOne, {.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .textureCrc = data::kTexturesSmokeBC44jpgCrc}, &gpBufferManager->mSmokeOccupancyVkBuffers[0], sizeof(shaders::AxisAlignedQuadLayout));
}

static int64_t siRendered = 0;
static int64_t siTotalCount = 0;

void PuffsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const PuffsInterpolate&
	{
		return rInterpolate.puffs;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kCrc, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineSmokeAxisAligned].at(kCrc), iCommandBuffer, 1, pBuffer);
	}
}

void PuffsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const PuffsInterpolate& rCurrent = rFrameInterpolate.puffs;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	siTotalCount += rCurrent.iCount;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	auto [pPuffsLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::AxisAlignedQuadLayout>(kCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Positions are local to the rendered cell; the basis converts them into the camera cell's frame.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[i];
		const PuffsType& rType = PuffsInterpolate::sTypes.at(rCurrent.puiTypeIndices[i]);
		float fIntensity = rCurrent.pfIntensities[i];
		float fArea = rCurrent.pfAreas[i];
		float fRotation = rCurrent.pfRotations[i];

		XMFLOAT4A f4Position {};
		if (!IsPointVisible(Rebase(rBasis, vecLocalPosition), f4Position))
		{
			continue;
		}

		XMStoreFloat4A(&f4Position, ProjectToBaseHeight(vecLocalPosition, rBasis));

		XMFLOAT4A f4Parameters {};
		f4Parameters.x = fIntensity;  // Smoke.frag uses this as intensity multiplier
		f4Parameters.y = fIntensity;  // Smoke.frag uses pow(max(0.0f, f4InParams.y), globalLayout.fSmokeIntensityFalloff).
		f4Parameters.w = fRotation;   // Smoke.frag uses this for Rotate()
		BuildAxisAlignedQuad(pPuffsLayouts[siRendered], f4Position, fArea, f4Parameters, rType.uiColor);

		++siRendered;
	}
}

void PuffsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterPuffs).iCount = siTotalCount;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterPuffsRendered).iCount = siRendered;
	}
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineSmokeAxisAligned].at(kCrc)->WriteIndirectBuffer(iCommandBuffer, gSmokeEnabled.Get<bool>() ? siRendered : 0);
}

} // namespace engine

#endif // BT_CLIENT
