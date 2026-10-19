#pragma once

#include "Frame/FrameBase.h"

#include "Frame/FrameInput.h"
// Direct include, not the Engine.h aggregation: the game PCH pulls this header in ahead of Engine.h.
#include "Frame/FrameRegistry.h"
#include "Frame/GridCoord.h"
#include "Frame/NavBuild.h"
#include "Frame/TimeStep.h"

namespace game
{

struct Frame;
struct BlastersInterpolate;
struct BlastersPostRender;
struct MissilesInterpolate;
struct MissilesPostRender;
struct FightersInterpolate;
struct FightersPostRender;
struct OmnisInterpolate;
struct OmnisPostRender;
struct BattleshipsInterpolate;
struct BattleshipsPostRender;

struct FrameInterpolate : public engine::FrameInterpolateBase
{
	// Called on Game creation
	static void Register();

#if defined(BT_CLIENT)
	// Called during Graphics creation
	static void GraphicsResources();
#endif

	static void AllocateAndCopy(FrameInterpolate& __restrict rCurrent, const FrameInterpolate& __restrict rPrevious);
	static void Update(FrameInterpolate& __restrict rCurrent, const Frame& __restrict rPreviousFrame, float fDeltaTime);

#if defined(BT_CLIENT)
	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
	static void DebugRender(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
#endif

	FrameInterpolate();
	~FrameInterpolate();
	FrameInterpolate(FrameInterpolate&&) noexcept;
	FrameInterpolate& operator=(FrameInterpolate&&) noexcept;

	float fSpawnTimer = 0.0f;

	std::unique_ptr<BlastersInterpolate> pBlasters;
	std::unique_ptr<MissilesInterpolate> pMissiles;
	std::unique_ptr<FightersInterpolate> pFighters;
	std::unique_ptr<OmnisInterpolate> pOmnis;
	std::unique_ptr<BattleshipsInterpolate> pBattleships;

	// The game frame-wide values Write, Read, and ServerRead serialize after the base values, in stream order.
	static auto Values()
	{
		return engine::FrameColumnList<FRAME_COLUMN(FrameInterpolate, fSpawnTimer)> {};
	}

	static common::crc_t Crcs(const FrameInterpolate& rCurrent);
	bool LogDifferences(const FrameInterpolate& rOther) const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
	void ServerRead(std::istream& rStream);
};

struct TransferRequest
{
	StatusChange change {};
	int8_t iDeltaX = 0;
	int8_t iDeltaY = 0;
};

struct FramePostRender : public engine::FramePostRenderBase
{
	FramePostRender();
	~FramePostRender();
	FramePostRender(FramePostRender&&) noexcept;
	FramePostRender& operator=(FramePostRender&&) noexcept;

	static void AllocateAndCopy(FramePostRender& __restrict rCurrent, const FramePostRender& __restrict rPrevious);
	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData);
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void AreaDamage(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void Transfer(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Spawn(Frame& __restrict rFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData);

	engine::AlignmentIdentifier randomAlignment {};
	engine::AlignmentIdentifier fleetAlignment {};

	std::unique_ptr<BlastersPostRender> pBlasters;
	std::unique_ptr<MissilesPostRender> pMissiles;
	std::unique_ptr<FightersPostRender> pFighters;
	std::unique_ptr<OmnisPostRender> pOmnis;
	std::unique_ptr<BattleshipsPostRender> pBattleships;

	// The game frame-wide values Write, Read, and ServerRead serialize after the base values, in stream order.
	static auto Values()
	{
		return engine::FrameColumnList<FRAME_COLUMN(FramePostRender, randomAlignment), FRAME_COLUMN(FramePostRender, fleetAlignment)> {};
	}

	static common::crc_t Crcs(const FramePostRender& rCurrent);
	bool LogDifferences(const FramePostRender& rOther) const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
	void ServerRead(std::istream& rStream);
};

// One registry query window: the context plus the single workbuffer allocation its spans point into. The owner
// keeps the whole object alive for the window; the allocation pops when it dies, so no context span outlives it.
struct RegistryWindow
{
	common::ScopedWorkbufferAllocation<std::byte*> buffer;
	engine::RegistryQueryContext context {};
};

struct Frame
{
	Frame();
	~Frame();
	Frame(Frame&&) noexcept;
	Frame& operator=(Frame&&) noexcept;

	static const int64_t kiVersion;

	// Post-tick main-thread identity views over the ship rows, one per ShipType in ShipType order. Held by value by
	// its caller for as long as it is used, and never across a tick, a transfer, or a reallocation.
	[[nodiscard]] static std::array<engine::RegistryOwnershipLayer, std::to_underlying(ShipType::kCount)> OwnershipLayers(const Frame& rFrame);
	// Rows owned by a fleet (pController == kFleet) across the ship types.
	[[nodiscard]] static int64_t OwnedEntityCount(const Frame& rFrame);

	FrameInterpolate interpolate;
	FramePostRender postRender;

	common::crc_t Crc() const;
	bool LogDifferences(const Frame& rOther) const;
	void ServerRead(std::istream& rStream);
};

std::ostream& operator<<(std::ostream& rStream, const Frame& rCurrent);
std::istream& operator>>(std::istream& rStream, Frame& rCurrent);

} // namespace game
