#include "PointLights.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

void PointLightsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kCrc, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout));
	gpPipelineManager->mDynamicPipelines.CreateAreaLightingPipeline(kDynamicPipelineAxisAlignedLighting, kCrc, kpcName, sizeof(shaders::AxisAlignedQuadLayout), data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc, data::kShadersLightingPointLightfragCrc, DescriptorFlags::kSamplerClamp);
	Buffer* pVisibleLightsBuffers = gpBufferManager->CreateDynamicBuffer(kCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineVisibleLights(kCrc, kpcName, pVisibleLightsBuffers);
}

static int64_t siRendered = 0;
static int64_t siTotalCount = 0;

void PointLightsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords)
{
	siRendered = 0;
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoords, [](const game::FrameInterpolate& rInterpolate) -> const PointLightsInterpolate&
	{
		return rInterpolate.pointLights;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kCrc, kBufferMain, kpcName, sizeof(shaders::AxisAlignedQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineAxisAlignedLighting].at(kCrc), iCommandBuffer, 1, pBuffer);
	}
	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineVisibleLights].at(kCrc), iCommandBuffer, 2, pBuffer);
	}
}

void PointLightsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const PointLightsInterpolate& rCurrent = rFrameInterpolate.pointLights;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	siTotalCount += rCurrent.iCount;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	auto [pPointLightsLayouts, iPointLightsBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::AxisAlignedQuadLayout>(kCrc, kBufferMain, iCommandBuffer);
	auto [pVisibleLightsLayouts, iVisibleLightsBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::VisibleLightQuadLayout>(kCrc, kBufferVisibleLights, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iPointLightsBufferCapacity);
	ASSERT(siRendered + rCurrent.iCount <= iVisibleLightsBufferCapacity);

	XMVECTOR vecCameraRightNormal = XMVector3Normalize(engine::gpCamera->mMatView.r[0]);
	XMVECTOR vecCameraUpNormal = XMVector3Normalize(engine::gpCamera->mMatView.r[1]);

	float fMinLightingArea = MinLightingDepositSize();

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Positions are local to the rendered cell; the basis converts them into the camera cell's frame.
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[i];
		const PointLightsType& rType = PointLightsInterpolate::sTypes.at(rCurrent.puiTypeIndices[i]);
		float fRotation = rCurrent.pfRotations[i];
		float fLightingArea = std::max(rCurrent.pfLightingAreas[i], fMinLightingArea);
		float fLightingIntensity = rCurrent.pfLightingIntensities[i];

		XMFLOAT4A f4Position {};
		if (!IsPointVisible(Rebase(rBasis, vecLocalPosition), f4Position))
		{
			continue;
		}

		// The visible sprite uses the position in the camera cell's frame before base-height projection.
		XMFLOAT4A f4VisiblePosition = f4Position;

		XMStoreFloat4A(&f4Position, ProjectToBaseHeight(vecLocalPosition, rBasis));

		int64_t iTextureIndex = gpTextureManager->mTextureDescriptors.CrcToIndex(rType.uiCrc);
		float fBlurredTextureIndex = gpTextureManager->mTextureDescriptors.CrcToBlurredIndex(rType.uiCrc);
		XMFLOAT4A f4Parameters {};
		f4Parameters.x = fBlurredTextureIndex;
		f4Parameters.y = fLightingIntensity;
		f4Parameters.z = fRotation;
		BuildAxisAlignedQuad(pPointLightsLayouts[siRendered], f4Position, fLightingArea, f4Parameters, rType.uiColor);

		float fVisibleArea = rCurrent.pfVisibleAreas[i];
		float fVisibleIntensity = rCurrent.pfVisibleIntensities[i];

		shaders::VisibleLightQuadLayout& rVisibleLayout = pVisibleLightsLayouts[siRendered];

		if (rType.bCameraAligned)
		{
			XMVECTOR vecCenter = XMLoadFloat4A(&f4VisiblePosition);
			XMVECTOR vecRight = XMVectorScale(vecCameraRightNormal, fVisibleArea);
			XMVECTOR vecUp = XMVectorScale(vecCameraUpNormal, fVisibleArea);
			XMFLOAT4A f4Corner {};
			XMStoreFloat4A(&f4Corner, XMVectorAdd(XMVectorSubtract(vecCenter, vecRight), vecUp));
			rVisibleLayout.pf4Vertices[0] = {f4Corner.x, f4Corner.y, f4Corner.z, 1.0f};
			XMStoreFloat4A(&f4Corner, XMVectorAdd(XMVectorAdd(vecCenter, vecRight), vecUp));
			rVisibleLayout.pf4Vertices[1] = {f4Corner.x, f4Corner.y, f4Corner.z, 1.0f};
			XMStoreFloat4A(&f4Corner, XMVectorSubtract(XMVectorSubtract(vecCenter, vecRight), vecUp));
			rVisibleLayout.pf4Vertices[2] = {f4Corner.x, f4Corner.y, f4Corner.z, 1.0f};
			XMStoreFloat4A(&f4Corner, XMVectorSubtract(XMVectorAdd(vecCenter, vecRight), vecUp));
			rVisibleLayout.pf4Vertices[3] = {f4Corner.x, f4Corner.y, f4Corner.z, 1.0f};
		}
		else
		{
			rVisibleLayout.pf4Vertices[0] = {f4VisiblePosition.x - fVisibleArea, f4VisiblePosition.y + fVisibleArea, f4VisiblePosition.z, 1.0f};
			rVisibleLayout.pf4Vertices[1] = {f4VisiblePosition.x + fVisibleArea, f4VisiblePosition.y + fVisibleArea, f4VisiblePosition.z, 1.0f};
			rVisibleLayout.pf4Vertices[2] = {f4VisiblePosition.x - fVisibleArea, f4VisiblePosition.y - fVisibleArea, f4VisiblePosition.z, 1.0f};
			rVisibleLayout.pf4Vertices[3] = {f4VisiblePosition.x + fVisibleArea, f4VisiblePosition.y - fVisibleArea, f4VisiblePosition.z, 1.0f};
		}

		rVisibleLayout.pf4TextureCoordinates[0] = {0.0f, 0.0f, 0.0f, 0.0f};
		rVisibleLayout.pf4TextureCoordinates[1] = {1.0f, 0.0f, 0.0f, 0.0f};
		rVisibleLayout.pf4TextureCoordinates[2] = {0.0f, 1.0f, 0.0f, 0.0f};
		rVisibleLayout.pf4TextureCoordinates[3] = {1.0f, 1.0f, 0.0f, 0.0f};

		rVisibleLayout.puiColors[0] = rType.uiColor;
		rVisibleLayout.puiColors[1] = rType.uiColor;
		rVisibleLayout.puiColors[2] = rType.uiColor;
		rVisibleLayout.puiColors[3] = rType.uiColor;

		rVisibleLayout.fIntensity = fVisibleIntensity;
		rVisibleLayout.fRotation = fRotation;
		rVisibleLayout.uiTextureIndex = static_cast<uint32_t>(iTextureIndex);

		++siRendered;
	}
}

void PointLightsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterPointLights).iCount = siTotalCount;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterPointLightsRendered).iCount = siRendered;
	}
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineAxisAlignedLighting].at(kCrc)->WriteIndirectBuffer(iCommandBuffer, gLightingEnabled.Get<bool>() ? siRendered : 0);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineVisibleLights].at(kCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

} // namespace engine

#endif // BT_CLIENT
