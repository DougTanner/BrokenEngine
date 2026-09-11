#pragma once

// Each pass contains its per-input failures behind one aggregate diagnostic and returns false instead of
// throwing, so a bad cubemap input never skips the other pass or any later export work.
bool GenerateIrradianceCubemaps();
bool GeneratePreFilteredCubemaps();
