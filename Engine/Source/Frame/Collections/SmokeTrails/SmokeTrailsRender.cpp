#include "SmokeTrails.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"

#include "Profile/ProfileRows.h"

namespace engine
{

constexpr int64_t kiDynamicBufferInitialCapacity = 2'048;

static int64_t siRendered = 0;

void SmokeTrailsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout), kiDynamicBufferInitialCapacity);
	gpPipelineManager->mDynamicPipelines.CreateDepositPipeline(kDynamicPipelineSmoke, kuiCrc, kpcName, data::kShadersQuadsQuadsVisibleAreavertCrc, data::kShadersSmokeSmokefragCrc, gpTextureManager->mRenderTargetTextures.mSmokeTextureOne, {.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeGradientTexture}, &gpBufferManager->mpSmokeOccupancyVkBuffers[0]);
}

void SmokeTrailsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const SmokeTrailsInterpolate&
	{
		return rInterpolate.smokeTrails;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmoke].at(kuiCrc), iCommandBuffer, 1, pBuffer);
	}
}

void SmokeTrailsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const SmokeTrailsInterpolate& rCurrent = rFrameInterpolate.smokeTrails;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (gbGameplayPaused || rCurrent.iCount == 0)
	{
		return;
	}

	auto [pTrailLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::QuadLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	static common::RandomEngine sRandomEngine;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Positions are local to the rendered cell; the basis converts them into the camera cell's frame.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[i];
		const SmokeTrailsType& rType = SmokeTrailsInterpolate::sTypes.at(rCurrent.puiTypeIndices[i]);
		float fIntensity = rCurrent.pfIntensities[i];
		float fWidth = rType.fWidth;
		XMVECTOR vecLocalSmoothedPosition = rCurrent.pVecSmoothedPositions[i];

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
		if (TickToSimulationTime(rFrameInterpolate.iTick) - rCurrent.pStartTimes[i] < 50ms)
		{
			fLength = 0.0f;
		}

		XMVECTOR vecBasePosition = ProjectToBaseHeight(vecLocalPosition, rBasis);
		XMVECTOR vecBaseSmoothedPosition = ProjectToBaseHeight(vecLocalSmoothedPosition, rBasis);

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

		shaders::QuadLayout& rTrailLayout = pTrailLayouts[siRendered];
		XMStoreFloat4A(&f4Position, vecPointOne);
		rTrailLayout.pf4VerticesTextureCoordinates[0] = {f4Position.x, f4Position.y, 0.0f, 0.0f};
		XMStoreFloat4A(&f4Position, vecPointTwo);
		rTrailLayout.pf4VerticesTextureCoordinates[1] = {f4Position.x, f4Position.y, 1.0f, 0.0f};
		XMStoreFloat4A(&f4Position, vecPointThree);
		rTrailLayout.pf4VerticesTextureCoordinates[2] = {f4Position.x, f4Position.y, 0.0f, 1.0f};
		XMStoreFloat4A(&f4Position, vecPointFour);
		rTrailLayout.pf4VerticesTextureCoordinates[3] = {f4Position.x, f4Position.y, 1.0f, 1.0f};

		float fQuantity = fIntensity * gSmokeTrailsQuantity.mfCurrent / fLengthScale;
		rTrailLayout.pf4Parameters[0] = {fQuantity, 1.0f, 0.0f, 0.0f};
		rTrailLayout.pf4Parameters[1] = {fQuantity, 1.0f, 0.0f, 0.0f};
		rTrailLayout.pf4Parameters[2] = {fQuantity, 0.0f, 0.0f, 0.0f};
		rTrailLayout.pf4Parameters[3] = {fQuantity, 0.0f, 0.0f, 0.0f};

		++siRendered;
	}
}

void SmokeTrailsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterSmokeTrailsRendered).iCount = siRendered;
	}
	gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmoke].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, gSmokeEnabled.Get<bool>() ? siRendered : 0);
}

} // namespace engine

#endif // BT_CLIENT
