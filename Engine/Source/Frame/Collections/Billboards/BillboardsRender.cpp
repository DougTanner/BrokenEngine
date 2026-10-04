#include "Billboards.h"

#if defined(BT_CLIENT)

#include "Graphics/Objects/PipelineDescriptorWriter.h"

#include "Profile/ProfileManager.h"

namespace engine
{

using enum BillboardFlags;

void BillboardsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::BillboardLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineBillboards(kuiCrc, kpcName, sizeof(shaders::BillboardLayout));
}

static int64_t siRendered = 0;
static int64_t siTotalCount = 0;

void BillboardsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;
	siTotalCount = 0;

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const BillboardsInterpolate&
	{
		return rInterpolate.billboards;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferMain, kpcName, sizeof(shaders::BillboardLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineBillboards].at(kuiCrc), iCommandBuffer, 2, pBuffer);
	}
}

void BillboardsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const BillboardsInterpolate& rCurrent = rFrameInterpolate.billboards;
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	siTotalCount += rCurrent.iCount;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	auto [pLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::BillboardLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		uint8_t uiTypeIndex = rCurrent.puiTypeIndices[i];
		BillboardFlags_t flags = rCurrent.pFlags[i];
		float fRotation = rCurrent.pfRotations[i];
		float fExtra = rCurrent.pfExtra[i];
		XMVECTOR vecLocalPosition = rCurrent.pVecPositions[i];

		const BillboardsInterpolate::Type& rType = BillboardsInterpolate::sTypes.at(uiTypeIndex);

		// Convert into the camera cell's frame, then project to clip space — the view matrix is built in that frame.
		XMVECTOR vecProjection = XMVector4Transform(Rebase(rBasis, vecLocalPosition), XMMatrixMultiply(engine::gpCamera->mMatView, engine::gpCamera->mMatPerspective));

		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, vecProjection);
		f4Position.x /= f4Position.w;
		f4Position.y /= f4Position.w;
		f4Position.z /= f4Position.w;
		f4Position.w = 1.0f;

		if (flags & kOffscreenOnly && !(f4Position.x < -1.0f - fExtra || f4Position.x > 1.0f + fExtra || f4Position.y > 1.0f + fExtra || f4Position.y < -1.0f - fExtra))
		{
			continue;
		}

		float fSize = rType.fSize;

		if (flags & kOffscreenOnly)
		{
			f4Position.x = std::clamp(f4Position.x, -1.0f + fSize / gpSwapchainManager->mfAspectRatio, 1.0f - fSize / gpSwapchainManager->mfAspectRatio);
			f4Position.y = std::clamp(f4Position.y, -1.0f + fSize, 1.0f - fSize);
		}

		// Offscreen rotation maps RotationFromPosition's signed heading with 3*pi/2 minus theta.
		if (flags & kOffscreenRotate)
		{
			fRotation = XM_PI + XM_PIDIV2 - common::RotationFromPosition(XMVector3Normalize(XMLoadFloat4A(&f4Position)));
		}

		shaders::BillboardLayout& rBillboardLayout = pLayouts[siRendered];
		rBillboardLayout.f4Position = f4Position;
		rBillboardLayout.fSize = fSize;
		rBillboardLayout.fTextureIndex = static_cast<float>(gpTextureManager->mTextureDescriptors.CrcToIndex(rType.uiCrc));
		rBillboardLayout.fRotation = fRotation;
		rBillboardLayout.fAlpha = rType.fAlpha;

		++siRendered;
	}
}

void BillboardsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterBillboards).iCount = siTotalCount;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterBillboardsRendered).iCount = siRendered;
	}

	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineBillboards].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

} // namespace engine

#endif // BT_CLIENT
