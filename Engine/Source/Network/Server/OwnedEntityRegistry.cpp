#include "Pch.h"

#include "Network/Server/OwnedEntityRegistry.h"

#if defined(BT_SERVER)

#include "Network/Server/Server.h"

namespace engine
{

void OwnedEntityRegistry::Add(int64_t iClientId, global_id_t globalId, GridCoord coord)
{
	mOwned.try_emplace(iClientId).first->second.push_back({.globalId = globalId, .coord = coord});

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoords.push_back(coord);
	}
}

void OwnedEntityRegistry::RemoveAt(int64_t iClientId, int64_t iIndex)
{
	auto it = mOwned.find(iClientId);
	if (it == mOwned.end())
	{
		return;
	}

	it->second.erase(it->second.begin() + iIndex);

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoords.erase(pClient->authorizedCoords.begin() + iIndex);
	}
}

void OwnedEntityRegistry::UpdateCoord(int64_t iClientId, global_id_t globalId, GridCoord newCoord)
{
	auto it = mOwned.find(iClientId);
	if (it == mOwned.end())
	{
		return;
	}

	for (int64_t i = 0; i < std::ssize(it->second); ++i)
	{
		OwnedEntity& rOwnedEntity = it->second.at(i);
		if (rOwnedEntity.globalId != globalId)
		{
			continue;
		}

		rOwnedEntity.coord = newCoord;
		ClientConnection* pClient = gpServer->FindClient(iClientId);
		if (pClient != nullptr)
		{
			pClient->authorizedCoords.at(i) = newCoord;
		}
		return;
	}
}

std::span<const OwnedEntity> OwnedEntityRegistry::Owned(int64_t iClientId) const
{
	auto it = mOwned.find(iClientId);
	if (it == mOwned.end())
	{
		return {};
	}
	return it->second;
}

void OwnedEntityRegistry::Remove(int64_t iClientId)
{
	mOwned.erase(iClientId);
}

void OwnedEntityRegistry::Clear(int64_t iClientId)
{
	mOwned.try_emplace(iClientId).first->second.clear();

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoords.clear();
	}
}

void OwnedEntityRegistry::Reserve(int64_t iClientId, int64_t iCount)
{
	mOwned.try_emplace(iClientId).first->second.reserve(static_cast<size_t>(iCount));

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoords.reserve(static_cast<size_t>(iCount));
	}
}

} // namespace engine

#endif // BT_SERVER
