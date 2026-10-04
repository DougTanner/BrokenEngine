#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class ParticleManager
{
public:

	static void Spawn(shaders::ParticlesSpawnLayout& rParticlesSpawnLayout, shaders::ParticleLayout layout, common::crc_t uiTextureCrc);

	ParticleManager();
	~ParticleManager();

	void RenderGlobal(int64_t iCommandBuffer);

	std::mutex mSpawnMutex;

	bool mbReset = true;

	shaders::ParticlesSpawnLayout mLongParticlesSpawnLayout {};
	shaders::ParticlesSpawnLayout mSquareParticlesSpawnLayout {};

};

inline ParticleManager* gpParticleManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
