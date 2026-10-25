#pragma once

// PCH-less consumers supply <windows.h>, <bcrypt.h>, <array>, <cstdint>, <span> and <string_view>.

namespace common
{

inline constexpr int kiServerMonitorPort = 27'017;
inline constexpr uint32_t kuiServerMonitorMagic = 0x4E4D5442;
inline constexpr uint32_t kuiServerMonitorProtocolVersion = 1;
inline constexpr int kiServerMonitorNonceSize = 32;
inline constexpr int kiServerMonitorResponseSize = 32;
inline constexpr uint32_t kuiServerMonitorMaximumFrameSize = 4 * 1'024 * 1'024;
inline constexpr uint32_t kuiServerMonitorCellActive = 1;
inline constexpr uint32_t kuiServerMonitorCellSubscribed = 2;

enum class ServerMonitorMessageType : uint32_t { kChallenge = 1, kSnapshot = 2 };

struct ServerMonitorFrameHeader
{
	uint32_t uiSize = 0;
	ServerMonitorMessageType eType = static_cast<ServerMonitorMessageType>(0);
};
static_assert(sizeof(ServerMonitorFrameHeader) == 8);

struct ServerMonitorChallenge
{
	uint32_t uiMagic = 0;
	uint32_t uiVersion = 0;
	std::array<uint8_t, 32> nonce {};
};
static_assert(sizeof(ServerMonitorChallenge) == 40);

struct ServerMonitorSnapshotHeader
{
	uint32_t uiStatisticsSectionCount = 0;
	uint32_t uiProfileSectionCount = 0;
	uint32_t uiCellCount = 0;
};
static_assert(sizeof(ServerMonitorSnapshotHeader) == 12);

struct ServerMonitorTextSection
{
	uint32_t uiColor = 0;
	uint32_t uiTextSize = 0;
};
static_assert(sizeof(ServerMonitorTextSection) == 8);

struct ServerMonitorCell
{
	int32_t iX = 0;
	int32_t iY = 0;
	uint32_t uiClientCount = 0;
	uint32_t uiFlags = 0;
	int64_t iEntityCount = 0;
};
static_assert(sizeof(ServerMonitorCell) == 24);

inline std::string_view ServerMonitorSecretKey(std::string_view fileText)
{
	std::size_t uiFirst = fileText.find_first_not_of(" \t\r\n");
	if (uiFirst == std::string_view::npos)
	{
		return {};
	}
	return fileText.substr(uiFirst, fileText.find_last_not_of(" \t\r\n") - uiFirst + 1);
}

inline bool ComputeServerMonitorResponse(std::string_view secretKey, std::span<const uint8_t, 32> nonce, std::array<uint8_t, 32>& rResponse)
{
	return BCRYPT_SUCCESS(BCryptHash(BCRYPT_HMAC_SHA256_ALG_HANDLE, reinterpret_cast<PUCHAR>(const_cast<char*>(secretKey.data())), static_cast<ULONG>(secretKey.size()), const_cast<PUCHAR>(nonce.data()), static_cast<ULONG>(nonce.size()), rResponse.data(), static_cast<ULONG>(rResponse.size())));
}

} // namespace common
