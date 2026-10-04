#pragma once

#if defined(BT_CLIENT)

#include "Frame/IslandTerrain.h"

namespace engine
{

// Phase 5 LRU grace: a template's GPU resources stay resident this many render frames after its
// last placement reference drops. ~5s @60Hz, ~2.5s @120Hz. Chosen to cover transient absences
// in moving-camera traversal without holding GPU memory indefinitely.
inline constexpr uint64_t kuiGraceRenderFrames = 300;

class IslandTerrainResidency
{
public:

	IslandTerrainResidency();
	~IslandTerrainResidency();

	// Client-only: assign or retrieve the bindless texture-array slot for an island template.
	// First call for a CRC binds its 4 textures into mRenderTargetTextures at the next free slot.
	// Newly-minted templates start in slot-0 fallback (slot points at the neutral placeholder
	// textures) until RestorationSweep detects per-texture adoption and patches the slot to real.
	int64_t AcquireTextureSlot(common::crc_t uiIslandCrc);

	// Phase 5 LRU eviction sweeps. Both must run inside RenderGlobal post-fence-wait
	// (descriptor-patch safety window), bracketing TextureManager::ProcessPendingTextures.
	void EvictionSweep();
	void RestorationSweep();

	// Cheap pre-scans: true iff EvictionSweep / RestorationSweep would actually free or patch GPU
	// resources this frame. RenderGlobal uses these to drain all in-flight fences only on churn
	// frames (the guard conditions mirror the in-sweep skip logic exactly).
	bool AnyEvictionPending() const;
	bool AnyRestorationPending() const;

	// Clear per-template GPU residency before Graphics tears down the VMA allocator. The arena
	// itself belongs to Islands, which is destroyed first.
	void ReleaseGpuResources();

	// Reset per-template slot-assignment state so the next AcquireTextureSlot call runs the
	// first-mint path (re-registering all five channel bindings; elevation remains at the new
	// TextureManager placeholder until the four chunk-backed channels are ready). Required after a
	// kSurface-tier Graphics teardown destroys TextureManager — the
	// stale iTextureSlot >= 0 would otherwise short-circuit AcquireTextureSlot's hot path and
	// strand every island on the new placeholder forever. Called from TextureManager ctor.
	void ResetTextureSlots();

private:
	// AcquireTextureSlot's first-mint path picks or reuses a slot, creates elevation from the in-memory
	// heightmap, wires five bindless array pointers, registers per-pipeline bindings, and returns the slot.
	int64_t FirstMintTextureSlot(common::crc_t uiIslandCrc, IslandTemplate& rTemplate, const common::crc_t (&rTextureCrcs)[4], std::string_view name);

	enum class MeshEvictionReason : uint8_t
	{
		kGrace,
		kArenaExhaustion,
	};

	// Evict one template's complete texture+mesh residency. Arena exhaustion bypasses only the
	// grace period; it still requires a resident, unreferenced template.
	bool EvictTemplate(common::crc_t uiIslandCrc, IslandTemplate& rTemplate, MeshEvictionReason eReason = MeshEvictionReason::kGrace);
	bool IsEvictionPending(const IslandTemplate& rTemplate) const;
	bool IsRestorationPending(common::crc_t uiIslandCrc, const IslandTemplate& rTemplate) const;
	bool HasArenaEvictionCandidate(common::crc_t uiExcludedCrc) const;

	// Starts at 1: slot 0 is reserved as a permanent neutral placeholder anchor, never adopted
	// by any real island. See TextureManager::mIslandPlaceholder* members.
	int64_t miNextTextureSlot = 1;

	// Slots reclaimed by EvictionSweep (full teardown sets the template's iTextureSlot = -1). Popped
	// first by AcquireTextureSlot before bumping miNextTextureSlot, so a long browse / churn session
	// reuses indices instead of marching toward the kiMaxIslands ceiling. Main-thread-only (RenderGlobal
	// eviction and UpdateActiveIslands mint run on the same thread). Cleared in ResetTextureSlots.
	std::vector<int64_t> mFreeTextureSlots;
};

inline IslandTerrainResidency* gpIslandTerrainResidency = nullptr;

} // namespace engine

#endif // BT_CLIENT
