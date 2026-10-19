#pragma once

namespace engine
{
struct CellStaticData;
} // namespace engine

namespace game
{

struct Frame;
struct FrameInput;

// kUpdateShip, kUpdateFleet, and kDestroyShip, applied once after every ship Update; kDestroyShip marks the row for the
// Destroy-phase compaction fold.
void ApplyUpdateStatusChanges(Frame& rFrame, const FrameInput& rFrameInput, const engine::CellStaticData& rStaticData);

} // namespace game
