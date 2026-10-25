#define NOMINMAX
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <winsock2.h>
// Winsock 2 must precede Windows headers that can include Winsock 1.
#include <windows.h>
#include <windowsx.h>
#include <ws2tcpip.h>
// CNG and shell headers require Windows types.
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>

// This shared header consumes the platform and standard types supplied above.
#include "../../Common/ServerMonitorProtocol.h"
struct TextSection
{
	COLORREF uiColor = 0;
	std::string text;
};

enum class ConnectionState { kDisconnected, kConnecting, kAwaitingChallenge, kResponsePending, kConnected };
static ConnectionState seConnectionState = ConnectionState::kDisconnected;
static SOCKET sSocket = INVALID_SOCKET;
static sockaddr_in sServerAddress {};
static std::wstring sHost;
static std::string sSecret;
static std::wstring sStatus;
static std::string sProfileText;
static ULONGLONG suiNextConnect = 0;
static std::vector<uint8_t> sReceiveBytes;
static std::array<uint8_t, common::kiServerMonitorResponseSize> sResponse {};
static int64_t siResponseOffset = 0;
static std::vector<TextSection> sStatisticsSections;
static std::vector<TextSection> sProfileSections;
static std::vector<common::ServerMonitorCell> sCells;

static void Disconnect(HWND hwnd)
{
	if (sSocket != INVALID_SOCKET)
	{
		closesocket(sSocket);
		sSocket = INVALID_SOCKET;
	}
	seConnectionState = ConnectionState::kDisconnected;
	suiNextConnect = GetTickCount64() + 2'000;
	sReceiveBytes.clear();
	sResponse.fill(0);
	siResponseOffset = 0;
	sStatisticsSections.clear();
	sProfileSections.clear();
	sProfileText.clear();
	sCells.clear();
	sStatus = L"Disconnected from " + sHost + L" - retrying";
	InvalidateRect(hwnd, nullptr, FALSE);
}

template<typename T>
static T ReadRecord(const uint8_t*& rpBytes)
{
	T record {};
	std::memcpy(&record, rpBytes, sizeof(record));
	rpBytes += sizeof(record);
	return record;
}

static void ReadSections(const uint8_t*& rpBytes, uint32_t uiCount, std::vector<TextSection>& rSections)
{
	rSections.clear();
	rSections.reserve(uiCount);
	for (int64_t i = 0; i < static_cast<int64_t>(uiCount); ++i)
	{
		common::ServerMonitorTextSection section = ReadRecord<common::ServerMonitorTextSection>(rpBytes);
		rSections.push_back({.uiColor = section.uiColor, .text = std::string(reinterpret_cast<const char*>(rpBytes), section.uiTextSize)});
		rpBytes += section.uiTextSize;
	}
}

static bool ParseFrames(HWND hwnd)
{
	int64_t iOffset = 0;
	while (std::ssize(sReceiveBytes) - iOffset >= static_cast<int64_t>(sizeof(common::ServerMonitorFrameHeader)))
	{
		const uint8_t* pBytes = sReceiveBytes.data() + iOffset;
		common::ServerMonitorFrameHeader frame = ReadRecord<common::ServerMonitorFrameHeader>(pBytes);
		int64_t iFrameSize = static_cast<int64_t>(sizeof(frame)) + frame.uiSize;
		if (std::ssize(sReceiveBytes) - iOffset < iFrameSize)
		{
			break;
		}
		if (frame.eType == common::ServerMonitorMessageType::kChallenge)
		{
			common::ServerMonitorChallenge challenge = ReadRecord<common::ServerMonitorChallenge>(pBytes);
			if (challenge.uiMagic != common::kuiServerMonitorMagic)
			{
				Disconnect(hwnd);
				return false;
			}
			if (challenge.uiVersion != common::kuiServerMonitorProtocolVersion)
			{
				Disconnect(hwnd);
				sStatus = L"Server protocol version " + std::to_wstring(challenge.uiVersion) + L", monitor expects 1";
				return false;
			}
			sResponse.fill(0);
			if (!sSecret.empty())
			{
				common::ComputeServerMonitorResponse(sSecret, challenge.nonce, sResponse);
			}
			siResponseOffset = 0;
			seConnectionState = ConnectionState::kResponsePending;
		}
		else if (frame.eType == common::ServerMonitorMessageType::kSnapshot)
		{
			common::ServerMonitorSnapshotHeader snapshot = ReadRecord<common::ServerMonitorSnapshotHeader>(pBytes);
			ReadSections(pBytes, snapshot.uiStatisticsSectionCount, sStatisticsSections);
			ReadSections(pBytes, snapshot.uiProfileSectionCount, sProfileSections);
			sCells.resize(snapshot.uiCellCount);
			for (common::ServerMonitorCell& rCell : sCells)
			{
				rCell = ReadRecord<common::ServerMonitorCell>(pBytes);
			}
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		iOffset += iFrameSize;
	}
	sReceiveBytes.erase(sReceiveBytes.begin(), sReceiveBytes.begin() + iOffset);
	return true;
}

static void NetworkTimer(HWND hwnd)
{
	if (seConnectionState == ConnectionState::kDisconnected)
	{
		if (GetTickCount64() < suiNextConnect)
		{
			return;
		}
		sSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		u_long uiNonblocking = 1;
		if (sSocket == INVALID_SOCKET)
		{
			Disconnect(hwnd);
			return;
		}
		if (ioctlsocket(sSocket, FIONBIO, &uiNonblocking) != 0)
		{
			Disconnect(hwnd);
			return;
		}
		int iResult = connect(sSocket, reinterpret_cast<const sockaddr*>(&sServerAddress), sizeof(sServerAddress));
		if (iResult != 0 && WSAGetLastError() != WSAEWOULDBLOCK)
		{
			Disconnect(hwnd);
			return;
		}
		seConnectionState = iResult == 0 ? ConnectionState::kAwaitingChallenge : ConnectionState::kConnecting;
	}
	if (seConnectionState == ConnectionState::kConnecting)
	{
		fd_set writable;
		FD_ZERO(&writable);
		FD_SET(sSocket, &writable);
		fd_set failed = writable;
		timeval timeout {};
		int iResult = select(0, nullptr, &writable, &failed, &timeout);
		if (iResult == SOCKET_ERROR || FD_ISSET(sSocket, &failed))
		{
			Disconnect(hwnd);
			return;
		}
		if (!FD_ISSET(sSocket, &writable))
		{
			return;
		}
		seConnectionState = ConnectionState::kAwaitingChallenge;
	}
	std::array<uint8_t, 16 * 1'024> bytes {};
	for (;;)
	{
		int iReceived = recv(sSocket, reinterpret_cast<char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
		if (iReceived == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			break;
		}
		if (iReceived <= 0)
		{
			Disconnect(hwnd);
			return;
		}
		sReceiveBytes.append_range(std::span(bytes.data(), static_cast<std::size_t>(iReceived)));
		if (!ParseFrames(hwnd))
		{
			return;
		}
	}
	if (seConnectionState == ConnectionState::kResponsePending)
	{
		int iSent = send(sSocket, reinterpret_cast<const char*>(sResponse.data() + siResponseOffset), static_cast<int>(std::ssize(sResponse) - siResponseOffset), 0);
		if (iSent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			return;
		}
		if (iSent <= 0)
		{
			Disconnect(hwnd);
			return;
		}
		siResponseOffset += iSent;
		if (siResponseOffset == std::ssize(sResponse))
		{
			seConnectionState = ConnectionState::kConnected;
		}
	}
}

enum class ServerTab : uint8_t { kMap, kProfile };
static ServerTab seActiveTab = ServerTab::kMap;

constexpr int64_t kiTabBarLeft = 250;
constexpr int64_t kiTabHeight = 32;
constexpr int64_t kiTabWidth = 100;
constexpr int64_t kiTabCount = 2;
constexpr std::string_view kpTabNames[] = {"Map", "Profile"};

constexpr int64_t kiCopyButtonWidth = 80;
constexpr int64_t kiCopyButtonHeight = 28;
constexpr int64_t kiCopyButtonMargin = 10;
static RECT sCopyButtonRectangle {};


static HDC sHdcBuffer = nullptr;
static HBITMAP sHbitmapBuffer = nullptr;
static int64_t siBufferWidth = 0;
static int64_t siBufferHeight = 0;
static HFONT sHfont = nullptr;

static HBRUSH sHbrushBackground = nullptr;  // RGB(30, 30, 30) — window / content background
static HBRUSH sHbrushCellClient = nullptr;  // RGB(40, 60, 140) — client-authorized cell
static HBRUSH sHbrushCellActive = nullptr;  // RGB(30, 100, 30) — active cell
static HBRUSH sHbrushGrayFill = nullptr;    // RGB(50, 50, 50) — idle cell / copy button
static HBRUSH sHbrushTabActive = nullptr;   // RGB(60, 60, 60) — active tab
static HBRUSH sHbrushTabInactive = nullptr; // RGB(40, 40, 40) — inactive tab

static HPEN sHpenCellBorderClient = nullptr; // RGB(100, 140, 255) — client cell border
static HPEN sHpenGray = nullptr;             // RGB(80, 80, 80) — normal borders / tab bottom line
static HPEN sHpenRed = nullptr;              // RGB(220, 40, 40) — subscribed-cell interior border
static HPEN sHpenBlue = nullptr;             // RGB(100, 180, 255) — active tab border / copy button border

static void EnsureCachedGraphicsDeviceInterfaceObjects()
{
	if (sHfont != nullptr)
	{
		return;
	}

	sHfont = CreateFont(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

	sHbrushBackground = CreateSolidBrush(RGB(30, 30, 30));
	sHbrushCellClient = CreateSolidBrush(RGB(40, 60, 140));
	sHbrushCellActive = CreateSolidBrush(RGB(30, 100, 30));
	sHbrushGrayFill = CreateSolidBrush(RGB(50, 50, 50));
	sHbrushTabActive = CreateSolidBrush(RGB(60, 60, 60));
	sHbrushTabInactive = CreateSolidBrush(RGB(40, 40, 40));

	sHpenCellBorderClient = CreatePen(PS_SOLID, 1, RGB(100, 140, 255));
	sHpenGray = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
	sHpenRed = CreatePen(PS_SOLID, 1, RGB(220, 40, 40));
	sHpenBlue = CreatePen(PS_SOLID, 1, RGB(100, 180, 255));
}

static void PaintMap(HDC hdcBuffer, std::span<char> buffer, int64_t iMapLeft, int64_t iMapTop, int64_t iMapWidth, int64_t iMapHeight)
{
	auto itActive = std::ranges::find_if(sCells, [](const common::ServerMonitorCell& rCell)
	{
		return (rCell.uiFlags & common::kuiServerMonitorCellActive) != 0;
	});
	if (itActive == sCells.end())
	{
		return;
	}

	int64_t iMinimumX = itActive->iX;
	int64_t iMaximumX = iMinimumX;
	int64_t iMinimumY = itActive->iY;
	int64_t iMaximumY = iMinimumY;

	for (const common::ServerMonitorCell& rCoordinate : sCells)
	{
		if ((rCoordinate.uiFlags & common::kuiServerMonitorCellActive) == 0)
		{
			continue;
		}
		iMinimumX = std::min<int64_t>(iMinimumX, rCoordinate.iX);
		iMaximumX = std::max<int64_t>(iMaximumX, rCoordinate.iX);
		iMinimumY = std::min<int64_t>(iMinimumY, rCoordinate.iY);
		iMaximumY = std::max<int64_t>(iMaximumY, rCoordinate.iY);
	}

	iMinimumX = std::max<int64_t>(iMinimumX - 1, std::numeric_limits<int32_t>::min());
	iMaximumX = std::min<int64_t>(iMaximumX + 1, std::numeric_limits<int32_t>::max());
	iMinimumY = std::max<int64_t>(iMinimumY - 1, std::numeric_limits<int32_t>::min());
	iMaximumY = std::min<int64_t>(iMaximumY + 1, std::numeric_limits<int32_t>::max());

	int64_t iGridWidth = iMaximumX - iMinimumX + 1;
	int64_t iGridHeight = iMaximumY - iMinimumY + 1;

	int64_t iCellWidth = std::min(60i64, iMapWidth / iGridWidth);
	int64_t iCellHeight = std::min(60i64, iMapHeight / iGridHeight);
	int64_t iCellSize = std::min(iCellWidth, iCellHeight);

	int64_t iGridPixelWidth = iGridWidth * iCellSize;
	int64_t iGridPixelHeight = iGridHeight * iCellSize;
	int64_t iOffsetX = (iMapWidth - iGridPixelWidth) / 2;
	int64_t iOffsetY = (iMapHeight - iGridPixelHeight) / 2;

	if (iCellSize < 4)
	{
		return;
	}

	for (int64_t i = iMinimumY; i <= iMaximumY; ++i)
	{
		for (int64_t j = iMinimumX; j <= iMaximumX; ++j)
		{
			int64_t iCellLeft = iMapLeft + iOffsetX + (j - iMinimumX) * iCellSize;
			int64_t iCellTop = iMapTop + iOffsetY + (iMaximumY - i) * iCellSize;
			RECT cellRectangle {.left = static_cast<LONG>(iCellLeft), .top = static_cast<LONG>(iCellTop), .right = static_cast<LONG>(iCellLeft + iCellSize), .bottom = static_cast<LONG>(iCellTop + iCellSize)};

			auto it = std::ranges::find_if(sCells, [i, j](const common::ServerMonitorCell& rCell)
			{
				return rCell.iX == j && rCell.iY == i;
			});
			bool bIsActive = it != sCells.end() && (it->uiFlags & common::kuiServerMonitorCellActive) != 0;
			bool bIsSubscribed = it != sCells.end() && (it->uiFlags & common::kuiServerMonitorCellSubscribed) != 0;
			int64_t iClientsInCell = it != sCells.end() ? it->uiClientCount : 0;
			HBRUSH hbrushCell = sHbrushGrayFill;
			if (iClientsInCell > 0)
			{
				hbrushCell = sHbrushCellClient;
			}
			else if (bIsActive)
			{
				hbrushCell = sHbrushCellActive;
			}
			FillRect(hdcBuffer, &cellRectangle, hbrushCell);

			HPEN hpen = (iClientsInCell > 0) ? sHpenCellBorderClient : sHpenGray;
			HPEN hpenOld = static_cast<HPEN>(SelectObject(hdcBuffer, hpen));
			MoveToEx(hdcBuffer, static_cast<int>(iCellLeft), static_cast<int>(iCellTop), nullptr);
			LineTo(hdcBuffer, static_cast<int>(iCellLeft + iCellSize), static_cast<int>(iCellTop));
			LineTo(hdcBuffer, static_cast<int>(iCellLeft + iCellSize), static_cast<int>(iCellTop + iCellSize));
			LineTo(hdcBuffer, static_cast<int>(iCellLeft), static_cast<int>(iCellTop + iCellSize));
			LineTo(hdcBuffer, static_cast<int>(iCellLeft), static_cast<int>(iCellTop));
			SelectObject(hdcBuffer, hpenOld);

			if (bIsSubscribed && iCellSize >= 20)
			{
				static constexpr int64_t kiInset = 2;
				HPEN hpenOld2 = static_cast<HPEN>(SelectObject(hdcBuffer, sHpenRed));
				HBRUSH hbrushOld = static_cast<HBRUSH>(SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH)));
				Rectangle(hdcBuffer, static_cast<int>(iCellLeft + kiInset), static_cast<int>(iCellTop + kiInset), static_cast<int>(iCellLeft + iCellSize - kiInset + 1), static_cast<int>(iCellTop + iCellSize - kiInset + 1));
				SelectObject(hdcBuffer, hbrushOld);
				SelectObject(hdcBuffer, hpenOld2);
			}

			if (bIsActive && iCellSize >= 24)
			{
				int64_t iEntityCount = it->iEntityCount;

				SetTextColor(hdcBuffer, RGB(220, 220, 220));

				int64_t iLineLength = std::snprintf(buffer.data(), buffer.size(), "(%d,%d)", static_cast<int>(j), static_cast<int>(i));
				TextOutA(hdcBuffer, static_cast<int>(iCellLeft + 2), static_cast<int>(iCellTop + 2), buffer.data(), static_cast<int>(iLineLength));

				iLineLength = std::snprintf(buffer.data(), buffer.size(), "%lld", iEntityCount);
				TextOutA(hdcBuffer, static_cast<int>(iCellLeft + 2), static_cast<int>(iCellTop + 28), buffer.data(), static_cast<int>(iLineLength));

				if (iClientsInCell > 0)
				{
					SetTextColor(hdcBuffer, RGB(150, 200, 255));
					iLineLength = std::snprintf(buffer.data(), buffer.size(), "%lld", iClientsInCell);
					TextOutA(hdcBuffer, static_cast<int>(iCellLeft + iCellSize - 20), static_cast<int>(iCellTop + 2), buffer.data(), static_cast<int>(iLineLength));
				}
			}
		}
	}
}

static void PaintWorkbufferText(HDC hdcBuffer, std::string_view text, int64_t iX, int64_t& riY, int64_t iLineHeight)
{
	int64_t iStart = 0;
	while (iStart < static_cast<int64_t>(text.size()))
	{
	int64_t iEnd = static_cast<int64_t>(text.find('\n', static_cast<std::size_t>(iStart)));
		if (iEnd == -1)
		{
			iEnd = static_cast<int64_t>(text.size());
		}

	std::string_view line = text.substr(static_cast<std::size_t>(iStart), static_cast<std::size_t>(iEnd - iStart));
		if (!line.empty())
		{
			TextOutA(hdcBuffer, static_cast<int>(iX), static_cast<int>(riY), line.data(), static_cast<int>(line.size()));
		}
		riY += iLineHeight;
		iStart = iEnd + 1;
	}
}

static void CopyProfileToClipboard(HWND hwnd)
{
	if (sProfileText.empty())
	{
		return;
	}

	if (OpenClipboard(hwnd) == 0)
	{
		return;
	}

	EmptyClipboard();
	HGLOBAL hglobalMemory = GlobalAlloc(GMEM_MOVEABLE, sProfileText.size() + 1);
	if (hglobalMemory != nullptr)
	{
		char* pcDestination = static_cast<char*>(GlobalLock(hglobalMemory));
		if (pcDestination != nullptr)
		{
			std::copy(sProfileText.begin(), sProfileText.end(), pcDestination);
			pcDestination[sProfileText.size()] = '\0';
			GlobalUnlock(hglobalMemory);
			SetClipboardData(CF_TEXT, hglobalMemory);
		}
		else
		{
			GlobalFree(hglobalMemory);
		}
	}
	CloseClipboard();
}

static void HandleClick(HWND hwnd, int64_t iX, int64_t iY)
{
	if (iY <= kiTabHeight && iX >= kiTabBarLeft)
	{
		int64_t iTabIndex = (iX - kiTabBarLeft) / kiTabWidth;
		if (iTabIndex == 0)
		{
			seActiveTab = ServerTab::kMap;
		}
		else if (iTabIndex == 1)
		{
			seActiveTab = ServerTab::kProfile;
		}
		return;
	}

	if (seActiveTab == ServerTab::kProfile)
	{
		POINT point {.x = static_cast<LONG>(iX), .y = static_cast<LONG>(iY)};
		if (PtInRect(&sCopyButtonRectangle, point) != 0)
		{
			CopyProfileToClipboard(hwnd);
		}
	}
}

static void PaintProfilePanel(HDC hdcBuffer, int64_t iLeft, int64_t iTop, int64_t iHeight)
{
	int64_t iTextX = iLeft + 10;
	int64_t iTextY = iTop;
	sProfileText.clear();
	for (const TextSection& rSection : sProfileSections)
	{
		SetTextColor(hdcBuffer, rSection.uiColor);
		PaintWorkbufferText(hdcBuffer, rSection.text, iTextX, iTextY, 32);
		sProfileText += rSection.text;
		iTextY += 4;
	}
	int64_t iButtonLeft = iLeft + kiCopyButtonMargin;
	int64_t iButtonTop = iTop + iHeight - kiCopyButtonHeight - kiCopyButtonMargin;
	sCopyButtonRectangle = {.left = static_cast<int>(iButtonLeft), .top = static_cast<int>(iButtonTop), .right = static_cast<int>(iButtonLeft + kiCopyButtonWidth), .bottom = static_cast<int>(iButtonTop + kiCopyButtonHeight)};

	FillRect(hdcBuffer, &sCopyButtonRectangle, sHbrushGrayFill);

	HPEN hpenOld = static_cast<HPEN>(SelectObject(hdcBuffer, sHpenBlue));
	MoveToEx(hdcBuffer, sCopyButtonRectangle.left, sCopyButtonRectangle.top, nullptr);
	LineTo(hdcBuffer, sCopyButtonRectangle.right, sCopyButtonRectangle.top);
	LineTo(hdcBuffer, sCopyButtonRectangle.right, sCopyButtonRectangle.bottom);
	LineTo(hdcBuffer, sCopyButtonRectangle.left, sCopyButtonRectangle.bottom);
	LineTo(hdcBuffer, sCopyButtonRectangle.left, sCopyButtonRectangle.top);
	SelectObject(hdcBuffer, hpenOld);

	SetTextColor(hdcBuffer, RGB(200, 200, 200));
	TextOutA(hdcBuffer, sCopyButtonRectangle.left + 14, sCopyButtonRectangle.top + 2, "Copy", 4);
}

static void PaintTabBar(HDC hdcBuffer, int64_t iWidth)
{
	for (int64_t i = 0; std::string_view tabName : kpTabNames)
	{
		int64_t iTabLeft = kiTabBarLeft + i * kiTabWidth;
		RECT tabRectangle {.left = static_cast<LONG>(iTabLeft), .top = 0, .right = static_cast<int>(iTabLeft + kiTabWidth), .bottom = static_cast<int>(kiTabHeight)};

		bool bActive = (i == static_cast<int64_t>(seActiveTab));
		HBRUSH hbrush = bActive ? sHbrushTabActive : sHbrushTabInactive;
		FillRect(hdcBuffer, &tabRectangle, hbrush);

		HPEN hpen = bActive ? sHpenBlue : sHpenGray;
		HPEN hpenOld = static_cast<HPEN>(SelectObject(hdcBuffer, hpen));
		MoveToEx(hdcBuffer, static_cast<int>(iTabLeft), static_cast<int>(kiTabHeight), nullptr);
		LineTo(hdcBuffer, static_cast<int>(iTabLeft), 0);
		LineTo(hdcBuffer, static_cast<int>(iTabLeft + kiTabWidth), 0);
		LineTo(hdcBuffer, static_cast<int>(iTabLeft + kiTabWidth), static_cast<int>(kiTabHeight));
		if (!bActive)
		{
			LineTo(hdcBuffer, static_cast<int>(iTabLeft), static_cast<int>(kiTabHeight));
		}
		SelectObject(hdcBuffer, hpenOld);

		SetTextColor(hdcBuffer, bActive ? RGB(255, 255, 255) : RGB(160, 160, 160));
		TextOutA(hdcBuffer, static_cast<int>(iTabLeft + 12), 4, tabName.data(), static_cast<int>(tabName.size()));
		++i;
	}

	HPEN hpenOld = static_cast<HPEN>(SelectObject(hdcBuffer, sHpenGray));
	int64_t iTabsEnd = kiTabBarLeft + kiTabCount * kiTabWidth;
	MoveToEx(hdcBuffer, static_cast<int>(iTabsEnd), kiTabHeight, nullptr);
	LineTo(hdcBuffer, static_cast<int>(iWidth), kiTabHeight);
	SelectObject(hdcBuffer, hpenOld);
}

static void Paint(HWND hwnd)
{
	PAINTSTRUCT paintStructure {};
	HDC hdc = BeginPaint(hwnd, &paintStructure);

	RECT clientRectangle {};
	GetClientRect(hwnd, &clientRectangle);
	int64_t iWidth = static_cast<int64_t>(clientRectangle.right) - clientRectangle.left;
	int64_t iHeight = static_cast<int64_t>(clientRectangle.bottom) - clientRectangle.top;

	if (iWidth <= 0 || iHeight <= 0)
	{
		EndPaint(hwnd, &paintStructure);
		return;
	}
	EnsureCachedGraphicsDeviceInterfaceObjects();

	if (sHdcBuffer == nullptr)
	{
		sHdcBuffer = CreateCompatibleDC(hdc);
	}
	if (sHbitmapBuffer == nullptr || iWidth != siBufferWidth || iHeight != siBufferHeight)
	{
		HBITMAP hbitmapNew = CreateCompatibleBitmap(hdc, static_cast<int>(iWidth), static_cast<int>(iHeight));
		SelectObject(sHdcBuffer, hbitmapNew); // deselects the previous bitmap (or the DC's default stock bitmap on first paint)
		if (sHbitmapBuffer != nullptr)
		{
			DeleteObject(sHbitmapBuffer);
		}
		sHbitmapBuffer = hbitmapNew;
		siBufferWidth = iWidth;
		siBufferHeight = iHeight;
	}
	HDC hdcBuffer = sHdcBuffer;

	FillRect(hdcBuffer, &clientRectangle, sHbrushBackground);

	SelectObject(hdcBuffer, sHfont);
	SetBkMode(hdcBuffer, TRANSPARENT);

	int64_t iTextY = 10;
	if (sStatisticsSections.empty())
	{
		SetTextColor(hdcBuffer, RGB(200, 200, 200));
		TextOutW(hdcBuffer, 10, static_cast<int>(iTextY), sStatus.data(), static_cast<int>(sStatus.size()));
	}
	for (const TextSection& rSection : sStatisticsSections)
	{
		SetTextColor(hdcBuffer, rSection.uiColor);
		PaintWorkbufferText(hdcBuffer, rSection.text, 10, iTextY, 36);
	}
	char pcLine[256] {};
	PaintTabBar(hdcBuffer, iWidth);

	int64_t iContentLeft = kiTabBarLeft;
	int64_t iContentTop = kiTabHeight + 5;
	int64_t iContentWidth = iWidth - iContentLeft - 10;
	int64_t iContentHeight = iHeight - iContentTop - 10;

	if (seActiveTab == ServerTab::kMap)
	{
		PaintMap(hdcBuffer, pcLine, iContentLeft, iContentTop, iContentWidth, iContentHeight);
	}
	else if (seActiveTab == ServerTab::kProfile)
	{
		PaintProfilePanel(hdcBuffer, iContentLeft, iContentTop, iContentHeight);
	}

	BitBlt(hdc, 0, 0, static_cast<int>(iWidth), static_cast<int>(iHeight), hdcBuffer, 0, 0, SRCCOPY);

	EndPaint(hwnd, &paintStructure);
}

static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uiMessage, WPARAM wParam, LPARAM lParam)
{
	switch (uiMessage)
	{
		case WM_TIMER:
			NetworkTimer(hwnd);
			return 0;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			Paint(hwnd);
			return 0;
		case WM_LBUTTONDOWN:
			HandleClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			InvalidateRect(hwnd, nullptr, FALSE);
			return 0;
		case WM_DESTROY:
			KillTimer(hwnd, 1);
			Disconnect(hwnd);
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProcW(hwnd, uiMessage, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, PWSTR, int iShowCommand)
{
	SetProcessDPIAware();
	int iArgumentCount = 0;
	PWSTR* ppArguments = CommandLineToArgvW(GetCommandLineW(), &iArgumentCount);
	if (ppArguments == nullptr)
	{
		return 1;
	}
	sHost = iArgumentCount > 1 ? ppArguments[1] : L"127.0.0.1";
	LocalFree(ppArguments);
	PWSTR pRoamingDirectory = nullptr;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pRoamingDirectory)))
	{
		return 1;
	}
	std::ifstream secretFile(std::filesystem::path(pRoamingDirectory) / L"ServerMonitor" / L"ServerMonitorSecret.txt", std::ios::binary);
	CoTaskMemFree(pRoamingDirectory);
	std::string secretText(std::istreambuf_iterator<char>(secretFile), {});
	sSecret = common::ServerMonitorSecretKey(secretText);
	WSADATA data {};
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
	{
		return 1;
	}
	ADDRINFOW hints {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	PADDRINFOW pAddresses = nullptr;
	std::wstring port = std::to_wstring(common::kiServerMonitorPort);
	if (GetAddrInfoW(sHost.c_str(), port.c_str(), &hints, &pAddresses) != 0)
	{
		MessageBoxW(nullptr, L"Unable to resolve server host", L"Server Monitor", MB_OK | MB_ICONERROR);
		WSACleanup();
		return 1;
	}
	std::memcpy(&sServerAddress, pAddresses->ai_addr, sizeof(sServerAddress));
	FreeAddrInfoW(pAddresses);
	WNDCLASSW windowClass {};
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = hinstance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = L"BrokenEngineServerMonitor";
	if (RegisterClassW(&windowClass) == 0)
	{
		WSACleanup();
		return 1;
	}
	std::wstring title = L"Server Monitor - " + sHost;
	HWND hwnd = CreateWindowW(windowClass.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hinstance, nullptr);
	if (hwnd == nullptr)
	{
		WSACleanup();
		return 1;
	}
	Disconnect(hwnd);
	suiNextConnect = 0;
	if (SetTimer(hwnd, 1, 50, nullptr) == 0)
	{
		DestroyWindow(hwnd);
		WSACleanup();
		return 1;
	}
	ShowWindow(hwnd, iShowCommand);
	MSG message {};
	int iResult = 0;
	while ((iResult = GetMessageW(&message, nullptr, 0, 0)) > 0)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	WSACleanup();
	return iResult == -1 ? 1 : static_cast<int>(message.wParam);
}
