#include "AreaDamage.h"

namespace engine
{

// StableVector construction only records the reserved count, so this reserves no address space and makes
// no OS call until the first Resize; growth then commits more of that reservation without moving.
thread_local common::StableVector<AreaDamageSource> AreaDamage::sAreaDamageSources {64 * kiAreaDamageSourcePreallocate};
thread_local int64_t AreaDamage::siAreaDamageSourceCount = 0;

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

float AreaDamage::Get(FXMVECTOR vecPosition, uint16_t uiCategoryMask, XMVECTOR& rvecClosestSource)
{
	float fTotalDamage = 0.0f;
	float fClosestDistance = std::numeric_limits<float>::max();
	rvecClosestSource = vecPosition;

	for (int64_t i = 0; i < siAreaDamageSourceCount; ++i)
	{
		const AreaDamageSource& rSource = sAreaDamageSources[i];
		// Filter by category
		if ((rSource.uiCategory & uiCategoryMask) == 0)
		{
			continue;
		}

		// Calculate distance
		XMVECTOR vecDiff = XMVectorSubtract(vecPosition, rSource.vecPosition);
		float fDistance = XMVectorGetX(XMVector3Length(vecDiff));

		// Skip if outside radius
		if (fDistance >= rSource.fRadius)
		{
			continue;
		}

		// Track closest source
		if (fDistance < fClosestDistance)
		{
			fClosestDistance = fDistance;
			rvecClosestSource = rSource.vecPosition;
		}

		// Linear falloff: full damage at center, zero at edge
		float fFalloff = 1.0f - (fDistance / rSource.fRadius);
		fTotalDamage += rSource.fDamage * fFalloff;
	}

	return fTotalDamage;
}

void AreaDamage::Clear()
{
	siAreaDamageSourceCount = 0;
}

} // namespace engine
