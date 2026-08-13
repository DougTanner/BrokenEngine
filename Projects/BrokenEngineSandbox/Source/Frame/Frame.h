#pragma once

#include "Frame/FrameBase.h"
#include "Frame/GridCoord.h"
#include "Frame/NavBuild.h"
#include "Frame/TimeStep.h"

#include "Input/Input.h"

namespace game
{

// Forward declarations (definitions in individual collection headers)
struct PlayersInterpolate;
struct PlayersPostRender;
struct BlastersInterpolate;
struct BlastersPostRender;
struct MissilesInterpolate;
struct MissilesPostRender;
struct SpaceshipsInterpolate;
struct SpaceshipsPostRender;
struct TargetsInterpolate;
struct TargetsPostRender;

using player_t = engine::id_t<PlayersInterpolate>;
using target_t = engine::id_t<TargetsInterpolate>;

enum class GameFlags : uint64_t
{
	kMainMenu    = 0x00000001,
	kGame        = 0x00000002,
};
using GameFlags_t = common::Flags<GameFlags>;

struct FrameInterpolate : public engine::FrameInterpolateBase
{
	// Called on Game creation
	static void Register();

#if defined(BT_CLIENT)
	// Called during Graphics creation
	static void GraphicsResources();
#endif

	// Interpolate phases
	static void AllocateAndCopy(FrameInterpolate& __restrict rCurrent, const FrameInterpolate& __restrict rPrevious);
	static void Update(FrameInterpolate& __restrict rCurrent, const Frame& __restrict rPreviousFrame, float fDeltaTime);

#if defined(BT_CLIENT)
	// Render
	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoords);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
	static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coord);
#endif

	FrameInterpolate();
	~FrameInterpolate();
	FrameInterpolate(FrameInterpolate&&) noexcept;
	FrameInterpolate& operator=(FrameInterpolate&&) noexcept;

	GameFlags_t gameFlags;
	float fSpawnTimer = 0.0f;

	std::unique_ptr<PlayersInterpolate> pPlayers;

	std::unique_ptr<BlastersInterpolate> pBlasters;
	std::unique_ptr<MissilesInterpolate> pMissiles;
	std::unique_ptr<SpaceshipsInterpolate> pSpaceships;
	std::unique_ptr<TargetsInterpolate> pTargets;

	static common::crc_t Crcs(const FrameInterpolate& rCurrent);
	bool LogDifferences(const FrameInterpolate& rOther) const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
	void ServerRead(std::istream& rStream);
};

struct TransferRequest
{
	StatusChangeType eType {};
	TransferData data {};
	int64_t iEntityId = 0;
	int8_t iDeltaX = 0;
	int8_t iDeltaY = 0;
	// Tick the request was pushed on. Every live entry should match the current tick.
	int64_t iPushedTick = 0;
};

struct FramePostRender : public engine::FramePostRenderBase
{
	FramePostRender();
	~FramePostRender();
	FramePostRender(FramePostRender&&) noexcept;
	FramePostRender& operator=(FramePostRender&&) noexcept;

	// Post render phases
	static void AllocateAndCopy(FramePostRender& __restrict rCurrent, const FramePostRender& __restrict rPrevious);
	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const FrameInput& __restrict rFrameInput, const engine::FrameStaticData& rStaticData);
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::FrameStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::FrameStaticData& rStaticData);
	static void AreaDamage(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::FrameStaticData& rStaticData);
	static void Transfer(Frame& __restrict rFrame, const engine::FrameStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::FrameStaticData& rStaticData);
	static void Spawn(Frame& __restrict rFrame, const FrameInput& __restrict rFrameInput, const engine::FrameStaticData& rStaticData);

	engine::alignment_t enemyAlignment {};
	engine::alignment_t playerAlignment {};

	std::unique_ptr<PlayersPostRender> pPlayers;

	std::unique_ptr<BlastersPostRender> pBlasters;
	std::unique_ptr<MissilesPostRender> pMissiles;
	std::unique_ptr<SpaceshipsPostRender> pSpaceships;
	std::unique_ptr<TargetsPostRender> pTargets;

	// Transient transfer output buffer (not serialized, not in CRC/equality)
	std::vector<TransferRequest> transferRequests;

	static common::crc_t Crcs(const FramePostRender& rCurrent);
	bool LogDifferences(const FramePostRender& rOther) const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
	void ServerRead(std::istream& rStream);
};

struct Frame
{
	Frame();
	~Frame();
	Frame(Frame&&) noexcept;
	Frame& operator=(Frame&&) noexcept;

	static const int64_t kiVersion;

	[[nodiscard]] static target_t XM_CALLCONV GetMissileTarget(Frame& __restrict rFrame, FXMVECTOR vecPosition, FXMVECTOR vecDirection, engine::alignment_t alignment);

	FrameInterpolate interpolate;
	FramePostRender postRender;

	common::crc_t Crcs() const;
	common::crc_t Crc() const;
	bool LogDifferences(const Frame& rOther) const;
	void ServerRead(std::istream& rStream);
};

std::ostream& operator<<(std::ostream& rStream, const Frame& rCurrent);
std::istream& operator>>(std::istream& rStream, Frame& rCurrent);

} // namespace game
