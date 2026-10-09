#if defined(BT_CLIENT)

#include "ParticleManager.h"

namespace engine
{

ParticleManager::ParticleManager()
: common::Singleton<ParticleManager>(gpParticleManager)
{
	ScopedBootTimer scopedBootTimer(kBootTimerParticleManager);
}

void ParticleManager::Spawn(shaders::ParticlesSpawnLayout& rParticlesSpawnLayout, shaders::ParticleLayout layout, common::crc_t uiTextureCrc)
{
	// Cull before locking: both checks use the by-value layout and immutable camera rect, so culled spawns do not take mSpawnMutex during
	// parallel dispatch.
	if (layout.f4Position.x < engine::gpCamera->mf4RenderVisibleArea.x || layout.f4Position.x > engine::gpCamera->mf4RenderVisibleArea.z || layout.f4Position.y > engine::gpCamera->mf4RenderVisibleArea.y || layout.f4Position.y < engine::gpCamera->mf4RenderVisibleArea.w)
	{
		return;
	}

	if (layout.fVisibleIntensity <= 0.0f)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(gpParticleManager->mSpawnMutex);

	if (rParticlesSpawnLayout.iCount == shaders::kiMaxParticlesSpawn)
	{
		DEBUG_BREAK();
		return;
	}

	layout.iCookie = static_cast<int32_t>(gpTextureManager->mTextureDescriptors.CrcToIndex(uiTextureCrc));
	layout.fSpawnIntensity = layout.fVisibleIntensity;
	rParticlesSpawnLayout.pParticles[rParticlesSpawnLayout.iCount] = layout;
	++rParticlesSpawnLayout.iCount;
}

void ParticleManager::RenderGlobal(int64_t iCommandBuffer)
{
	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	shaders::ParticlesSpawnLayout& rLongParticlesSpawnLayout = *reinterpret_cast<shaders::ParticlesSpawnLayout*>(&gpBufferManager->mLongParticlesSpawnStorageBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	shaders::ParticlesSpawnLayout& rSquareParticlesSpawnLayout = *reinterpret_cast<shaders::ParticlesSpawnLayout*>(&gpBufferManager->mSquareParticlesSpawnStorageBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	float fStretchVelocityStart = 1.0f;
	float fStretchVelocityEnd = 10.0f;
	rGlobalLayout.fParticlesStretchVelocityStart = fStretchVelocityStart;
	rGlobalLayout.fParticlesStretchVelocityMultiplier = 2.0f;
	// Compute the stretch-range reciprocal once on the CPU; it is invariant across shader invocations.
	rGlobalLayout.fParticlesStretchRangeInverse = 1.0f / (fStretchVelocityEnd - fStretchVelocityStart);

	rLongParticlesSpawnLayout.iCount = mLongParticlesSpawnLayout.iCount;
	std::memcpy(&rLongParticlesSpawnLayout.pParticles[0], &mLongParticlesSpawnLayout.pParticles[0], rLongParticlesSpawnLayout.iCount * sizeof(shaders::ParticleLayout));
	mLongParticlesSpawnLayout.iCount = 0;

	rSquareParticlesSpawnLayout.iCount = mSquareParticlesSpawnLayout.iCount;
	std::memcpy(&rSquareParticlesSpawnLayout.pParticles[0], &mSquareParticlesSpawnLayout.pParticles[0], rSquareParticlesSpawnLayout.iCount * sizeof(shaders::ParticleLayout));
	mSquareParticlesSpawnLayout.iCount = 0;

	// Live particles and this iteration's staged spawns are in the previous camera cell's frame. A one-cell step shifts
	// them in the update pass; a larger step leaves nothing worth keeping, so it clears the pool. Written every call
	// because each command buffer has its own global uniform.
	static RetainedAreaBasis sRetainedAreaBasis {};
	if (std::optional<XMFLOAT2> of2Shift = sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate))
	{
		rGlobalLayout.f2ParticlesBasisShift.x = of2Shift->x;
		rGlobalLayout.f2ParticlesBasisShift.y = of2Shift->y;
	}
	else
	{
		rGlobalLayout.f2ParticlesBasisShift = {};
		mbReset = true;
	}

	rLongParticlesSpawnLayout.iReset = mbReset ? 1 : 0;
	rSquareParticlesSpawnLayout.iReset = mbReset ? 1 : 0;
	mbReset = false;
}

void ParticleManager::DiscardStagedSpawns()
{
	mLongParticlesSpawnLayout.iCount = 0;
	mSquareParticlesSpawnLayout.iCount = 0;
}

} // namespace engine

#endif // BT_CLIENT
