#include "SmokeTrails.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

static int64_t siRendered = 0;
static int64_t siTotalCount = 0;

void SmokeTrailsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineSmoke(kuiCrc, kpcName, sizeof(shaders::QuadLayout));
}

void SmokeTrailsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;
	siTotalCount = 0;

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
		gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineSmoke].at(kuiCrc)->UpdateStorageBufferDescriptor(iCommandBuffer, 1, pBuffer);
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

	auto [pTrailLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::QuadLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	static common::RandomEngine sRandomEngine;

	for (const auto& [rId, riIndex] : rCurrent.idToIndexMap)
	{
		// Positions are local to the rendered cell; the basis converts them into the camera cell's frame.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[riIndex];
		const SmokeTrailsType& rType = SmokeTrailsInterpolate::sTypes.at(rCurrent.puiTypeIndices[riIndex]);
		float fIntensity = rCurrent.pfIntensities[riIndex];
		float fWidth = rType.fWidth;
		std::chrono::duration<float> startTime(rCurrent.pfStartTimes[riIndex]);
		XMVECTOR vecLocalSmoothedPosition = rCurrent.pVecSmoothedPositions[riIndex];

		XMFLOAT4A f4Position {};
		if (!IsPointVisible(Rebase(rBasis, vecLocalPosition), f4Position))
		{
			continue;
		}

		float fJitterOne = gSmokeTrailsSideJitter.Get() * common::Random(sRandomEngine);
		fJitterOne = fJitterOne * fJitterOne;
		float fJitterTwo = gSmokeTrailsSideJitter.Get() * common::Random(sRandomEngine);
		fJitterTwo = fJitterTwo * fJitterTwo;

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

		XMVECTOR vecPointOne = XMVectorAdd(vecBasePosition, XMVectorScale(vecLeftNormal, gSmokeTrailsWidthCurrent.Get() * fWidth));
		XMVECTOR vecPointTwo = XMVectorAdd(vecBasePosition, XMVectorScale(vecLeftNormal, -gSmokeTrailsWidthCurrent.Get() * fWidth));

		float fLength = gSmokeTrailsLength.Get() + gSmokeTrailsLengthJitter.Get() * common::Random(sRandomEngine);
		if (std::chrono::duration<float>(rFrameInterpolate.fCurrentTime) - startTime < std::chrono::duration<float>(50ms))
		{
			fLength = 0.0f;
		}

		XMVECTOR vecPointThree = XMVectorSubtract(XMVectorAdd(vecBaseSmoothedPosition, XMVectorScale(vecLeftNormal, gSmokeTrailsWidthPrevious.Get() * fJitterOne)), XMVectorScale(vecToSmoothedNormal, fLength * fLengthScale));
		XMVECTOR vecPointFour = XMVectorSubtract(XMVectorAdd(vecBaseSmoothedPosition, XMVectorScale(vecLeftNormal, -gSmokeTrailsWidthPrevious.Get() * fJitterTwo)), XMVectorScale(vecToSmoothedNormal, fLength * fLengthScale));

		XMStoreFloat4A(&f4Position, vecPointOne);
		pTrailLayouts[siRendered].pf4VerticesTexcoords[0] = {f4Position.x, f4Position.y, 0.0f, 0.0f};
		XMStoreFloat4A(&f4Position, vecPointTwo);
		pTrailLayouts[siRendered].pf4VerticesTexcoords[1] = {f4Position.x, f4Position.y, 1.0f, 0.0f};
		XMStoreFloat4A(&f4Position, vecPointThree);
		pTrailLayouts[siRendered].pf4VerticesTexcoords[2] = {f4Position.x, f4Position.y, 0.0f, 1.0f};
		XMStoreFloat4A(&f4Position, vecPointFour);
		pTrailLayouts[siRendered].pf4VerticesTexcoords[3] = {f4Position.x, f4Position.y, 1.0f, 1.0f};

		float fQuantity = fIntensity * gSmokeTrailsQuantity.Get() / fLengthScale;
		pTrailLayouts[siRendered].pf4Params[0] = {fQuantity, 1.0f, 0.0f, 0.0f};
		pTrailLayouts[siRendered].pf4Params[1] = {fQuantity, 1.0f, 0.0f, 0.0f};
		pTrailLayouts[siRendered].pf4Params[2] = {fQuantity, 0.0f, 0.0f, 0.0f};
		pTrailLayouts[siRendered].pf4Params[3] = {fQuantity, 0.0f, 0.0f, 0.0f};

		pTrailLayouts[siRendered].f4Params = {};
		pTrailLayouts[siRendered].uiColor = rType.uiColor;

		++siRendered;
	}
}

void SmokeTrailsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	gpProfileManager->SetCount(kCpuCounterSmokeTrails, siTotalCount);
	gpProfileManager->SetCount(kCpuCounterSmokeTrailsRendered, siRendered);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineSmoke].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, gSmokeEnabled.Get<bool>() ? siRendered : 0);
}

} // namespace engine

#endif // BT_CLIENT
