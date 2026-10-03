#pragma once

#if defined(BT_SERVER)

namespace engine
{

struct OwnedEntity
{
	GlobalId globalId {};
	GridCoord coord {};
};

class OwnedEntityRegistry
{
public:

	void Add(int64_t iClientId, GlobalId globalId, GridCoord coord);
	void RemoveAt(int64_t iClientId, int64_t iIndex);
	void UpdateCoord(int64_t iClientId, GlobalId globalId, GridCoord newCoord);
	void Clear(int64_t iClientId);
	void Reserve(int64_t iClientId, int64_t iCount);

	std::unordered_map<int64_t, std::vector<OwnedEntity>> mOwned;
};

} // namespace engine

#endif // BT_SERVER
