#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class NetworkDiscoveryScanner
{
public:

	NetworkDiscoveryScanner();
	~NetworkDiscoveryScanner();

	void StartScan();
	void Poll();

	bool IsScanning();
	enum class DiscoveryScannerFlags : uint8_t
	{
		kStarted = 1 << 0,
		kFound   = 1 << 1,
	};

private:

	SOCKET muiSocket = INVALID_SOCKET;
	common::Timer mTimer;

public:

	common::Flags<DiscoveryScannerFlags> mFlags;
	char mpcFoundAddress[16] {};
};

} // namespace engine

#endif // BT_CLIENT
