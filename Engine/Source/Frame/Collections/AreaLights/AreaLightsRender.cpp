#include "AreaLights.h"

#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/WrapperBase.h"

#include "Profile/ProfileRows.h"

namespace engine
{

constexpr int64_t kiDynamicBufferInitialCapacity = 8'192;

void AreaLightsInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		common::crc_t uiDepositKey = DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i);
		gpBufferManager->CreateDynamicBuffer(uiDepositKey, kBufferMain, kpcName, sizeof(shaders::QuadLayout), kiDynamicBufferInitialCapacity);
		gpPipelineManager->mDynamicPipelines.CreateAreaLightingPipeline(kDynamicPipelineLighting, uiDepositKey, kpcName, data::kShadersQuadsQuadsVisibleAreavertCrc, data::kShadersLightingAreaLightfragCrc, DescriptorFlags::kSamplerRepeat);
	}
	Buffer* pVisibleLightsBuffers = gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferVisibleLights, kpcName, sizeof(shaders::VisibleLightQuadLayout), kiDynamicBufferInitialCapacity);
	gpPipelineManager->mDynamicPipelines.CreatePipelineVisibleLights(kuiCrc, kpcName, pVisibleLightsBuffers);
}

static int64_t siRendered = 0; // Visible-light cursor
static int64_t spiDepositsRendered[shaders::kiLightingLayers] {}; // Deposit cursor per lighting layer
static int64_t siTotalCount = 0;

void AreaLightsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;
	std::ranges::fill(spiDepositsRendered, 0);
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const AreaLightsInterpolate&
	{
		return rInterpolate.areaLights;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	// Each light deposits into one layer, and any layer may receive every light, so every layer's buffer takes the full capacity.
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		common::crc_t uiDepositKey = DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i);
		if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(uiDepositKey, kBufferMain, kpcName, sizeof(shaders::QuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
		{
			PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineLighting].at(uiDepositKey), iCommandBuffer, 1, pBuffer);
		}
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
	shaders::QuadLayout* ppAreaLightsLayouts[shaders::kiLightingLayers] {};
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		auto [pAreaLightsLayouts, iAreaLightsBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::QuadLayout>(DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i), kBufferMain, iCommandBuffer);
		ASSERT(spiDepositsRendered[i] + rCurrent.iCount <= iAreaLightsBufferCapacity);
		ppAreaLightsLayouts[i] = pAreaLightsLayouts;
	}

	float fMinimumLightingSize = MinLightingDepositSize();

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Every position below stays local to the rendered cell until it reaches the GPU
		// layout, the frustum test, or ProjectToFlightPlane — the three conversion points of this renderer.
		XMVECTOR vecVisiblePosition0 = rCurrent.pVecVisiblePositions[0][i];
		XMVECTOR vecVisiblePosition1 = rCurrent.pVecVisiblePositions[1][i];
		XMVECTOR vecVisiblePosition2 = rCurrent.pVecVisiblePositions[2][i];
		XMVECTOR vecVisiblePosition3 = rCurrent.pVecVisiblePositions[3][i];

		XMVECTOR vecCenter = XMVectorScale(XMVectorAdd(XMVectorAdd(vecVisiblePosition0, vecVisiblePosition1), XMVectorAdd(vecVisiblePosition2, vecVisiblePosition3)), 0.25f);
		const AreaLightsType& rType = AreaLightsInterpolate::sTypes.at(rCurrent.puiTypeIndices[i]);
		int64_t iTextureIndex = gpTextureManager->mTextureDescriptors.CrcToIndex(rType.uiCrc);
		float fBlurredTextureIndex = gpTextureManager->mTextureDescriptors.CrcToBlurredIndex(rType.uiCrc);
		float fIntensityMultiplier = rCurrent.pfIntensityMultipliers[i];
		float fVisibleIntensity = rType.pVisibleIntensityWrapper != nullptr ? rType.pVisibleIntensityWrapper->mfCurrent : rType.fVisibleIntensity;
		float fLightingSize = std::max(rType.pLightingSizeWrapper != nullptr ? rType.pLightingSizeWrapper->mfCurrent : rType.fLightingSize, fMinimumLightingSize);
		float fLightingIntensity = rType.pLightingIntensityWrapper != nullptr ? rType.pLightingIntensityWrapper->mfCurrent : rType.fLightingIntensity;

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

		// The deposit quad goes to the lighting layer of the flight plane nearest the light, projected onto that plane.
		int64_t iLayer = gpFlightPlanes->NearestPlaneIndex(XMVectorGetZ(vecCenter));
		shaders::QuadLayout& rAreaLayout = ppAreaLightsLayouts[iLayer][spiDepositsRendered[iLayer]];

		XMVECTOR vecBaseLighting0 = ProjectToFlightPlane(vecLightingPosition0, rBasis, iLayer);
		XMVECTOR vecBaseLighting1 = ProjectToFlightPlane(vecLightingPosition1, rBasis, iLayer);
		XMVECTOR vecBaseLighting2 = ProjectToFlightPlane(vecLightingPosition2, rBasis, iLayer);
		XMVECTOR vecBaseLighting3 = ProjectToFlightPlane(vecLightingPosition3, rBasis, iLayer);

		XMFLOAT4A f4Base {};
		XMStoreFloat4A(&f4Base, vecBaseLighting0);
		rAreaLayout.pf4VerticesTextureCoordinates[0] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[0].x, rType.pf2TextureCoordinates[0].y};
		XMStoreFloat4A(&f4Base, vecBaseLighting1);
		rAreaLayout.pf4VerticesTextureCoordinates[1] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[1].x, rType.pf2TextureCoordinates[1].y};
		XMStoreFloat4A(&f4Base, vecBaseLighting2);
		rAreaLayout.pf4VerticesTextureCoordinates[2] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[2].x, rType.pf2TextureCoordinates[2].y};
		XMStoreFloat4A(&f4Base, vecBaseLighting3);
		rAreaLayout.pf4VerticesTextureCoordinates[3] = {f4Base.x, f4Base.y, rType.pf2TextureCoordinates[3].x, rType.pf2TextureCoordinates[3].y};

		XMFLOAT4A f4Parameters {};
		f4Parameters.x = fBlurredTextureIndex;
		f4Parameters.y = fLightingIntensity * fIntensityMultiplier;
		rAreaLayout.pf4Parameters[0] = f4Parameters;
		rAreaLayout.pf4Parameters[1] = f4Parameters;
		rAreaLayout.pf4Parameters[2] = f4Parameters;
		rAreaLayout.pf4Parameters[3] = f4Parameters;
		rAreaLayout.uiColor = rType.puiColors[0];

		++spiDepositsRendered[iLayer];
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
	// Only the refreshing lighting layer deposits; a frame with no refresh deposits nothing.
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineLighting].at(DynamicPipelines::FlightPlaneDepositKey(kuiCrc, i))->WriteIndirectBuffer(iCommandBuffer, gLightingEnabled.Get<bool>() && i == giLightingRefreshLayer ? spiDepositsRendered[i] : 0);
	}
	gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineVisibleLights].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

} // namespace engine

#endif // BT_CLIENT
