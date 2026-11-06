#pragma once

#include "Data/Model.h"

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/Collections/Ships/ShipTypes.h"

namespace game
{

inline constexpr int64_t kiShipTypeCount = std::to_underlying(ShipType::kCount);
inline constexpr int64_t kiShipControllerCount = std::to_underlying(ShipController::kCount);

struct ShipHitboxType
{
	// The model chunk each controller's hitbox is built from; each ship Render asserts its scene names the same model.
	std::array<common::crc_t, kiShipControllerCount> models {};
	float fGameplayRadius = 0.0f;
	float fPusherRadiusScale = 0.0f;
};

// Indexed by ShipType; models by ShipController, matching each type's Render model table.
inline constexpr std::array<ShipHitboxType, kiShipTypeCount> kShipHitboxTypes
{
	ShipHitboxType {.models = {data::kModelsRavenStarConflictV2scenegltfMODELCrc, data::kModelsSpaceshipscenegltfMODELCrc}, .fGameplayRadius = kfFighterRadius, .fPusherRadiusScale = 1.5f},
	ShipHitboxType {.models = {data::kModelsSpaceGliderGltfscenegltfMODELCrc, data::kModelsSpaceFighterscenegltfMODELCrc}, .fGameplayRadius = kfOmniRadius, .fPusherRadiusScale = 1.5f},
	ShipHitboxType {.models = {data::kModelsBattleshipJesterz86scenegltfMODELCrc, data::kModelsSpaceBattleshipAquamarinescenegltfMODELCrc}, .fGameplayRadius = kfBattleshipRadius, .fPusherRadiusScale = 1.5f},
};

// One controller's shape at gameplay size, facing +X, centered on the ship, with z relative to its height.
struct ShipHitboxShape
{
	engine::CollisionHitbox hitbox {};
	// The row radius: it encloses the hull for a kSphere or kOutline model and the box for a kBox model.
	float fRadius = 0.0f;
	float fPusherRadius = 0.0f;
	// Cached upright water clearance floor for spawn and per-tick height targets.
	float fMinimumOriginZ = 0.0f;
	bool bNavigationObstacle = false;
};

// Indexed by ShipType, then ShipController. Written once by InitializeShipHitboxes before the first frame; read-only
// afterwards.
inline std::array<std::array<ShipHitboxShape, kiShipControllerCount>, kiShipTypeCount> gShipHitboxShapes {};
inline float gfLargestShipPusherRadius = 0.0f;

// Builds each ship controller's hitbox shape at gameplay size from its model's baked outline. Runs once at boot, after the
// eager packs load and before the first frame.
void InitializeShipHitboxes();

// Poses the (type, controller) shape at vecPosition, yawed by the horizontal unit vecDirection, writing the setting and,
// as far as it reaches, the box and outline.
void XM_CALLCONV BuildShipHitbox(ShipType eType, ShipController eController, FXMVECTOR vecPosition, FXMVECTOR vecDirection, engine::CollisionHitbox& rHitbox);

#if defined(BT_CLIENT)
// Returns the center-directed ray's outline contact in the cell-local frame; an inside start uses the exit surface.
XMVECTOR XM_CALLCONV ShipHullContact(ShipType eType, ShipController eController, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecShooterPosition);

// Draws the row's sphere and, as far as its setting reaches, the box and outline prisms; vecPosition and vecDirection are
// the interpolated pose, local to rBasis's cell.
void XM_CALLCONV DebugRenderShipHitbox(const engine::RenderBasis& rBasis, ShipType eType, ShipController eController, FXMVECTOR vecPosition, FXMVECTOR vecDirection);
#endif

} // namespace game
