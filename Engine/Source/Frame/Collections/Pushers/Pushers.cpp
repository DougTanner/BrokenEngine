#include "Pushers.h"

namespace engine
{

template struct Collection<PushersInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<PushersPostRender>;

void PushersInterpolate::AllocateAndCopy(PushersInterpolate& rCurrent, const PushersInterpolate& rPrevious)
{
	Allocate(rCurrent, rPrevious, rCurrent.Members());
}

bool PushersInterpolate::LogDifferences(const PushersInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("PushersInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pfRadii">(i, pfRadii[i], rOther.pfRadii[i]);
		bEqual &= common::LogDifference<"pfIntensities">(i, pfIntensities[i], rOther.pfIntensities[i]);
		bEqual &= common::LogDifference<"puiPowers">(i, puiPowers[i], rOther.puiPowers[i]);
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
	}

	return bEqual;
}

bool PushersPostRender::LogDifferences(const PushersPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("PushersPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pIds">(i, pIds[i], rOther.pIds[i]);
		bEqual &= common::LogDifference<"pLifecycleFlags">(i, pLifecycleFlags[i], rOther.pLifecycleFlags[i]);
	}

	return bEqual;
}

} // namespace engine
