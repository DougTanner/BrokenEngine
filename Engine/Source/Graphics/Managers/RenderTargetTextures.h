#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class RenderTargetTextures
{
public:
	void Create();

	void RegisterDebugTextures();
	void CreateShadowTextures();
	void CreateSmokeTextures();
	void CreateWindTextures();
	void CreateObjectShadowsTextures();
	void CreateTerrainTextures();
	void CreateWaterDisplacementTextures();
	void CreateWaterFoamTextures();

	Texture mLogTexture;

	Texture mTerrainElevationTexture;

	Texture mSmokeGradientTexture;
	// One ping-pong pair per flight plane; only the first gpFlightPlanes->Count() of each are created.
	std::array<Texture, shaders::kiMaxFlightPlanes> mSmokeTexturesOne;
	std::array<Texture, shaders::kiMaxFlightPlanes> mSmokeTexturesTwo;
	Texture* mppSmokeTexturesOne[shaders::kiMaxFlightPlanes] {}; // Sampler-array binding; slots >= plane count alias plane 0 (never sampled)

	Texture mWindTextureOne;
	Texture mWindTextureTwo;

	int64_t miDebugTextureCount = 0;
	Texture* mppDebugTextures[shaders::kiMaxDebugTextures] {};
	int64_t mpiDebugTextureFormats[shaders::kiMaxDebugTextures] {};

	Texture mShadowElevationTexture;
	Texture mShadowTexture;
	Texture mShadowBlurTexture;
	Texture mShadowBlurIntermediateTexture;
	Texture mShadowHistoryTexture; // Last-refresh final shadow, refreshed by ShadowHistoryCopy.comp for ShadowTemporal reprojection

	Texture mObjectShadowsTexture;
	Texture mObjectShadowsBlurTexture;
	Texture mObjectShadowsBlurIntermediateTexture;

	// Per-frame compute output: Gerstner-wave displacement and Jacobian normal sampled by Water.vert (kPipelineWater).
	// Storage matches the LOD0 water-mesh vertex grid; the active LOD writes only the top-left
	// (iWaterActiveQuadX + 1) by (iWaterActiveQuadY + 1) texel rectangle each frame.
	Texture mWaterDisplacementTexture;
	Texture mWaterDisplacementNormalTexture;

	Texture mWaterFoamCurrentTexture;
	Texture mWaterFoamHistoryTexture;

	std::vector<Texture*> mElevationTextures;
	std::vector<Texture*> mColorTextures;
	std::vector<Texture*> mNormalsTextures;
	std::vector<Texture*> mAmbientOcclusionTextures;
	std::vector<Texture*> mMasksTextures;
};

} // namespace engine

#endif // defined(BT_CLIENT)
