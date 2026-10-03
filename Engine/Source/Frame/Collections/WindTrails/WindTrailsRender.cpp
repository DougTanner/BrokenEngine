#include "WindTrails.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

static int64_t siRendered = 0;

void WindTrailsInterpolate::GraphicsResources()
{
	gpBufferManager->CreateDynamicBuffer(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineWindDepositA(kuiCrc, kpcName, sizeof(shaders::QuadLayout));
	gpPipelineManager->mDynamicPipelines.CreatePipelineWindDepositB(kuiCrc, kpcName);
}

void WindTrailsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	EraseStaleRenderState(sPreviousPositions, rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const std::unordered_map<WindTrailsInterpolate::id_t, int64_t>&
	{
		return rInterpolate.windTrails.idToIndexMap;
	});

	if (!gWindEnabled.Get<bool>())
	{
		return;
	}

	int64_t iTotalCapacity = AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const WindTrailsInterpolate&
	{
		return rInterpolate.windTrails;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, kBufferMain, kpcName, sizeof(shaders::QuadLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositA].at(kuiCrc)->UpdateStorageBufferDescriptor(iCommandBuffer, 1, pBuffer);
		gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositB].at(kuiCrc)->UpdateStorageBufferDescriptor(iCommandBuffer, 1, pBuffer);
	}
}

void WindTrailsInterpolate::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	const WindTrailsInterpolate& rCurrent = rFrameInterpolate.windTrails;
	// sPreviousPositions caches a trail's position in its own cell's frame, which is the frame this render reads it
	// back in, so the cache needs no conversion of its own.
	const RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (!gWindEnabled.Get<bool>())
	{
		// Heap: unordered_map insertions/lookups for per-trail previous positions
		ScopedSuppressAllocationTracking suppress;

		for (const auto& [rId, rIndex] : rCurrent.idToIndexMap)
		{
			sPreviousPositions.insert_or_assign(rId, rCurrent.pVecPositions[rIndex]);
		}
		return;
	}

	if (rCurrent.iCount == 0)
	{
		return;
	}

	auto [pQuadLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::QuadLayout>(kuiCrc, kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	{
		// Heap: unordered_map insertions/lookups for per-trail previous positions
		ScopedSuppressAllocationTracking suppress;

		for (const auto& [rId, rIndex] : rCurrent.idToIndexMap)
		{
			XMVECTOR vecLocalPosition = rCurrent.pVecPositions[rIndex];
			float fIntensity = rCurrent.pfIntensities[rIndex];
			float fWidth = rCurrent.pfWidths[rIndex];

			XMFLOAT4A f4Position {};
			if (!IsPointVisible(Rebase(rBasis, vecLocalPosition), f4Position))
			{
				continue;
			}

			auto it = sPreviousPositions.find(rId);
			XMVECTOR vecLocalPreviousPosition = (it != sPreviousPositions.end()) ? it->second : vecLocalPosition;
			float fLengthMultiplier = rCurrent.pfLengthMultipliers[rIndex];

			XMVECTOR vecBasePosition = ProjectToBaseHeight(vecLocalPosition, rBasis);
			XMVECTOR vecBasePreviousPosition = ProjectToBaseHeight(vecLocalPreviousPosition, rBasis);

			XMVECTOR vecDisplacement = XMVectorSubtract(vecBasePosition, vecBasePreviousPosition);
			vecDisplacement = XMVectorScale(vecDisplacement, fLengthMultiplier);
			vecBasePreviousPosition = XMVectorSubtract(vecBasePosition, vecDisplacement);
			float fDistance = XMVectorGetX(XMVector3Length(vecDisplacement));
			if (fDistance <= 0.001f)
			{
				continue;
			}

			XMVECTOR vecDirectionNormal = XMVector3Normalize(vecDisplacement);

			XMVECTOR vecPerpendicularNormal = XMVector3Normalize(XMVector3Cross(vecDirectionNormal, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)));

			XMVECTOR vecFrontLeft = XMVectorAdd(vecBasePosition, XMVectorScale(vecPerpendicularNormal, fWidth));
			XMVECTOR vecFrontRight = XMVectorSubtract(vecBasePosition, XMVectorScale(vecPerpendicularNormal, fWidth));
			XMVECTOR vecBackLeft = XMVectorAdd(vecBasePreviousPosition, XMVectorScale(vecPerpendicularNormal, fWidth));
			XMVECTOR vecBackRight = XMVectorSubtract(vecBasePreviousPosition, XMVectorScale(vecPerpendicularNormal, fWidth));

			float fWindDirectionX = XMVectorGetX(vecDirectionNormal);
			float fWindDirectionY = XMVectorGetY(vecDirectionNormal);

			XMFLOAT4A f4Vertex {};

			XMStoreFloat4A(&f4Vertex, vecFrontLeft);
			pQuadLayouts[siRendered].pf4VerticesTexcoords[0] = {f4Vertex.x, f4Vertex.y, 0.0f, 1.0f};
			XMStoreFloat4A(&f4Vertex, vecFrontRight);
			pQuadLayouts[siRendered].pf4VerticesTexcoords[1] = {f4Vertex.x, f4Vertex.y, 1.0f, 1.0f};
			XMStoreFloat4A(&f4Vertex, vecBackLeft);
			pQuadLayouts[siRendered].pf4VerticesTexcoords[2] = {f4Vertex.x, f4Vertex.y, 0.0f, 0.0f};
			XMStoreFloat4A(&f4Vertex, vecBackRight);
			pQuadLayouts[siRendered].pf4VerticesTexcoords[3] = {f4Vertex.x, f4Vertex.y, 1.0f, 0.0f};

			XMFLOAT4A f4Parameters = {fIntensity, fWindDirectionX, fWindDirectionY, 0.0f};
			pQuadLayouts[siRendered].pf4Params[0] = f4Parameters;
			pQuadLayouts[siRendered].pf4Params[1] = f4Parameters;
			pQuadLayouts[siRendered].pf4Params[2] = f4Parameters;
			pQuadLayouts[siRendered].pf4Params[3] = f4Parameters;

			pQuadLayouts[siRendered].f4Params = {};
			pQuadLayouts[siRendered].uiColor = 0xFFFFFFFF;

			++siRendered;
		}

		// Snapshot current positions as previous for next render
		for (const auto& [rId, rIndex] : rCurrent.idToIndexMap)
		{
			sPreviousPositions.insert_or_assign(rId, rCurrent.pVecPositions[rIndex]);
		}
	}
}

void WindTrailsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositA].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, giWindTextureIndex == 0 ? siRendered : 0);
	gpPipelineManager->mDynamicPipelines.mPipelineMaps[kDynamicPipelineWindDepositB].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, giWindTextureIndex == 1 ? siRendered : 0);
}

} // namespace engine

#endif // BT_CLIENT
