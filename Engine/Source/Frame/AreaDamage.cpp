#include "AreaDamage.h"

#include "Frame/Collision.h"

namespace engine
{

// 3D distance from rf3Point to the CCW convex polygon extruded over [fMinimumZ, fMaximumZ]; 0 inside.
static float DistanceToPrism(const XMFLOAT3A& rf3Point, std::span<const XMFLOAT2> vertices, float fMinimumZ, float fMaximumZ)
{
	bool bInside = true;
	float fEdgeDistanceSquared = std::numeric_limits<float>::max();
	for (int64_t i = 0; i < std::ssize(vertices); ++i)
	{
		const XMFLOAT2& rStartVertex = vertices[i];
		const XMFLOAT2& rEndVertex = vertices[(i + 1) % std::ssize(vertices)];
		float fEdgeX = rEndVertex.x - rStartVertex.x;
		float fEdgeY = rEndVertex.y - rStartVertex.y;
		float fOffsetX = rf3Point.x - rStartVertex.x;
		float fOffsetY = rf3Point.y - rStartVertex.y;
		// The interior of a CCW polygon lies left of every edge.
		if (fEdgeX * fOffsetY - fEdgeY * fOffsetX < 0.0f)
		{
			bInside = false;
		}
		float fEdgeTime = std::clamp((fOffsetX * fEdgeX + fOffsetY * fEdgeY) / (fEdgeX * fEdgeX + fEdgeY * fEdgeY), 0.0f, 1.0f);
		float fClosestX = fOffsetX - fEdgeTime * fEdgeX;
		float fClosestY = fOffsetY - fEdgeTime * fEdgeY;
		fEdgeDistanceSquared = std::min(fEdgeDistanceSquared, fClosestX * fClosestX + fClosestY * fClosestY);
	}
	float fHorizontalDistanceSquared = bInside ? 0.0f : fEdgeDistanceSquared;
	float fVerticalDistance = std::max({fMinimumZ - rf3Point.z, rf3Point.z - fMaximumZ, 0.0f});
	return std::sqrt(fHorizontalDistanceSquared + fVerticalDistance * fVerticalDistance);
}

void AreaDamage::Add(const AreaDamageSource& rSource)
{
	int64_t iIndex = siAreaDamageSourceCount;
	if (sAreaDamageSources.Size() == 0)
	{
		sAreaDamageSources.Resize(kiAreaDamageSourcePreallocate);
	}

	if (siAreaDamageSourceCount >= sAreaDamageSources.Size())
	{
		LOG(kDefault, kWarning, "AreaDamage: sAreaDamageSources overflow (count: {}, capacity: {}). Increase kiAreaDamageSourcePreallocate in AreaDamage.h", siAreaDamageSourceCount, sAreaDamageSources.Size());
		DEBUG_BREAK();
		sAreaDamageSources.Resize(siAreaDamageSourceCount * 2);
	}
	sAreaDamageSources[iIndex] = rSource;
	++siAreaDamageSourceCount;
}

float AreaDamage::Get(FXMVECTOR vecPosition, const CollisionHitbox& rHitbox, uint16_t uiCategoryMask, XMVECTOR& rvecClosestSource)
{
	float fTotalDamage = 0.0f;
	float fClosestDistance = std::numeric_limits<float>::max();
	rvecClosestSource = vecPosition;
	std::span<const XMFLOAT2> shapeVertices = rHitbox.eHitboxLevel == common::ModelHitboxLevel::kOutline ? std::span<const XMFLOAT2>(rHitbox.pf2HullVertices, static_cast<size_t>(rHitbox.iHullVertexCount)) : std::span<const XMFLOAT2>(rHitbox.pf2BoxVertices);

	for (int64_t i = 0; i < siAreaDamageSourceCount; ++i)
	{
		const AreaDamageSource& rSource = sAreaDamageSources[i];
		if ((rSource.uiCategory & uiCategoryMask) == 0)
		{
			continue;
		}

		float fDistance = 0.0f;
		if (rHitbox.eHitboxLevel == common::ModelHitboxLevel::kSphere)
		{
			XMVECTOR vecDifference = XMVectorSubtract(vecPosition, rSource.vecPosition);
			fDistance = XMVectorGetX(XMVector3Length(vecDifference));
		}
		else
		{
			XMFLOAT3A f3Source {};
			XMStoreFloat3A(&f3Source, rSource.vecPosition);
			fDistance = DistanceToPrism(f3Source, shapeVertices, rHitbox.fMinimumZ, rHitbox.fMaximumZ);
		}

		if (fDistance >= rSource.fRadius)
		{
			continue;
		}

		if (fDistance < fClosestDistance)
		{
			fClosestDistance = fDistance;
			rvecClosestSource = rSource.vecPosition;
		}

		float fFalloff = 1.0f - (fDistance / rSource.fRadius);
		fTotalDamage += rSource.fDamage * fFalloff;
	}

	return fTotalDamage;
}

} // namespace engine
