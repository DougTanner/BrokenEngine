#pragma once

namespace engine
{

struct CellStaticData;

} // namespace engine

namespace game
{

struct Frame;
struct FrameInput;

// Applied after every ship Update; kDestroyShip begins Fighter/Omni explosions and marks Battleships for removal.
void ApplyUpdateStatusChanges(Frame& rFrame, const FrameInput& rFrameInput, const engine::CellStaticData& rStaticData);

} // namespace game
