#include "SmokeTrails.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"

#include "Profile/ProfileRows.h"

namespace engine
{

static int64_t spiRendered[shaders::kiMaxFlightPlanes] {};
static int64_t siTotalCount = 0;

void SmokeTrailsInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		common::crc_t uiDepositKey = DynamicPipelines::SmokeDepositKey(kuiCrc, i);
		gpBufferManager->CreateDynamicBuffer(uiDepositKey, kBufferMain, kpcName, sizeof(shaders::QuadLayout));
		gpPipelineManager->mDynamicPipelines.CreateDepositPipeline(kDynamicPipelineSmoke, uiDepositKey, kpcName, data::kShadersQuadsQuadsVisibleAreavertCrc, data::kShadersSmokeSmokefragCrc, gpTextureManager->mRenderTargetTextures.mSmokeTexturesOne[i], {.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeGradientTexture}, &gpBufferManager->mpSmokeOccupancyVkBuffers[i][0], sizeof(shaders::QuadLayout));
	}
}

void SmokeTrailsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	std::ranges::fill(spiRendered, 0);
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const SmokeTrailsInterpolate&
	{
		return rInterpolate.smokeTrails;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	// Each trail adds at most one quad per plane, so every plane's buffer takes the full capacity.
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		common::crc_t uiDepositKey = DynamicPipelines::SmokeDepositKey(kuiCrc, i);
		if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(uiDepositKey, kBufferMain, kpcName, sizeof(shaders::QuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
		{
			PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmoke].at(uiDepositKey), iCommandBuffer, 1, pBuffer);
		}
	}
}

void SmokeTrailsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const SmokeTrailsInterpolate& rCurrent = rFrameInterpolate.smokeTrails;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	siTotalCount += rCurrent.iCount;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	shaders::QuadLayout* ppTrailLayouts[shaders::kiMaxFlightPlanes] {};
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		auto [pTrailLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::QuadLayout>(DynamicPipelines::SmokeDepositKey(kuiCrc, i), kBufferMain, iCommandBuffer);
		ASSERT(spiRendered[i] + rCurrent.iCount <= iBufferCapacity);
		ppTrailLayouts[i] = pTrailLayouts;
	}

	static common::RandomEngine sRandomEngine;

	for (const auto& [rId, riIndex] : rCurrent.idToIndexMap)
	{
		// Positions are local to the rendered cell; the basis converts them into the camera cell's frame.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[riIndex];
		const SmokeTrailsType& rType = SmokeTrailsInterpolate::sTypes.at(rCurrent.puiTypeIndices[riIndex]);
		float fIntensity = rCurrent.pfIntensities[riIndex];
		float fWidth = rType.fWidth;
		XMVECTOR vecLocalSmoothedPosition = rCurrent.pVecSmoothedPositions[riIndex];

		XMFLOAT4A f4Position {};
		if (!IsPointVisible(Rebase(rBasis, vecLocalPosition), f4Position))
		{
			continue;
		}

		float fJitterOne = gSmokeTrailsSideJitter.mfCurrent * common::Random(sRandomEngine);
		fJitterOne = fJitterOne * fJitterOne;
		float fJitterTwo = gSmokeTrailsSideJitter.mfCurrent * common::Random(sRandomEngine);
		fJitterTwo = fJitterTwo * fJitterTwo;

		float fLength = gSmokeTrailsLength.mfCurrent + gSmokeTrailsLengthJitter.mfCurrent * common::Random(sRandomEngine);
		if (TickToSimulationTime(rFrameInterpolate.iTick) - rCurrent.pStartTimes[riIndex] < 50ms)
		{
			fLength = 0.0f;
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

			XMVECTOR vecBasePosition = ProjectToFlightPlane(vecLocalPosition, rBasis, iPlane);
			XMVECTOR vecBaseSmoothedPosition = ProjectToFlightPlane(vecLocalSmoothedPosition, rBasis, iPlane);

			XMVECTOR vecToSmoothed = XMVectorSubtract(vecBasePosition, vecBaseSmoothedPosition);
			float fLengthScale = XMVectorGetX(XMVector3Length(vecToSmoothed));
			if (fLengthScale <= 0.01f)
			{
				continue;
			}
			XMVECTOR vecToSmoothedNormal = XMVector3Normalize(vecToSmoothed);

			XMVECTOR vecLeftNormal = XMVector3Normalize(XMVector3Cross(vecToSmoothedNormal, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)));

			XMVECTOR vecPointOne = XMVectorAdd(vecBasePosition, XMVectorScale(vecLeftNormal, gSmokeTrailsWidthCurrent.mfCurrent * fWidth));
			XMVECTOR vecPointTwo = XMVectorAdd(vecBasePosition, XMVectorScale(vecLeftNormal, -gSmokeTrailsWidthCurrent.mfCurrent * fWidth));

			XMVECTOR vecPointThree = XMVectorSubtract(XMVectorAdd(vecBaseSmoothedPosition, XMVectorScale(vecLeftNormal, gSmokeTrailsWidthPrevious.mfCurrent * fJitterOne)), XMVectorScale(vecToSmoothedNormal, fLength * fLengthScale));
			XMVECTOR vecPointFour = XMVectorSubtract(XMVectorAdd(vecBaseSmoothedPosition, XMVectorScale(vecLeftNormal, -gSmokeTrailsWidthPrevious.mfCurrent * fJitterTwo)), XMVectorScale(vecToSmoothedNormal, fLength * fLengthScale));

			shaders::QuadLayout& rTrailLayout = ppTrailLayouts[iPlane][spiRendered[iPlane]];
			XMStoreFloat4A(&f4Position, vecPointOne);
			rTrailLayout.pf4VerticesTextureCoordinates[0] = {f4Position.x, f4Position.y, 0.0f, 0.0f};
			XMStoreFloat4A(&f4Position, vecPointTwo);
			rTrailLayout.pf4VerticesTextureCoordinates[1] = {f4Position.x, f4Position.y, 1.0f, 0.0f};
			XMStoreFloat4A(&f4Position, vecPointThree);
			rTrailLayout.pf4VerticesTextureCoordinates[2] = {f4Position.x, f4Position.y, 0.0f, 1.0f};
			XMStoreFloat4A(&f4Position, vecPointFour);
			rTrailLayout.pf4VerticesTextureCoordinates[3] = {f4Position.x, f4Position.y, 1.0f, 1.0f};

			float fQuantity = fIntensity * gSmokeTrailsQuantity.mfCurrent / fLengthScale * pfPlaneWeights[j];
			rTrailLayout.pf4Parameters[0] = {fQuantity, 1.0f, 0.0f, 0.0f};
			rTrailLayout.pf4Parameters[1] = {fQuantity, 1.0f, 0.0f, 0.0f};
			rTrailLayout.pf4Parameters[2] = {fQuantity, 0.0f, 0.0f, 0.0f};
			rTrailLayout.pf4Parameters[3] = {fQuantity, 0.0f, 0.0f, 0.0f};

			rTrailLayout.f4Parameters = {};
			rTrailLayout.uiColor = static_cast<uint32_t>(rType.iColor);

			++spiRendered[iPlane];
		}
	}
}

void SmokeTrailsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterSmokeTrails).iCount = siTotalCount;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterSmokeTrailsRendered).iCount = std::ranges::fold_left(spiRendered, 0i64, std::plus {});
	}
	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmoke].at(DynamicPipelines::SmokeDepositKey(kuiCrc, i))->WriteIndirectBuffer(iCommandBuffer, gSmokeEnabled.Get<bool>() ? spiRendered[i] : 0);
	}
}

} // namespace engine

#endif // BT_CLIENT
