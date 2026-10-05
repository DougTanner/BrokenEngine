#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/NetworkDiscoveryResponder.h"

namespace engine
{

NetworkDiscoveryResponder::NetworkDiscoveryResponder()
{
	muiSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (muiSocket == INVALID_SOCKET)
	{
		LOG(kNetwork, kError, "NetworkDiscoveryResponder socket creation failed: {}", WSAGetLastError());
	}

	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_port = htons(static_cast<uint16_t>(kiDiscoveryPort));
	address.sin_addr.s_addr = (gLaunchOptions.flags & LaunchOptionFlags::kLoopbackOnly) ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
	if (bind(muiSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
	{
		LOG(kNetwork, kError, "NetworkDiscoveryResponder bind failed: {}", WSAGetLastError());
	}

	u_long uiNonBlocking = 1;
	if (ioctlsocket(muiSocket, FIONBIO, &uiNonBlocking) == SOCKET_ERROR)
	{
		LOG(kNetwork, kError, "NetworkDiscoveryResponder ioctlsocket failed: {}", WSAGetLastError());
	}
}

NetworkDiscoveryResponder::~NetworkDiscoveryResponder()
{
	closesocket(muiSocket);
}

void NetworkDiscoveryResponder::Poll()
{
	ASSERT(common::gpMultithreading->IsMainThread());

	sockaddr_in senderAddress {};
	int iSenderLength = sizeof(senderAddress);
	uint32_t uiMagic = 0;

	int64_t iReceived = recvfrom(muiSocket, reinterpret_cast<char*>(&uiMagic), sizeof(uiMagic), 0, reinterpret_cast<sockaddr*>(&senderAddress), &iSenderLength);
	if (iReceived == sizeof(uiMagic) && uiMagic == kiDiscoveryMagic)
	{
		uint32_t uiResponse = static_cast<uint32_t>(kiDiscoveryMagic);
		sendto(muiSocket, reinterpret_cast<const char*>(&uiResponse), sizeof(uiResponse), 0, reinterpret_cast<sockaddr*>(&senderAddress), iSenderLength);
	}
}

} // namespace engine

#endif // BT_SERVER
