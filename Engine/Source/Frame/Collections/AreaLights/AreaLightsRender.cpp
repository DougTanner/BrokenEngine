#include "AreaLights.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/WrapperBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

void AreaLightsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineLighting(kuiCrc, kpcName, sizeof(shaders::QuadLayout));
	Buffer* pVisibleLightsBuffers = gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout));
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

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineLighting].at(kuiCrc)->UpdateStorageBufferDescriptor(iCommandBuffer, 1, pBuffer);
	}
	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineVisibleLights].at(kuiCrc)->UpdateStorageBufferDescriptor(iCommandBuffer, 2, pBuffer);
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
	auto [pAreaLightsLayouts, iAreaLightsBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::QuadLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iVisibleLightsBufferCapacity);
	ASSERT(siRendered + rCurrent.iCount <= iAreaLightsBufferCapacity);

	float fMinimumLightingSize = MinLightingDepositSize();

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Every position below stays local to the rendered cell until it reaches the GPU
		// layout, the frustum test, or ProjectToBaseHeight — the three conversion points of this renderer.
		XMVECTOR vecVisiblePosition0 = rCurrent.pVecVisiblePositions[0][i];
		XMVECTOR vecVisiblePosition1 = rCurrent.pVecVisiblePositions[1][i];
		XMVECTOR vecVisiblePosition2 = rCurrent.pVecVisiblePositions[2][i];
		XMVECTOR vecVisiblePosition3 = rCurrent.pVecVisiblePositions[3][i];

		XMVECTOR vecCenter = XMVectorScale(XMVectorAdd(XMVectorAdd(vecVisiblePosition0, vecVisiblePosition1), XMVectorAdd(vecVisiblePosition2, vecVisiblePosition3)), 0.25f);
		const AreaLightsType& rType = AreaLightsInterpolate::sTypes.at(rCurrent.puiTypeIndices[i]);
		int64_t iTextureIndex = gpTextureManager->mTextureDescriptors.CrcToIndex(rType.uiCrc);
		float fBlurredTextureIndex = gpTextureManager->mTextureDescriptors.CrcToBlurredIndex(rType.uiCrc);
		float fIntensityMultiplier = rCurrent.pfIntensityMultipliers[i];
		float fVisibleIntensity = rType.pVisibleIntensityWrapper != nullptr ? rType.pVisibleIntensityWrapper->Get() : rType.fVisibleIntensity;
		float fLightingSize = std::max(rType.pLightingSizeWrapper != nullptr ? rType.pLightingSizeWrapper->Get() : rType.fLightingSize, fMinimumLightingSize);
		float fLightingIntensity = rType.pLightingIntensityWrapper != nullptr ? rType.pLightingIntensityWrapper->Get() : rType.fLightingIntensity;

		// Lighting quads preserve the visible quad's orientation.
		XMVECTOR vecOffset0 = XMVectorSubtract(vecVisiblePosition0, vecCenter);
		float fVisibleExtent = XMVectorGetX(XMVector2Length(vecOffset0));
		float fScale = fVisibleExtent > 0.0f ? fLightingSize / fVisibleExtent : 1.0f;
		XMVECTOR vecScaleFactor = XMVectorReplicate(fScale);

		XMVECTOR vecLightingPosition0 = XMVectorMultiplyAdd(vecOffset0, vecScaleFactor, vecCenter);
		XMVECTOR vecLightingPosition1 = XMVectorMultiplyAdd(XMVectorSubtract(vecVisiblePosition1, vecCenter), vecScaleFactor, vecCenter);
		XMVECTOR vecLightingPosition2 = XMVectorMultiplyAdd(XMVectorSubtract(vecVisiblePosition2, vecCenter), vecScaleFactor, vecCenter);
		XMVECTOR vecLightingPosition3 = XMVectorMultiplyAdd(XMVectorSubtract(vecVisiblePosition3, vecCenter), vecScaleFactor, vecCenter);

		// Rebasing translates the bounding box, so converting its two corners preserves the rectangle.
		auto [vecMinimum, vecMaximum] = common::ComputeAxisAlignedBoundingBox(vecVisiblePosition0, vecVisiblePosition1, vecVisiblePosition2, vecVisiblePosition3, vecLightingPosition0, vecLightingPosition1, vecLightingPosition2, vecLightingPosition3);
		if (!common::AxisAlignedBoundingBoxIntersectsArea(engine::gpCamera->f4RenderVisibleArea, Rebase(rBasis, vecMinimum), Rebase(rBasis, vecMaximum)))
		{
			continue;
		}

		shaders::VisibleLightQuadLayout& rVisibleLayout = pVisibleLightsLayouts[siRendered];
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[0], Rebase(rBasis, vecVisiblePosition0));
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[1], Rebase(rBasis, vecVisiblePosition1));
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[2], Rebase(rBasis, vecVisiblePosition2));
		XMStoreFloat4(&rVisibleLayout.pf4Vertices[3], Rebase(rBasis, vecVisiblePosition3));

		rVisibleLayout.pf4Texcoords[0] = {rType.pf2TextureCoordinates[0].x, rType.pf2TextureCoordinates[0].y, 0.0f, 0.0f};
		rVisibleLayout.pf4Texcoords[1] = {rType.pf2TextureCoordinates[1].x, rType.pf2TextureCoordinates[1].y, 0.0f, 0.0f};
		rVisibleLayout.pf4Texcoords[2] = {rType.pf2TextureCoordinates[2].x, rType.pf2TextureCoordinates[2].y, 0.0f, 0.0f};
		rVisibleLayout.pf4Texcoords[3] = {rType.pf2TextureCoordinates[3].x, rType.pf2TextureCoordinates[3].y, 0.0f, 0.0f};

		rVisibleLayout.puiColors[0] = rType.puiColors[0];
		rVisibleLayout.puiColors[1] = rType.puiColors[1];
		rVisibleLayout.puiColors[2] = rType.puiColors[2];
		rVisibleLayout.puiColors[3] = rType.puiColors[3];

		rVisibleLayout.fIntensity = fVisibleIntensity * fIntensityMultiplier;
		rVisibleLayout.fRotation = 0.0f;
		rVisibleLayout.uiTextureIndex = static_cast<uint32_t>(iTextureIndex);

		// Populate area light quad with base height positions for ground shadow effect
		shaders::QuadLayout& rAreaLayout = pAreaLightsLayouts[siRendered];

		XMVECTOR vecBaseLighting0 = ProjectToBaseHeight(vecLightingPosition0, rBasis);
		XMVECTOR vecBaseLighting1 = ProjectToBaseHeight(vecLightingPosition1, rBasis);
		XMVECTOR vecBaseLighting2 = ProjectToBaseHeight(vecLightingPosition2, rBasis);
		XMVECTOR vecBaseLighting3 = ProjectToBaseHeight(vecLightingPosition3, rBasis);

		XMFLOAT4A f4Base {};
		XMStoreFloat4A(&f4Base, vecBaseLighting0);
		rAreaLayout.pf4VerticesTexcoords[0] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[0].x, rType.pf2TextureCoordinates[0].y};
		XMStoreFloat4A(&f4Base, vecBaseLighting1);
		rAreaLayout.pf4VerticesTexcoords[1] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[1].x, rType.pf2TextureCoordinates[1].y};
		XMStoreFloat4A(&f4Base, vecBaseLighting2);
		rAreaLayout.pf4VerticesTexcoords[2] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[2].x, rType.pf2TextureCoordinates[2].y};
		XMStoreFloat4A(&f4Base, vecBaseLighting3);
		rAreaLayout.pf4VerticesTexcoords[3] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[3].x, rType.pf2TextureCoordinates[3].y};

		XMFLOAT4A f4Parameters {};
		f4Parameters.x = fBlurredTextureIndex;
		f4Parameters.y = fLightingIntensity * fIntensityMultiplier;
		rAreaLayout.pf4Params[0] = f4Parameters;
		rAreaLayout.pf4Params[1] = f4Parameters;
		rAreaLayout.pf4Params[2] = f4Parameters;
		rAreaLayout.pf4Params[3] = f4Parameters;
		rAreaLayout.uiColor = rType.puiColors[0];

		++siRendered;
	}
}

void AreaLightsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	gpProfileManager->SetCount(kCpuCounterAreaLights, siTotalCount);
	gpProfileManager->SetCount(kCpuCounterAreaLightsRendered, siRendered);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineLighting].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, gLightingEnabled.Get<bool>() ? siRendered : 0);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineVisibleLights].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

} // namespace engine

#endif // BT_CLIENT
