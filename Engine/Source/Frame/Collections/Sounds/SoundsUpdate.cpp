#include "Sounds.h"

#if defined(BT_CLIENT)

namespace engine
{

void SoundsInterpolate::Update([[maybe_unused]] game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
}

void SoundsInterpolate::Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData)
{
	SoundsInterpolate& rSounds = rFrameInterpolate.sounds;
	int64_t iIndex = rSounds.idToIndexMap.at(id);

	rSounds.pVecPositions[iIndex] = XMVectorSetW(rData.vecPosition, 1.0f);
	rSounds.pVecVelocities[iIndex] = rData.vecVelocity;
	rSounds.puiCrcs[iIndex] = rData.uiCrc;
	rSounds.pfVolumes[iIndex] = rData.fVolume;
	rSounds.pfPitches[iIndex] = rData.fPitch;
	rSounds.pfFadeOutTimes[iIndex] = rData.fFadeOutTime;
}

void SoundsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

void SoundsPostRender::Add(game::Frame& __restrict rFrame, sound_t& rId)
{
	ASSERT(!(rId.uuid.iValue != 0));

	SoundsInterpolate& rInterpolate = rFrame.interpolate.sounds;
	SoundsPostRender& rPostRender = rFrame.postRender.sounds;

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	auto [iSpawnIndex, newId] = AddGeneratedIndexableElement(rInterpolate, rPostRender, [&rFrame]()
	{
		return sound_t {Uuid {rFrame.postRender.MakeUuid(rFrame.postRender.uiNextSoundUuid)}};
	});
	rId = newId;
	rPostRender.pIds[iSpawnIndex] = newId;
	ZeroMemberRow(iSpawnIndex, rInterpolate.Members());
}

} // namespace engine

#endif // BT_CLIENT
