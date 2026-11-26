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
	// Clears lighting deposit, active spread, combine, and history targets to black (ambient alpha 1.0) and arms gbLightingTemporalReset. Callers have drained the graphics queue.
	void ClearLightingTextures();
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
	Texture mSmokeTextureOne;
	Texture mSmokeTextureTwo;

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

	// Combine outputs and history persist across skipped lighting refreshes.
	Texture mpCombineTextures[3];
	Texture* mppLightingFinalTextures[3] {}; // Sampler-array binding

	// Direction-averaged 0.25*(E+W+N+S) of the finalized directional combine values, written by
	// LightingHistoryCopy.comp. Terrain/Water read it with one texture sample in the ambient path.
	Texture mAmbientCombineTexture;

	// Persistent last-refresh directional combined lighting (R/G/B), reprojected by LightingTemporal.comp to
	// de-flicker the texel-ramp resample (mirror of mShadowHistoryTexture). LightingHistoryCopy.comp publishes the
	// temporal result over the whole texture on refresh.
	Texture mpLightingHistoryTextures[3];

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
