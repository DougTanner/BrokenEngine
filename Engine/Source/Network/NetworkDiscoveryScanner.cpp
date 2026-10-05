#include "Pch.h"

#if defined(BT_CLIENT)

#include "Network/NetworkDiscoveryScanner.h"

namespace engine
{

NetworkDiscoveryScanner::NetworkDiscoveryScanner()
{
	muiSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (muiSocket == INVALID_SOCKET)
	{
		LOG(kNetwork, kError, "NetworkDiscoveryScanner socket creation failed: {}", WSAGetLastError());
	}

	if (gLaunchOptions.flags & LaunchOptionFlags::kLoopbackOnly)
	{
		sockaddr_in address {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (bind(muiSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
		{
			LOG(kNetwork, kError, "NetworkDiscoveryScanner bind failed: {}", WSAGetLastError());
		}
	}
	else
	{
		BOOL bBroadcast = TRUE;
		if (setsockopt(muiSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bBroadcast), sizeof(bBroadcast)) == SOCKET_ERROR)
		{
			LOG(kNetwork, kError, "NetworkDiscoveryScanner setsockopt SO_BROADCAST failed: {}", WSAGetLastError());
		}
	}

	u_long uiNonBlocking = 1;
	if (ioctlsocket(muiSocket, FIONBIO, &uiNonBlocking) == SOCKET_ERROR)
	{
		LOG(kNetwork, kError, "NetworkDiscoveryScanner ioctlsocket failed: {}", WSAGetLastError());
	}
}

NetworkDiscoveryScanner::~NetworkDiscoveryScanner()
{
	closesocket(muiSocket);
}

void NetworkDiscoveryScanner::StartScan()
{
	uint32_t uiMagic = static_cast<uint32_t>(kiDiscoveryMagic);

	sockaddr_in localAddress {};
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(static_cast<uint16_t>(kiDiscoveryPort));
	localAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sendto(muiSocket, reinterpret_cast<const char*>(&uiMagic), sizeof(uiMagic), 0, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress));

	if (!(gLaunchOptions.flags & LaunchOptionFlags::kLoopbackOnly))
	{
		sockaddr_in broadcastAddress {};
		broadcastAddress.sin_family = AF_INET;
		broadcastAddress.sin_port = htons(static_cast<uint16_t>(kiDiscoveryPort));
		broadcastAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		sendto(muiSocket, reinterpret_cast<const char*>(&uiMagic), sizeof(uiMagic), 0, reinterpret_cast<sockaddr*>(&broadcastAddress), sizeof(broadcastAddress));
	}

	mTimer.Reset();
	mFlags.Set(DiscoveryScannerFlags::kStarted);
}

void NetworkDiscoveryScanner::Poll()
{
	ASSERT(common::gpMultithreading->IsMainThread());

	sockaddr_in senderAddress {};
	int iSenderLength = sizeof(senderAddress);
	uint32_t uiMagic = 0;

	int64_t iReceived = recvfrom(muiSocket, reinterpret_cast<char*>(&uiMagic), sizeof(uiMagic), 0, reinterpret_cast<sockaddr*>(&senderAddress), &iSenderLength);
	if (iReceived == sizeof(uiMagic) && uiMagic == kiDiscoveryMagic)
	{
		uint8_t* pBytes = reinterpret_cast<uint8_t*>(&senderAddress.sin_addr);
		std::snprintf(mpcFoundAddress, sizeof(mpcFoundAddress), "%u.%u.%u.%u", pBytes[0], pBytes[1], pBytes[2], pBytes[3]);
		mFlags.Set(DiscoveryScannerFlags::kFound);
	}
}

bool NetworkDiscoveryScanner::IsScanning()
{
	if (!(mFlags & DiscoveryScannerFlags::kStarted) || (mFlags & DiscoveryScannerFlags::kFound))
	{
		return false;
	}

	return mTimer.GetDeltaNs() < kDiscoveryScanDuration;
}

} // namespace engine

#endif // BT_CLIENT
