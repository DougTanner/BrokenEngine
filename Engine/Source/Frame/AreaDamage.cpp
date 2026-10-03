#include "AreaDamage.h"

namespace engine
{

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
		if ((rSource.uiCategory & uiCategoryMask) == 0)
		{
			continue;
		}

		XMVECTOR vecDifference = XMVectorSubtract(vecPosition, rSource.vecPosition);
		float fDistance = XMVectorGetX(XMVector3Length(vecDifference));

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
