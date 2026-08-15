#pragma once

#if defined(BT_SERVER)

namespace engine
{

struct OwnedEntity
{
	global_id_t globalId {};
	GridCoord coord {};
};

class OwnedEntityRegistry
{
public:

	void Add(int64_t iClientId, global_id_t globalId, GridCoord coord);
	void RemoveAt(int64_t iClientId, int64_t iIndex);
	void UpdateCoord(int64_t iClientId, global_id_t globalId, GridCoord newCoord);
	std::span<const OwnedEntity> Owned(int64_t iClientId) const;
	void Remove(int64_t iClientId);
	void Clear(int64_t iClientId);
	void Reserve(int64_t iClientId, int64_t iCount);

private:

	std::unordered_map<int64_t, std::vector<OwnedEntity>> mOwned;
};

} // namespace engine

#endif // BT_SERVER
