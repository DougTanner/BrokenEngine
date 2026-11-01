#pragma once

namespace engine
{

inline constexpr int64_t kiAreaDamageSourcePreallocate = 16;

struct CollisionHitbox;

struct AreaDamageSource
{
	XMVECTOR vecPosition {};
	float fRadius = 0.0f;
	float fDamage = 0.0f;
	uint16_t uiCategory = 0;
};

class AreaDamage
{
public:

	static void Add(const AreaDamageSource& rSource);

	// Returns total damage with linear falloff applied, filtered by category mask. Each source's distance is to the
	// hitbox's deepest shape: its box prism for kBox, its hull prism for kOutline, and vecPosition for kSphere.
	// Outputs the closest damage source position via rvecClosestSource
	static float Get(FXMVECTOR vecPosition, const CollisionHitbox& rHitbox, uint16_t uiCategoryMask, XMVECTOR& rvecClosestSource);

	static inline thread_local int64_t siAreaDamageSourceCount = 0;

private:

	// StableVector construction only records the reserved count, so this reserves no address space and makes
	// no OS call until the first Resize; growth then commits more of that reservation without moving.
	static inline thread_local common::StableVector<AreaDamageSource> sAreaDamageSources = common::StableVector<AreaDamageSource>(64 * kiAreaDamageSourcePreallocate);
};

} // namespace engine
