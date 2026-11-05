#include "ShipHitboxes.h"

#include "File/PackChunks.h"
#if defined(BT_CLIENT)
#include "Graphics/Debug/DebugRender.h"
#endif

namespace game
{

void InitializeShipHitboxes()
{
	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	for (int64_t i = 0; i < kiShipTypeCount; ++i)
	{
		const ShipHitboxType& rType = kShipHitboxTypes[i];
		for (int64_t j = 0; j < kiShipControllerCount; ++j)
		{
			const engine::EagerChunk& rChunk = rChunkMap.at(rType.models[j]);
			const common::ModelHeader& rModelHeader = rChunk.pHeader->modelHeader;
			// Ship models require uniform authored scale.
			ASSERT(rModelHeader.f3Scale.x == rModelHeader.f3Scale.y && rModelHeader.f3Scale.x == rModelHeader.f3Scale.z);

			int64_t iOutlineOffset = common::ModelHeader::OutlineOffset(rChunk.pHeader->iSize);
			ASSERT(iOutlineOffset + static_cast<int64_t>(sizeof(common::ModelOutline)) <= rChunk.iDataSize);
			const common::ModelOutline& rOutline = *reinterpret_cast<const common::ModelOutline*>(rChunk.pData + iOutlineOffset);
			ASSERT(rOutline.iHullVertexCount >= 3 && rOutline.iHullVertexCount <= common::kiModelOutlineVertexMaximum);

			ShipHitboxShape& rShape = gShipHitboxShapes[i][j];
			rShape.fMinimumOriginZ = 1.0f - rOutline.fMinimumZ;
			engine::CollisionHitbox& rHitbox = rShape.hitbox;
			rHitbox.eHitboxLevel = rOutline.eHitboxLevel;
			rHitbox.fMinimumZ = rOutline.fMinimumZ;
			rHitbox.fMaximumZ = rOutline.fMaximumZ;
			rHitbox.iHullVertexCount = rOutline.iHullVertexCount;

			XMFLOAT2 f2BoxMinimum {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
			XMFLOAT2 f2BoxMaximum {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
			float fHullLengthSquared = 0.0f;
			for (int64_t k = 0; k < rHitbox.iHullVertexCount; ++k)
			{
				XMFLOAT2 f2Vertex = rOutline.pf2HullVertices[k];
				rHitbox.pf2HullVertices[k] = f2Vertex;
				f2BoxMinimum.x = std::min(f2BoxMinimum.x, f2Vertex.x);
				f2BoxMinimum.y = std::min(f2BoxMinimum.y, f2Vertex.y);
				f2BoxMaximum.x = std::max(f2BoxMaximum.x, f2Vertex.x);
				f2BoxMaximum.y = std::max(f2BoxMaximum.y, f2Vertex.y);
				fHullLengthSquared = std::max(fHullLengthSquared, f2Vertex.x * f2Vertex.x + f2Vertex.y * f2Vertex.y);
			}

			rHitbox.pf2BoxVertices[0] = {f2BoxMinimum.x, f2BoxMinimum.y};
			rHitbox.pf2BoxVertices[1] = {f2BoxMaximum.x, f2BoxMinimum.y};
			rHitbox.pf2BoxVertices[2] = {f2BoxMaximum.x, f2BoxMaximum.y};
			rHitbox.pf2BoxVertices[3] = {f2BoxMinimum.x, f2BoxMaximum.y};

			// The sphere encloses the box corners for kBox and the hull otherwise.
			float fHorizontalLengthSquared = fHullLengthSquared;
			if (rHitbox.eHitboxLevel == common::ModelHitboxLevel::kBox)
			{
				fHorizontalLengthSquared = 0.0f;
				for (const XMFLOAT2& rf2Corner : rHitbox.pf2BoxVertices)
				{
					fHorizontalLengthSquared = std::max(fHorizontalLengthSquared, rf2Corner.x * rf2Corner.x + rf2Corner.y * rf2Corner.y);
				}
			}
			float fHeightSquared = std::max(rHitbox.fMinimumZ * rHitbox.fMinimumZ, rHitbox.fMaximumZ * rHitbox.fMaximumZ);
			rShape.fRadius = std::sqrt(fHorizontalLengthSquared + fHeightSquared);
			rShape.fPusherRadius = rShape.fRadius * rType.fPusherRadiusScale;
			gfLargestShipPusherRadius = std::max(gfLargestShipPusherRadius, rShape.fPusherRadius);
		}
	}
}

void XM_CALLCONV BuildShipHitbox(ShipType eType, ShipController eController, FXMVECTOR vecPosition, FXMVECTOR vecDirection, engine::CollisionHitbox& rHitbox)
{
	const engine::CollisionHitbox& rShapeHitbox = gShipHitboxShapes[std::to_underlying(eType)][std::to_underlying(eController)].hitbox;
	rHitbox.eHitboxLevel = rShapeHitbox.eHitboxLevel;
	if (rShapeHitbox.eHitboxLevel == common::ModelHitboxLevel::kSphere)
	{
		return;
	}

	// The heading is a horizontal unit vector, so (x, y) is the yaw's (cos, sin) with no trigonometry.
	XMFLOAT2 f2Position {XMVectorGetX(vecPosition), XMVectorGetY(vecPosition)};
	float fCos = XMVectorGetX(vecDirection);
	float fSin = XMVectorGetY(vecDirection);
	float fHeight = XMVectorGetZ(vecPosition);
	rHitbox.fMinimumZ = fHeight + rShapeHitbox.fMinimumZ;
	rHitbox.fMaximumZ = fHeight + rShapeHitbox.fMaximumZ;
	common::BuildWorldHull(rShapeHitbox.pf2BoxVertices, f2Position, fCos, fSin, rHitbox.pf2BoxVertices);

	if (rShapeHitbox.eHitboxLevel == common::ModelHitboxLevel::kOutline)
	{
		rHitbox.iHullVertexCount = rShapeHitbox.iHullVertexCount;
		common::BuildWorldHull(std::span<const XMFLOAT2>(rShapeHitbox.pf2HullVertices, static_cast<size_t>(rShapeHitbox.iHullVertexCount)), f2Position, fCos, fSin, rHitbox.pf2HullVertices);
	}
}

#if defined(BT_CLIENT)
XMVECTOR XM_CALLCONV ShipHullContact(ShipType eType, ShipController eController, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecShooterPosition)
{
	const engine::CollisionHitbox& rHitbox = gShipHitboxShapes[std::to_underlying(eType)][std::to_underlying(eController)].hitbox;
	XMVECTOR vecOffset = XMVectorSubtract(vecShooterPosition, vecPosition);
	float fX = XMVectorGetX(vecOffset);
	float fY = XMVectorGetY(vecOffset);
	float fCosine = XMVectorGetX(vecDirection);
	float fSine = XMVectorGetY(vecDirection);
	XMFLOAT3A f3Start {fCosine * fX + fSine * fY, -fSine * fX + fCosine * fY, XMVectorGetZ(vecOffset)};
	XMFLOAT3A f3Delta {-f3Start.x, -f3Start.y, -f3Start.z};
	float fEnter = 0.0f;
	float fExit = std::numeric_limits<float>::max();
	bool bHit = engine::ClipSegmentToPrism(f3Start, f3Delta, std::span<const XMFLOAT2>(rHitbox.pf2HullVertices, static_cast<size_t>(rHitbox.iHullVertexCount)), rHitbox.fMinimumZ, rHitbox.fMaximumZ, fEnter, fExit);
	ASSERT(bHit);
	// Extending through the center reaches the far surface when overlapping ships put the shooter inside the hull.
	float fContact = fEnter > 0.0f ? fEnter : fExit;
	ASSERT(std::isfinite(fContact));
	return XMVectorSetW(XMVectorMultiplyAdd(XMVectorReplicate(fContact), XMVectorSubtract(vecPosition, vecShooterPosition), vecShooterPosition), 1.0f);
}

static void DebugRenderPrism(const engine::RenderBasis& rBasis, std::span<const XMFLOAT2> vertices, float fMinimumZ, float fMaximumZ, const XMFLOAT4A& rf4Color)
{
	auto Corner = [&](int64_t i, float fZ)
	{
		XMFLOAT3A f3Corner {};
		XMStoreFloat3A(&f3Corner, engine::Rebase(rBasis, XMVectorSet(vertices[i].x, vertices[i].y, fZ, 1.0f)));
		return f3Corner;
	};

	for (int64_t i = 0; i < std::ssize(vertices); ++i)
	{
		int64_t iNext = (i + 1) % std::ssize(vertices);
		XMFLOAT3A f3Bottom = Corner(i, fMinimumZ);
		XMFLOAT3A f3Top = Corner(i, fMaximumZ);
		engine::DebugRender::Line(f3Bottom, Corner(iNext, fMinimumZ), rf4Color);
		engine::DebugRender::Line(f3Top, Corner(iNext, fMaximumZ), rf4Color);
		engine::DebugRender::Line(f3Bottom, f3Top, rf4Color);
	}
}

void XM_CALLCONV DebugRenderShipHitbox(const engine::RenderBasis& rBasis, ShipType eType, ShipController eController, FXMVECTOR vecPosition, FXMVECTOR vecDirection)
{
	engine::CollisionHitbox hitbox {};
	BuildShipHitbox(eType, eController, vecPosition, vecDirection, hitbox);

	XMFLOAT3A f3Center {};
	XMStoreFloat3A(&f3Center, engine::Rebase(rBasis, vecPosition));
	engine::DebugRender::Sphere(f3Center, gShipHitboxShapes[std::to_underlying(eType)][std::to_underlying(eController)].fRadius, {1.0f, 1.0f, 1.0f, 1.0f});

	if (hitbox.eHitboxLevel >= common::ModelHitboxLevel::kBox)
	{
		DebugRenderPrism(rBasis, hitbox.pf2BoxVertices, hitbox.fMinimumZ, hitbox.fMaximumZ, {1.0f, 1.0f, 0.0f, 1.0f});
	}
	if (hitbox.eHitboxLevel == common::ModelHitboxLevel::kOutline)
	{
		DebugRenderPrism(rBasis, std::span<const XMFLOAT2>(hitbox.pf2HullVertices, static_cast<size_t>(hitbox.iHullVertexCount)), hitbox.fMinimumZ, hitbox.fMaximumZ, {0.0f, 1.0f, 1.0f, 1.0f});
	}
}
#endif // BT_CLIENT

} // namespace game
