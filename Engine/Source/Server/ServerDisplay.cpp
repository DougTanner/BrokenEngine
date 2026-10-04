#include "Pch.h"

#if defined(BT_SERVER)

#include "Server/ServerDisplay.h"

#include "Network/Server/Server.h"

#include "Profile/ProfileManager.h"

// Game headers stay outside Engine.h to keep game types out of the shared PCH.
#include "Frame/ServerCellStats.h"
#include "Game.h"

namespace engine
{

enum class ServerTab : uint8_t { kMap, kProfile };
static ServerTab seActiveTab = ServerTab::kMap;

constexpr int64_t kiTabBarLeft = 250;
constexpr int64_t kiTabHeight = 32;
constexpr int64_t kiTabWidth = 100;
constexpr int64_t kiTabCount = 2;
constexpr std::string_view kTabNames[] = {"Map", "Profile"};

constexpr int64_t kiCopyButtonWidth = 80;
constexpr int64_t kiCopyButtonHeight = 28;
constexpr int64_t kiCopyButtonMargin = 10;
static RECT sCopyButtonRectangle {};
static std::string sProfileText;

// Dirty-check state for the throttled repaint: coarse hash of the last content the window drew.
static uint64_t suiLastContentHash = 0;
static bool sbHasLastContentHash = false;

// Entity totals for the left panel are summed once per tick from the game cell-stat seam and stay file-static for the
// process's one window. Profiling-disabled builds skip aggregation, so these remain zero and the entity rows read zero.
static int64_t siTotalPlayers = 0;
static int64_t siTotalSpaceships = 0;
static int64_t siTotalBlasters = 0;
static int64_t siTotalMissiles = 0;

// Persistent GDI objects (one window per process): the back-buffer DC/bitmap and font are created on first
// paint and the bitmap is recreated only when the client size changes; the palette brushes/pens are a fixed
// RGB set created once instead of per-cell per-paint. Nothing is explicitly destroyed — process teardown
// reclaims GDI objects.
static HDC shDeviceContextBuffer = nullptr;
static HBITMAP shBitmapBuffer = nullptr;
static int siBufferWidth = 0;
static int siBufferHeight = 0;
static HFONT shFont = nullptr;

static HBRUSH shBrushBackground = nullptr;  // RGB(30, 30, 30) — window / content background
static HBRUSH shBrushCellClient = nullptr;  // RGB(40, 60, 140) — client-authorized cell
static HBRUSH shBrushCellActive = nullptr;  // RGB(30, 100, 30) — active cell
static HBRUSH shBrushGrayFill = nullptr;    // RGB(50, 50, 50) — idle cell / copy button
static HBRUSH shBrushTabActive = nullptr;   // RGB(60, 60, 60) — active tab
static HBRUSH shBrushTabInactive = nullptr; // RGB(40, 40, 40) — inactive tab

static HPEN shPenCellBorderClient = nullptr; // RGB(100, 140, 255) — client cell border
static HPEN shPenGray = nullptr;             // RGB(80, 80, 80) — normal borders / tab bottom line
static HPEN shPenRed = nullptr;              // RGB(220, 40, 40) — subscribed-cell interior border
static HPEN shPenBlue = nullptr;             // RGB(100, 180, 255) — active tab border / copy button border

static void EnsureCachedGraphicsDeviceInterfaceObjects()
{
	if (shFont != nullptr)
	{
		return;
	}

	shFont = CreateFont(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

	shBrushBackground = CreateSolidBrush(RGB(30, 30, 30));
	shBrushCellClient = CreateSolidBrush(RGB(40, 60, 140));
	shBrushCellActive = CreateSolidBrush(RGB(30, 100, 30));
	shBrushGrayFill = CreateSolidBrush(RGB(50, 50, 50));
	shBrushTabActive = CreateSolidBrush(RGB(60, 60, 60));
	shBrushTabInactive = CreateSolidBrush(RGB(40, 40, 40));

	shPenCellBorderClient = CreatePen(PS_SOLID, 1, RGB(100, 140, 255));
	shPenGray = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
	shPenRed = CreatePen(PS_SOLID, 1, RGB(220, 40, 40));
	shPenBlue = CreatePen(PS_SOLID, 1, RGB(100, 180, 255));
}

void ServerUpdateDisplayStatistics()
{
	if constexpr (!kbProfiling)
	{
		return;
	}

	siTotalPlayers = 0;
	siTotalSpaceships = 0;
	siTotalBlasters = 0;
	siTotalMissiles = 0;
	int64_t iTotalExplosions = 0;

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCoordinateFrames.at(rCoordinate).pCurrent));
		siTotalPlayers += cellStatistics.iPlayers;
		siTotalSpaceships += cellStatistics.iSpaceships;
		siTotalBlasters += cellStatistics.iBlasters;
		siTotalMissiles += cellStatistics.iMissiles;
		iTotalExplosions += cellStatistics.iExplosions;
	}

	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount = iTotalExplosions;
	}

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	mi_stats_merge();
	mi_stats_t mimallocStatistics = {};
	mimallocStatistics.size = sizeof(mi_stats_t);
	mimallocStatistics.version = MI_STAT_VERSION;
	mi_stats_get(&mimallocStatistics);

	gpProfileManager->miMimallocCommittedMebibytes = mimallocStatistics.committed.current / (1'024 * 1'024);
	gpProfileManager->miMimallocPeakCommittedMebibytes = mimallocStatistics.committed.peak / (1'024 * 1'024);
	gpProfileManager->miMimallocHeapUsedMebibytes = mimallocStatistics.page_committed.current / (1'024 * 1'024);
	gpProfileManager->miMimallocPeakHeapUsedMebibytes = mimallocStatistics.page_committed.peak / (1'024 * 1'024);
#endif

	gpProfileManager->UpdateProfileText();
}

bool ServerDisplayContentChanged()
{
	// Coarse FNV-1a fold over the substantive stats/map inputs the window draws — active coords and their
	// entity totals, the client list with its authorized/subscribed coords, the active tab, and memory
	// stats. The free-running tick/time and CPU-timer values are deliberately excluded so an idle server
	// (stable entity/map state) stops repainting instead of blitting an all-but-identical frame at 4 Hz.
	// POD-only fold — no heap allocation, so it stays inside the caller's ScopedSuppressAllocationTracking.
	uint64_t uiHash = 14'695'981'039'346'656'037ULL;
	auto Mix = [&uiHash](int64_t iValue)
	{
		uiHash ^= static_cast<uint64_t>(iValue);
		uiHash *= 1'099'511'628'211ULL;
	};

	Mix(static_cast<int64_t>(seActiveTab));

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		Mix(rCoordinate.iX);
		Mix(rCoordinate.iY);

		// Mix each count separately — summing would alias conversions (e.g. spaceship death -1 spaceship
		// +1 explosion leaves the sum unchanged) and skip a repaint whose displayed numbers did change.
		game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCoordinateFrames.at(rCoordinate).pCurrent));
		Mix(cellStatistics.iPlayers);
		Mix(cellStatistics.iSpaceships);
		Mix(cellStatistics.iBlasters);
		Mix(cellStatistics.iMissiles);
		Mix(cellStatistics.iExplosions);
	}

	const std::vector<ClientConnection>& rClients = gpServer->mClients;
	Mix(std::ssize(rClients));
	for (const ClientConnection& rClient : rClients)
	{
		for (const GridCoord& rOwnedCoordinate : rClient.authorizedCoordinates)
		{
			Mix(rOwnedCoordinate.iX);
			Mix(rOwnedCoordinate.iY);
		}

		for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
		{
			if (rClient.slots.at(i).subscription.flags & SubscriptionFlags::kActive)
			{
				Mix(rClient.slots.at(i).subscription.coordinate.iX);
				Mix(rClient.slots.at(i).subscription.coordinate.iY);
			}
		}
	}

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	Mix(gpProfileManager->miMimallocCommittedMebibytes);
	Mix(gpProfileManager->miMimallocPeakCommittedMebibytes);
	Mix(gpProfileManager->miMimallocHeapUsedMebibytes);
	Mix(gpProfileManager->miMimallocPeakHeapUsedMebibytes);
#endif

	if (sbHasLastContentHash && uiHash == suiLastContentHash)
	{
		return false;
	}

	suiLastContentHash = uiHash;
	sbHasLastContentHash = true;
	return true;
}

static void PaintGridMap(HDC hDeviceContextBuffer, std::span<char> buffer, int iMapLeft, int iMapTop, int iMapWidth, int iMapHeight, const std::vector<ClientConnection>& rClients)
{
	if (game::gpGame->mActiveCoordinates.empty())
	{
		return;
	}

	// Compute grid bounds with 1-cell padding
	int32_t iMinimumX = game::gpGame->mActiveCoordinates.at(0).iX;
	int32_t iMaximumX = iMinimumX;
	int32_t iMinimumY = game::gpGame->mActiveCoordinates.at(0).iY;
	int32_t iMaximumY = iMinimumY;

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		iMinimumX = std::min(iMinimumX, rCoordinate.iX);
		iMaximumX = std::max(iMaximumX, rCoordinate.iX);
		iMinimumY = std::min(iMinimumY, rCoordinate.iY);
		iMaximumY = std::max(iMaximumY, rCoordinate.iY);
	}

	iMinimumX -= 1;
	iMaximumX += 1;
	iMinimumY -= 1;
	iMaximumY += 1;

	int32_t iGridWidth = iMaximumX - iMinimumX + 1;
	int32_t iGridHeight = iMaximumY - iMinimumY + 1;

	int iCellWidth = std::min(60, iMapWidth / iGridWidth);
	int iCellHeight = std::min(60, iMapHeight / iGridHeight);
	int iCellSize = std::min(iCellWidth, iCellHeight);

	int iGridPixelWidth = iGridWidth * iCellSize;
	int iGridPixelHeight = iGridHeight * iCellSize;
	int iOffsetX = (iMapWidth - iGridPixelWidth) / 2;
	int iOffsetY = (iMapHeight - iGridPixelHeight) / 2;

	if (iCellSize < 4)
	{
		return;
	}

	for (int32_t i = iMinimumY; i <= iMaximumY; ++i)
	{
		for (int32_t j = iMinimumX; j <= iMaximumX; ++j)
		{
			int iCellLeft = iMapLeft + iOffsetX + (j - iMinimumX) * iCellSize;
			int iCellTop = iMapTop + iOffsetY + (iMaximumY - i) * iCellSize;
			RECT cellRectangle {.left = iCellLeft, .top = iCellTop, .right = iCellLeft + iCellSize, .bottom = iCellTop + iCellSize};

			GridCoord coordinate {.iX = j, .iY = i};
			bool bIsActive = false;
			bool bIsSubscribed = false;
			int64_t iClientsInCell = 0;

			for (const GridCoord& rActiveCoordinate : game::gpGame->mActiveCoordinates)
			{
				if (rActiveCoordinate == coordinate)
				{
					bIsActive = true;
					break;
				}
			}

			for (const ClientConnection& rClient : rClients)
			{
				for (const GridCoord& rOwnedCoordinate : rClient.authorizedCoordinates)
				{
					if (rOwnedCoordinate == coordinate)
					{
						++iClientsInCell;
					}
				}

				if (!bIsSubscribed && rClient.FindSlotForCoordinate(coordinate) >= 0)
				{
					bIsSubscribed = true;
				}
			}

			HBRUSH hBrushCell = shBrushGrayFill;
			if (iClientsInCell > 0)
			{
				hBrushCell = shBrushCellClient;
			}
			else if (bIsActive)
			{
				hBrushCell = shBrushCellActive;
			}
			FillRect(hDeviceContextBuffer, &cellRectangle, hBrushCell);

			HPEN hPen = (iClientsInCell > 0) ? shPenCellBorderClient : shPenGray;
			HPEN hOldPen = static_cast<HPEN>(SelectObject(hDeviceContextBuffer, hPen));
			MoveToEx(hDeviceContextBuffer, iCellLeft, iCellTop, nullptr);
			LineTo(hDeviceContextBuffer, iCellLeft + iCellSize, iCellTop);
			LineTo(hDeviceContextBuffer, iCellLeft + iCellSize, iCellTop + iCellSize);
			LineTo(hDeviceContextBuffer, iCellLeft, iCellTop + iCellSize);
			LineTo(hDeviceContextBuffer, iCellLeft, iCellTop);
			SelectObject(hDeviceContextBuffer, hOldPen);

			// Red interior border for subscribed cells
			if (bIsSubscribed && iCellSize >= 20)
			{
				static constexpr int kiInset = 2;
				HPEN hOldPen2 = static_cast<HPEN>(SelectObject(hDeviceContextBuffer, shPenRed));
				HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hDeviceContextBuffer, GetStockObject(NULL_BRUSH)));
				Rectangle(hDeviceContextBuffer, iCellLeft + kiInset, iCellTop + kiInset, iCellLeft + iCellSize - kiInset + 1, iCellTop + iCellSize - kiInset + 1);
				SelectObject(hDeviceContextBuffer, hOldBrush);
				SelectObject(hDeviceContextBuffer, hOldPen2);
			}

			// Cell labels for active cells
			if (bIsActive && iCellSize >= 24)
			{
				game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCoordinateFrames.at(coordinate).pCurrent));
				int64_t iEntityCount = cellStatistics.iPlayers + cellStatistics.iSpaceships + cellStatistics.iBlasters + cellStatistics.iMissiles + cellStatistics.iExplosions;

				SetTextColor(hDeviceContextBuffer, RGB(220, 220, 220));

				int iLineLength = std::min(std::snprintf(buffer.data(), buffer.size(), "(%d,%d)", j, i), static_cast<int>(buffer.size()) - 1);
				TextOutA(hDeviceContextBuffer, iCellLeft + 2, iCellTop + 2, buffer.data(), iLineLength);

				iLineLength = std::min(std::snprintf(buffer.data(), buffer.size(), "%lld", iEntityCount), static_cast<int>(buffer.size()) - 1);
				TextOutA(hDeviceContextBuffer, iCellLeft + 2, iCellTop + 28, buffer.data(), iLineLength);

				if (iClientsInCell > 0)
				{
					SetTextColor(hDeviceContextBuffer, RGB(150, 200, 255));
					iLineLength = std::min(std::snprintf(buffer.data(), buffer.size(), "%lld", iClientsInCell), static_cast<int>(buffer.size()) - 1);
					TextOutA(hDeviceContextBuffer, iCellLeft + iCellSize - 20, iCellTop + 2, buffer.data(), iLineLength);
				}
			}
		}
	}
}

static void PaintWorkbufferText(HDC hDeviceContextBuffer, std::string_view text, int64_t iX, int64_t& riY, int64_t iLineHeight)
{
	size_t uiStart = 0;
	while (uiStart < text.size())
	{
		size_t uiEnd = text.find('\n', uiStart);
		if (uiEnd == std::string_view::npos)
		{
			uiEnd = text.size();
		}

		std::string_view line = text.substr(uiStart, uiEnd - uiStart);
		if (!line.empty())
		{
			TextOutA(hDeviceContextBuffer, static_cast<int>(iX), static_cast<int>(riY), line.data(), static_cast<int>(line.size()));
		}
		riY += iLineHeight;
		uiStart = uiEnd + 1;
	}
}

static void CopyProfileToClipboard(HWND hWindow)
{
	if (sProfileText.empty())
	{
		return;
	}

	if (OpenClipboard(hWindow) == 0)
	{
		return;
	}

	EmptyClipboard();
	HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, sProfileText.size() + 1);
	if (hMemory != nullptr)
	{
		char* pcDestination = static_cast<char*>(GlobalLock(hMemory));
		if (pcDestination != nullptr)
		{
			std::memcpy(pcDestination, sProfileText.data(), sProfileText.size());
			pcDestination[sProfileText.size()] = '\0';
			GlobalUnlock(hMemory);
			SetClipboardData(CF_TEXT, hMemory);
		}
		else
		{
			GlobalFree(hMemory);
		}
	}
	CloseClipboard();
}

void HandleServerClick(HWND hWindow, int64_t iX, int64_t iY)
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
			CopyProfileToClipboard(hWindow);
		}
	}
}

static void PaintProfilePanel(HDC hDeviceContextBuffer, int iLeft, int iTop, [[maybe_unused]] int iWidth, int iHeight)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	int64_t iTextX = iLeft + 10;
	int64_t iTextY = iTop;
	int64_t iLineHeight = 32;

	// Heap: std::string operations for clipboard cache
	ScopedSuppressAllocationTracking suppress;
	sProfileText.clear();

	char pcLine[256] {};
	int iLineLength = std::min(std::snprintf(pcLine, sizeof(pcLine), "FPS: %lld  Potential: %lld", gpProfileManager->mFullUpdatesInTheLastSecond.Get(), gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average() > 0 ? 1'000'000 / gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average() : static_cast<int64_t>(0)), static_cast<int>(sizeof(pcLine)) - 1);
	SetTextColor(hDeviceContextBuffer, RGB(100, 180, 255));
	TextOutA(hDeviceContextBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, iLineLength);
	sProfileText += pcLine;
	sProfileText += "\n";
	iTextY += iLineHeight + 4;

	// Re-evaluate row visibility on the shared 2s cadence (timers and counters together this paint).
	bool bReevaluate = gpProfileManager->TickVisibilityCadence();

	{
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		FormatCpuTimersText(rWorkbuffer, bReevaluate);
		std::string_view timers = rWorkbuffer.View();
		SetTextColor(hDeviceContextBuffer, RGB(220, 220, 220));
		PaintWorkbufferText(hDeviceContextBuffer, timers, iTextX, iTextY, iLineHeight);
		sProfileText += timers;
	}

	iTextY += 4;

	{
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		FormatCpuCountersText(rWorkbuffer, bReevaluate);
		std::string_view counters = rWorkbuffer.View();
		SetTextColor(hDeviceContextBuffer, RGB(150, 220, 150));
		PaintWorkbufferText(hDeviceContextBuffer, counters, iTextX, iTextY, iLineHeight);
		sProfileText += counters;
	}

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	iTextY += 4;
	SetTextColor(hDeviceContextBuffer, RGB(220, 180, 100));

	auto MemoryLine = [&](const char* pcFormat, int64_t iValue)
	{
		int iMemoryLineLength = std::min(std::snprintf(pcLine, sizeof(pcLine), pcFormat, iValue), static_cast<int>(sizeof(pcLine)) - 1);
		TextOutA(hDeviceContextBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, iMemoryLineLength);
		sProfileText += pcLine;
		sProfileText += "\n";
		iTextY += iLineHeight;
	};

	MemoryLine("Committed: %lld MiB", gpProfileManager->miMimallocCommittedMebibytes);
	MemoryLine("Peak cmtd: %lld MiB", gpProfileManager->miMimallocPeakCommittedMebibytes);
	MemoryLine("Heap used: %lld MiB", gpProfileManager->miMimallocHeapUsedMebibytes);
	MemoryLine("Peak heap: %lld MiB", gpProfileManager->miMimallocPeakHeapUsedMebibytes);
#endif

	int64_t iButtonLeft = iLeft + kiCopyButtonMargin;
	int64_t iButtonTop = iTop + iHeight - kiCopyButtonHeight - kiCopyButtonMargin;
	sCopyButtonRectangle = {.left = static_cast<int>(iButtonLeft), .top = static_cast<int>(iButtonTop), .right = static_cast<int>(iButtonLeft + kiCopyButtonWidth), .bottom = static_cast<int>(iButtonTop + kiCopyButtonHeight)};

	FillRect(hDeviceContextBuffer, &sCopyButtonRectangle, shBrushGrayFill);

	HPEN hOldPen = static_cast<HPEN>(SelectObject(hDeviceContextBuffer, shPenBlue));
	MoveToEx(hDeviceContextBuffer, sCopyButtonRectangle.left, sCopyButtonRectangle.top, nullptr);
	LineTo(hDeviceContextBuffer, sCopyButtonRectangle.right, sCopyButtonRectangle.top);
	LineTo(hDeviceContextBuffer, sCopyButtonRectangle.right, sCopyButtonRectangle.bottom);
	LineTo(hDeviceContextBuffer, sCopyButtonRectangle.left, sCopyButtonRectangle.bottom);
	LineTo(hDeviceContextBuffer, sCopyButtonRectangle.left, sCopyButtonRectangle.top);
	SelectObject(hDeviceContextBuffer, hOldPen);

	SetTextColor(hDeviceContextBuffer, RGB(200, 200, 200));
	TextOutA(hDeviceContextBuffer, sCopyButtonRectangle.left + 14, sCopyButtonRectangle.top + 2, "Copy", 4);
}

static void PaintTabBar(HDC hDeviceContextBuffer, int iWidth)
{
	for (int64_t i = 0; std::string_view tabName : kTabNames)
	{
		int iTabLeft = static_cast<int>(kiTabBarLeft + i * kiTabWidth);
		RECT tabRectangle {.left = iTabLeft, .top = 0, .right = static_cast<int>(iTabLeft + kiTabWidth), .bottom = static_cast<int>(kiTabHeight)};

		bool bActive = (i == static_cast<int64_t>(seActiveTab));
		HBRUSH hBrush = bActive ? shBrushTabActive : shBrushTabInactive;
		FillRect(hDeviceContextBuffer, &tabRectangle, hBrush);

		HPEN hPen = bActive ? shPenBlue : shPenGray;
		HPEN hOldPen = static_cast<HPEN>(SelectObject(hDeviceContextBuffer, hPen));
		MoveToEx(hDeviceContextBuffer, iTabLeft, static_cast<int>(kiTabHeight), nullptr);
		LineTo(hDeviceContextBuffer, iTabLeft, 0);
		LineTo(hDeviceContextBuffer, static_cast<int>(iTabLeft + kiTabWidth), 0);
		LineTo(hDeviceContextBuffer, static_cast<int>(iTabLeft + kiTabWidth), static_cast<int>(kiTabHeight));
		if (!bActive)
		{
			LineTo(hDeviceContextBuffer, iTabLeft, static_cast<int>(kiTabHeight));
		}
		SelectObject(hDeviceContextBuffer, hOldPen);

		SetTextColor(hDeviceContextBuffer, bActive ? RGB(255, 255, 255) : RGB(160, 160, 160));
		TextOutA(hDeviceContextBuffer, iTabLeft + 12, 4, tabName.data(), static_cast<int>(tabName.size()));
		++i;
	}

	// Bottom line across non-tab area
	HPEN hOldPen = static_cast<HPEN>(SelectObject(hDeviceContextBuffer, shPenGray));
	int iTabsEnd = static_cast<int>(kiTabBarLeft + kiTabCount * kiTabWidth);
	MoveToEx(hDeviceContextBuffer, iTabsEnd, kiTabHeight, nullptr);
	LineTo(hDeviceContextBuffer, iWidth, kiTabHeight);
	SelectObject(hDeviceContextBuffer, hOldPen);
}

void PaintServerDisplay(HWND hWindow)
{
	PAINTSTRUCT paintStructure {};
	HDC hDeviceContext = BeginPaint(hWindow, &paintStructure);

	RECT clientRectangle {};
	GetClientRect(hWindow, &clientRectangle);
	int iWidth = clientRectangle.right - clientRectangle.left;
	int iHeight = clientRectangle.bottom - clientRectangle.top;

	EnsureCachedGraphicsDeviceInterfaceObjects();

	if (shDeviceContextBuffer == nullptr)
	{
		shDeviceContextBuffer = CreateCompatibleDC(hDeviceContext);
	}
	if (shBitmapBuffer == nullptr || iWidth != siBufferWidth || iHeight != siBufferHeight)
	{
		HBITMAP hBitmapNew = CreateCompatibleBitmap(hDeviceContext, iWidth, iHeight);
		SelectObject(shDeviceContextBuffer, hBitmapNew); // deselects the previous bitmap (or the DC's default stock bitmap on first paint)
		if (shBitmapBuffer != nullptr)
		{
			DeleteObject(shBitmapBuffer);
		}
		shBitmapBuffer = hBitmapNew;
		siBufferWidth = iWidth;
		siBufferHeight = iHeight;
	}
	HDC hDeviceContextBuffer = shDeviceContextBuffer;

	FillRect(hDeviceContextBuffer, &clientRectangle, shBrushBackground);

	SelectObject(hDeviceContextBuffer, shFont);
	SetBkMode(hDeviceContextBuffer, TRANSPARENT);

	game::ServerCellStats originStatistics = game::GetServerCellStatistics((*game::gpGame->mCoordinateFrames.at(kOriginCoordinate).pCurrent));
	int64_t iTick = originStatistics.iTick;
	float fCurrentTime = originStatistics.durationCurrentTime.count();

	const std::vector<ClientConnection>& rClients = gpServer->mClients;
	int64_t iClientCount = std::ssize(rClients);

	int64_t iActiveCells = std::ssize(game::gpGame->mActiveCoordinates);

	// Left half: text stats
	int iTextX = 10;
	int iTextY = 10;
	int iLineHeight = 36;
	char pcLine[256] {};

	auto TextLine = [&](const char* pcFormat, auto... arguments)
	{
		int iTextLength = std::min(std::snprintf(pcLine, sizeof(pcLine), pcFormat, arguments...), static_cast<int>(sizeof(pcLine)) - 1);
		TextOutA(hDeviceContextBuffer, iTextX, iTextY, pcLine, iTextLength);
		iTextY += iLineHeight;
	};

	SetTextColor(hDeviceContextBuffer, RGB(100, 180, 255));
	TextLine("FPS: %lld", gpProfileManager->mFullUpdatesInTheLastSecond.Get());
	int64_t iFrameUpdateMicroseconds = gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average();
	int64_t iPotentialFramesPerSecond = iFrameUpdateMicroseconds > 0 ? 1'000'000 / iFrameUpdateMicroseconds : 0;
	TextLine("Potential: %lld", iPotentialFramesPerSecond);
	iTextY += iLineHeight;

	SetTextColor(hDeviceContextBuffer, RGB(200, 200, 200));

	TextLine("Tick: %lld", iTick);
	TextLine("Time: %.2f s", static_cast<double>(fCurrentTime));
	TextLine("Active cells: %lld", iActiveCells);
	int iTextLength = std::min(std::snprintf(pcLine, sizeof(pcLine), "Clients: %lld", iClientCount), static_cast<int>(sizeof(pcLine)) - 1);
	TextOutA(hDeviceContextBuffer, iTextX, iTextY, pcLine, iTextLength);
	iTextY += iLineHeight;

	SetTextColor(hDeviceContextBuffer, RGB(150, 220, 150));

	TextLine("Players: %lld", siTotalPlayers);
	TextLine("Spaceships: %lld", siTotalSpaceships);
	TextLine("Blasters: %lld", siTotalBlasters);
	TextLine("Missiles: %lld", siTotalMissiles);
	iTextLength = std::min(std::snprintf(pcLine, sizeof(pcLine), "Explosions: %lld", gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount), static_cast<int>(sizeof(pcLine)) - 1);
	TextOutA(hDeviceContextBuffer, iTextX, iTextY, pcLine, iTextLength);

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	iTextY += iLineHeight * 2;
	SetTextColor(hDeviceContextBuffer, RGB(220, 180, 100));

	TextLine("Committed: %lld MiB", gpProfileManager->miMimallocCommittedMebibytes);
	TextLine("Peak cmtd: %lld MiB", gpProfileManager->miMimallocPeakCommittedMebibytes);
	TextLine("Heap used: %lld MiB", gpProfileManager->miMimallocHeapUsedMebibytes);
	iTextLength = std::min(std::snprintf(pcLine, sizeof(pcLine), "Peak heap: %lld MiB", gpProfileManager->miMimallocPeakHeapUsedMebibytes), static_cast<int>(sizeof(pcLine)) - 1);
	TextOutA(hDeviceContextBuffer, iTextX, iTextY, pcLine, iTextLength);
#endif

	PaintTabBar(hDeviceContextBuffer, iWidth);

	// Right side content area (below tab bar)
	int iContentLeft = static_cast<int>(kiTabBarLeft);
	int iContentTop = static_cast<int>(kiTabHeight) + 5;
	int iContentWidth = iWidth - iContentLeft - 10;
	int iContentHeight = iHeight - iContentTop - 10;

	if (seActiveTab == ServerTab::kMap)
	{
		PaintGridMap(hDeviceContextBuffer, pcLine, iContentLeft, iContentTop, iContentWidth, iContentHeight, rClients);
	}
	else if (seActiveTab == ServerTab::kProfile)
	{
		PaintProfilePanel(hDeviceContextBuffer, iContentLeft, iContentTop, iContentWidth, iContentHeight);
	}

	BitBlt(hDeviceContext, 0, 0, iWidth, iHeight, hDeviceContextBuffer, 0, 0, SRCCOPY);

	EndPaint(hWindow, &paintStructure);
}

} // namespace engine

#endif // BT_SERVER
