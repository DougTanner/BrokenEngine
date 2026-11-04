#pragma once

#if defined(BT_CLIENT)

namespace game
{

struct FrameInterpolate;

} // namespace game

namespace engine
{


struct ModelShadowBounds
{
	XMFLOAT3A f3Minimum {};
	XMFLOAT3A f3Maximum {};
};

inline ModelShadowBounds ComputeModelShadowBounds(const common::ModelOutline& rOutline)
{
	ModelShadowBounds bounds
	{
		.f3Minimum = {rOutline.pf2HullVertices[0].x, rOutline.pf2HullVertices[0].y, rOutline.fMinimumZ},
		.f3Maximum = {rOutline.pf2HullVertices[0].x, rOutline.pf2HullVertices[0].y, rOutline.fMaximumZ},
	};
	for (int64_t i = 1; i < rOutline.iHullVertexCount; ++i)
	{
		bounds.f3Minimum.x = std::min(bounds.f3Minimum.x, rOutline.pf2HullVertices[i].x);
		bounds.f3Minimum.y = std::min(bounds.f3Minimum.y, rOutline.pf2HullVertices[i].y);
		bounds.f3Maximum.x = std::max(bounds.f3Maximum.x, rOutline.pf2HullVertices[i].x);
		bounds.f3Maximum.y = std::max(bounds.f3Maximum.y, rOutline.pf2HullVertices[i].y);
	}
	return bounds;
}

// Outline bounds already contain authored alignment; apply only the remaining instance transform.
inline XMFLOAT2 XM_CALLCONV ComputeModelShadowRange(const ModelShadowBounds& rBounds, FXMMATRIX matInstanceTransform, FXMVECTOR vecPosition)
{
	XMFLOAT2 f2Range {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
	for (uint64_t i = 0; i < 8; ++i)
	{
		XMVECTOR vecCorner = XMVectorSet((i & 1ui64) != 0 ? rBounds.f3Maximum.x : rBounds.f3Minimum.x, (i & 2ui64) != 0 ? rBounds.f3Maximum.y : rBounds.f3Minimum.y, (i & 4ui64) != 0 ? rBounds.f3Maximum.z : rBounds.f3Minimum.z, 1.0f);
		XMVECTOR vecLateral = XMVectorSubtract(XMVector3TransformCoord(vecCorner, matInstanceTransform), vecPosition);
		float fRelativeX = XMVectorGetX(vecLateral);
		f2Range.x = std::min(f2Range.x, fRelativeX);
		f2Range.y = std::max(f2Range.y, fRelativeX);
	}
	return f2Range;
}

struct WorldSizedTexelArea
{
	XMFLOAT4 f4Area {};
	float fWorldTexelX = 0.0f;
	float fWorldTexelY = 0.0f;
	float fFullWidth = 0.0f;
	float fFullHeight = 0.0f;
};

// The footprint is the framing's straight-down frustum at the reference angle, so field of view and projection never
// move it.
inline WorldSizedTexelArea XM_CALLCONV ComputeWorldSizedTexelArea(float fHeadroomMultiplier, float fTexelFraming, float fTextureWidth, float fTextureHeight, float fAspect, FXMVECTOR vecCameraPosition)
{
	float fTangentHalfFieldOfView = std::tan(0.5f * XMConvertToRadians(CameraBase::kfFramingFieldOfView));
	float fWorldTexelX = (2.0f * fHeadroomMultiplier * fAspect * fTangentHalfFieldOfView / fTextureWidth) * fTexelFraming;
	float fWorldTexelY = (2.0f * fHeadroomMultiplier * fTangentHalfFieldOfView / fTextureHeight) * fTexelFraming;
	float fFullWidth = fTextureWidth * fWorldTexelX;
	float fFullHeight = fTextureHeight * fWorldTexelY;
	XMFLOAT4A f4CameraPosition {};
	XMStoreFloat4A(&f4CameraPosition, vecCameraPosition);
	int64_t iLeftTexel = static_cast<int64_t>(std::floor((f4CameraPosition.x - fFullWidth * 0.5f) / fWorldTexelX));
	int64_t iTopTexel = static_cast<int64_t>(std::floor((f4CameraPosition.y + fFullHeight * 0.5f) / fWorldTexelY));
	float fLeft = static_cast<float>(iLeftTexel) * fWorldTexelX;
	float fTop = static_cast<float>(iTopTexel) * fWorldTexelY;
	return WorldSizedTexelArea
	{
		.f4Area = {fLeft, fTop, fLeft + fFullWidth, fTop - fFullHeight},
		.fWorldTexelX = fWorldTexelX,
		.fWorldTexelY = fWorldTexelY,
		.fFullWidth = fFullWidth,
		.fFullHeight = fFullHeight,
	};
}

// Retained world rectangles (shadow and lighting history, and the held visible area) are expressed in the camera
// cell's frame, so they follow the camera when it changes cell. The whole-cell step keeps GPU history usable for the
// edge-neighbour step the subscription ring allows; a larger step leaves no overlap, so the owner resets instead.
struct RetainedAreaBasis
{
	GridCoord coordinate {};

	[[nodiscard]] std::optional<XMFLOAT2> Advance(GridCoord cameraCoordinate)
	{
		int64_t iStepDistance = GridDistance(coordinate, cameraCoordinate);
		XMFLOAT2 f2Offset = MakeRenderBasis(coordinate, cameraCoordinate).f2Offset;
		coordinate = cameraCoordinate;
		if (iStepDistance > 1)
		{
			return std::nullopt;
		}

		return f2Offset;
	}
};

inline void ShiftArea(XMFLOAT4& rf4Area, XMFLOAT2 f2Offset)
{
	rf4Area.x += f2Offset.x;
	rf4Area.z += f2Offset.x;
	rf4Area.y += f2Offset.y;
	rf4Area.w += f2Offset.y;
}

// Areas are x = left, y = top (max-Y), z = right, w = bottom (min-Y). No margin: the held area must still cover the
// whole live visible area.
inline bool IsVisibleAreaInsideHeldArea(const XMFLOAT4& rf4VisibleArea, const XMFLOAT4& rf4HeldArea)
{
	return rf4VisibleArea.x >= rf4HeldArea.x && rf4VisibleArea.z <= rf4HeldArea.z && rf4VisibleArea.y <= rf4HeldArea.y
	    && rf4VisibleArea.w >= rf4HeldArea.w;
}

struct TemporalAreaLatch
{
	bool bInitialized = false;
	XMFLOAT4 f4CurrentArea {};
	XMFLOAT4 f4PreviousArea {};
	float fBlend = 1.0f;

	float Update(const XMFLOAT4& rf4CurrentArea, bool& rbReset, float fRequestedBlend, XMFLOAT4& rf4PreviousArea)
	{
		if (rbReset)
		{
			rbReset = false;
			bInitialized = false;
		}

		float fResolvedBlend = fRequestedBlend;
		if (!bInitialized)
		{
			f4CurrentArea = rf4CurrentArea;
			f4PreviousArea = rf4CurrentArea;
			bInitialized = true;
			fResolvedBlend = 1.0f;
		}
		else
		{
			f4PreviousArea = f4CurrentArea;
			f4CurrentArea = rf4CurrentArea;
		}

		rf4PreviousArea = f4PreviousArea;
		fBlend = fResolvedBlend;
		return fBlend;
	}
};

// What the last published frame actually sent, for the read-only presentation_continuity_probe agent command; nothing
// in rendering reads it back. Every field is written on the client main loop thread inside RenderFrameGlobal, each by
// its own owner: GlobalUniforms.cpp writes shadow, LightingUniforms.cpp writes pLighting, SmokeUniforms.cpp writes
// smoke, and WaterUniforms.cpp writes the camera basis, the water origins, fNoiseFrequency, and — because it is
// populated last — iPublishedFrames, so the counter advances only for a frame whose whole snapshot is complete. Each
// iHistoryResets counts only the multi-cell basis advances that discarded that owner's history.
struct RetainedAreaReport
{
	XMFLOAT4 f4CurrentArea {};
	XMFLOAT4 f4PreviousArea {};
	int64_t iHistoryResets = 0;
};

struct PresentationContinuitySnapshot
{
	int64_t iPublishedFrames = 0;
	GridCoord cameraBasisCoordinate {};
	XMFLOAT2 f2WaterOrigin {};
	XMFLOAT2 f2ReducedNoiseOrigin {};
	float fNoiseFrequency = 0.0f;
	RetainedAreaReport shadow {};
	RetainedAreaReport pLighting[shaders::kiLightingLayers] {}; // Per lighting layer
	RetainedAreaReport smoke {};
};

inline PresentationContinuitySnapshot gPresentationContinuity {};

bool IsMoonOut(float fSunAngle);

void RenderFrameGlobal(int64_t iCommandBuffer, std::chrono::duration<float> currentTime);
void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, std::chrono::duration<float> currentTime);

inline bool gbShadowTemporalReset = false; // Set by CreateShadowTextures; re-arms the PopulateShadowParameters first-frame guard so a recreate doesn't blend stale history for one frame

// Set by CreateLightingTextures; re-arms the PopulateLightingParameters first-frame guard so a recreate doesn't blend stale history for one frame.
inline bool gbLightingTemporalReset = false;
// The lighting layer whose deposit, spread, combine, temporal, and history-copy chain runs this frame, or -1 for none.
// PopulateLightingParameters writes it once per frame in Global; collection EndRender and RenderLightingSpreadIndirect
// read it in Main to publish only that layer's deposit counts and chain dispatches.
inline int64_t giLightingRefreshLayer = -1;
void RenderLightingGlobal(int64_t iCommandBuffer);
void RenderLightingMain(int64_t iCommandBuffer);
void RenderWaterMain(int64_t iCommandBuffer, std::chrono::duration<float> currentTime);
void RenderLightingSpreadIndirect(int64_t iCommandBuffer);

inline bool gbWaterFoamReset = true; // Re-armed when the retained foam textures are created.
// Recorded foam commands and published foam uniforms must both follow this predicate.
bool WaterFoamActive();

// GameBase publishes gameplay pause before Global so render publishers share one client-only decision.
inline bool gbGameplayPaused = false;

inline bool gbSmokeClear = true;

void RenderSmokeGlobal(int64_t iCommandBuffer);

inline int64_t giWindTextureIndex = 0; // 0 = write TextureOne, 1 = write TextureTwo

void RenderWindGlobal(int64_t iCommandBuffer);

void PopulateWaterParameters(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fDayPercent, std::chrono::duration<float> elapsedTime);

} // namespace engine

#endif // defined(BT_CLIENT)
