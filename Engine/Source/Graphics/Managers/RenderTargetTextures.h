#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class RenderTargetTextures
{
public:
	void Create();

	void DestroyLightingTextures();
	void CreateLightingTextures();
	void RegisterDebugTextures(int64_t iPassCount);
	void CreateShadowTextures();
	void CreateSmokeTextures();
	void CreateWindTextures();
	// Zeroes both wind textures and their occupancy. Callers have drained the graphics queue.
	void ClearWindTextures();
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

	Texture mpLightingTextures[3];
	Texture* mppLightingDepositTextures[3] {};
	VkRenderPass mLightingVkRenderPass = VK_NULL_HANDLE;
	VkFramebuffer mLightingVkFramebuffer = VK_NULL_HANDLE;

	// Spread output textures [pass][color R/G/B]
	// mpSpreadTextures: accumulated output fed into next pass
	// mpSpreadOnlyTextures: pre-accumulation snapshot read by LightCombine
	Texture mpSpreadTextures[shaders::kiMaxSpreadPasses][3];
	Texture mpSpreadOnlyTextures[shaders::kiMaxSpreadPasses][3];
	VkRenderPass mSpreadVkRenderPass = VK_NULL_HANDLE;
	VkFramebuffer mpSpreadVkFramebuffers[shaders::kiMaxSpreadPasses] {};

	// Combine outputs and history persist per lighting layer [layer][color]; the deposit and spread textures above are
	// one scratch chain the refreshing layer reuses.
	Texture mpCombineTextures[shaders::kiLightingLayers][3];
	Texture* mppLightingFinalTextures[shaders::kiLightingLayers * 3] {}; // Layer-major sampler-array binding

	// Direction-averaged 0.25*(E+W+N+S) of the tone-mapped per-direction values.
	// Terrain/Water read it with one texture sample in the ambient path.
	Texture mpAmbientCombineTextures[shaders::kiLightingLayers];
	Texture* mppAmbientCombineTextures[shaders::kiLightingLayers] {}; // Sampler-array binding

	// Persistent last-refresh combined lighting (3 directional + ambient), reprojected by LightingTemporal.comp to
	// de-flicker the texel-ramp resample (mirror of mShadowHistoryTexture). LightingHistoryCopy.comp publishes the
	// temporal result over the whole texture on refresh.
	Texture mpLightingHistoryTextures[shaders::kiLightingLayers][3];
	Texture mpAmbientHistoryTextures[shaders::kiLightingLayers];

	// Debug texture array (channels A/B/C used by formats needing 3 textures per slot, e.g., spread direction combined)
	int64_t miDebugTextureCount = 0;
	Texture* mppDebugTextures[shaders::kiMaxDebugTextures] {};
	Texture* mppDebugTexturesB[shaders::kiMaxDebugTextures] {};
	Texture* mppDebugTexturesC[shaders::kiMaxDebugTextures] {};
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
