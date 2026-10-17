#include "Pch.h"

#include "Network/Server/OwnedEntityRegistry.h"

#if defined(BT_SERVER)

#include "Network/Server/Server.h"

namespace engine
{

void OwnedEntityRegistry::Add(int64_t iClientId, GlobalId globalId, GridCoord coord)
{
	mOwned.try_emplace(iClientId).first->second.push_back({.globalId = globalId, .coord = coord});

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoordinates.push_back(coord);
	}
}

void OwnedEntityRegistry::Clear(int64_t iClientId)
{
	mOwned.try_emplace(iClientId).first->second.clear();

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoordinates.clear();
	}
}

void OwnedEntityRegistry::Reserve(int64_t iClientId, int64_t iCount)
{
	mOwned.try_emplace(iClientId).first->second.reserve(static_cast<size_t>(iCount));

	ClientConnection* pClient = gpServer->FindClient(iClientId);
	if (pClient != nullptr)
	{
		pClient->authorizedCoordinates.reserve(static_cast<size_t>(iCount));
	}
}

} // namespace engine

#endif // BT_SERVER
