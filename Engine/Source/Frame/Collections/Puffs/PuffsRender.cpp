#include "Puffs.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileRows.h"

namespace engine
{

constexpr int64_t kiDynamicBufferInitialCapacity = 1'024;

void PuffsInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		common::crc_t uiDepositKey = DynamicPipelines::SmokeDepositKey(kCrc, i);
		gpBufferManager->CreateDynamicBuffer(uiDepositKey, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout), kiDynamicBufferInitialCapacity);
		gpPipelineManager->mDynamicPipelines.CreateDepositPipeline(kDynamicPipelineSmokeAxisAligned, uiDepositKey, kpcName, data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc, data::kShadersSmokeSmokefragCrc, gpTextureManager->mRenderTargetTextures.mSmokeTexturesOne[i], {.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .textureCrc = data::kTexturesSmokeBC44jpgCrc}, &gpBufferManager->mpSmokeOccupancyVkBuffers[i][0]);
	}
}

static int64_t spiRendered[shaders::kiMaxFlightPlanes] {};
static int64_t siTotalCount = 0;

void PuffsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	std::ranges::fill(spiRendered, 0);
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const PuffsInterpolate&
	{
		return rInterpolate.puffs;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	// Each puff adds at most one quad per plane, so every plane's buffer takes the full capacity.
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		common::crc_t uiDepositKey = DynamicPipelines::SmokeDepositKey(kCrc, i);
		if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(uiDepositKey, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
		{
			PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmokeAxisAligned].at(uiDepositKey), iCommandBuffer, 1, pBuffer);
		}
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

	shaders::AxisAlignedQuadLayout* ppPuffsLayouts[shaders::kiMaxFlightPlanes] {};
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		auto [pPuffsLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::AxisAlignedQuadLayout>(DynamicPipelines::SmokeDepositKey(kCrc, i), kBufferMain, iCommandBuffer);
		ASSERT(spiRendered[i] + rCurrent.iCount <= iBufferCapacity);
		ppPuffsLayouts[i] = pPuffsLayouts;
	}

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

		SmokePlaneWeights planeWeights = ComputeSmokePlaneWeights(XMVectorGetZ(vecLocalPosition));
		float pfPlaneWeights[] = {1.0f - planeWeights.fUpperWeight, planeWeights.fUpperWeight};
		for (int64_t j = 0; j < std::ssize(pfPlaneWeights); ++j)
		{
			if (pfPlaneWeights[j] == 0.0f)
			{
				continue;
			}
			int64_t iPlane = planeWeights.iLowerPlane + j;

			XMStoreFloat4A(&f4Position, ProjectToFlightPlane(vecLocalPosition, rBasis, iPlane));

			XMFLOAT4A f4Parameters {};
			f4Parameters.x = fIntensity * pfPlaneWeights[j];  // Smoke.frag uses this as intensity multiplier
			f4Parameters.y = fIntensity;  // Smoke.frag uses pow(max(0.0f, f4InParams.y), globalLayout.fSmokeIntensityFalloff).
			f4Parameters.w = fRotation;   // Smoke.frag uses this for Rotate()
			BuildAxisAlignedQuad(ppPuffsLayouts[iPlane][spiRendered[iPlane]], f4Position, fArea, f4Parameters, rType.uiColor);

			++spiRendered[iPlane];
		}
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
		gpProfileManager->GetCpuCounter(kCpuCounterPuffsRendered).iCount = std::ranges::fold_left(spiRendered, 0i64, std::plus {});
	}
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmokeAxisAligned].at(DynamicPipelines::SmokeDepositKey(kCrc, i))->WriteIndirectBuffer(iCommandBuffer, gSmokeEnabled.Get<bool>() ? spiRendered[i] : 0);
	}
}

} // namespace engine

#endif // BT_CLIENT
