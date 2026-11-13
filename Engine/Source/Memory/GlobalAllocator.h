#pragma once

inline constinit std::atomic<int64_t> giAllocationsThisFrame = 0;

inline constinit std::atomic<bool> gbAllocationTrackingReady = false;
