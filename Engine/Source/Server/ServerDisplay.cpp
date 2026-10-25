#include "Pch.h"

#if defined(BT_SERVER)

#include "Server/ServerDisplay.h"

#include "Network/Server/Server.h"

#include "Profile/ProfileRows.h"

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
constexpr std::string_view kpTabNames[] = {"Map", "Profile"};

constexpr int64_t kiCopyButtonWidth = 80;
constexpr int64_t kiCopyButtonHeight = 28;
constexpr int64_t kiCopyButtonMargin = 10;
static RECT sCopyButtonRectangle {};
static std::string sProfileText;

// Dirty-check state for the throttled repaint: coarse hash of the last content the window drew.
static uint64_t suiLastContentHash = 0;

// Entity totals for the left panel are summed once per tick from the game cell-stat seam and stay file-static for the
// process's one window. Profiling-disabled builds skip aggregation, so these remain zero and the entity rows read zero.
static std::array<int64_t, std::size(game::kServerEntityCountNames)> sTotalEntityCounts {};

// Persistent GDI objects (one window per process): the back-buffer DC/bitmap and font are created on first
// paint and the bitmap is recreated only when the client size changes; the palette brushes/pens are a fixed
// RGB set created once instead of per-cell per-paint. Nothing is explicitly destroyed — process teardown
// reclaims GDI objects.
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

void ServerUpdateDisplayStatistics()
{
	if constexpr (!kbProfiling)
	{
		return;
	}

	sTotalEntityCounts.fill(0);
	int64_t iTotalExplosions = 0;

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCells.at(rCoordinate).pCurrent));
		for (int64_t i = 0; i < std::ssize(sTotalEntityCounts); ++i)
		{
			sTotalEntityCounts[i] += cellStatistics.entityCounts[i];
		}
		iTotalExplosions += cellStatistics.iExplosions;
	}

	gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount = iTotalExplosions;

#if !defined(ENABLE_CRT_DEBUG_HEAP)
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
	uint64_t uiHash = 14'695'981'039'346'656'037ui64;
	auto Mix = [&uiHash](int64_t iValue)
	{
		uiHash ^= static_cast<uint64_t>(iValue);
		uiHash *= 1'099'511'628'211ui64;
	};

	Mix(static_cast<int64_t>(seActiveTab));

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		Mix(rCoordinate.iQ);
		Mix(rCoordinate.iR);

		// Mix each count separately — summing would alias conversions (e.g. a death that removes one entity
		// and adds one explosion leaves the sum unchanged) and skip a repaint whose displayed numbers did change.
		game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCells.at(rCoordinate).pCurrent));
		for (int64_t iEntityCount : cellStatistics.entityCounts)
		{
			Mix(iEntityCount);
		}
		Mix(cellStatistics.iExplosions);
	}

	const std::vector<ClientConnection>& rClients = gpServer->mClients;
	Mix(std::ssize(rClients));
	for (const ClientConnection& rClient : rClients)
	{
		for (const GridCoord& rOwnedCoordinate : rClient.authorizedCoordinates)
		{
			Mix(rOwnedCoordinate.iQ);
			Mix(rOwnedCoordinate.iR);
		}

		for (const ClientConnection::SlotState& rSlot : rClient.slots)
		{
			if (rSlot.subscription.flags & SubscriptionFlags::kActive)
			{
				Mix(rSlot.subscription.coordinate.iQ);
				Mix(rSlot.subscription.coordinate.iR);
			}
		}
	}

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	Mix(gpProfileManager->miMimallocCommittedMebibytes);
	Mix(gpProfileManager->miMimallocPeakCommittedMebibytes);
	Mix(gpProfileManager->miMimallocHeapUsedMebibytes);
	Mix(gpProfileManager->miMimallocPeakHeapUsedMebibytes);
#endif

	if (uiHash == suiLastContentHash)
	{
		return false;
	}

	suiLastContentHash = uiHash;
	return true;
}

static void PaintGridMap(HDC hdcBuffer, std::span<char> buffer, int64_t iMapLeft, int64_t iMapTop, int64_t iMapWidth, int64_t iMapHeight, const std::vector<ClientConnection>& rClients)
{
	if (game::gpGame->mActiveCoordinates.empty())
	{
		return;
	}

	// Compute grid bounds with 1-cell padding
	int64_t iMinimumX = game::gpGame->mActiveCoordinates.at(0).iQ;
	int64_t iMaximumX = iMinimumX;
	int64_t iMinimumY = game::gpGame->mActiveCoordinates.at(0).iR;
	int64_t iMaximumY = iMinimumY;

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		iMinimumX = std::min<int64_t>(iMinimumX, rCoordinate.iQ);
		iMaximumX = std::max<int64_t>(iMaximumX, rCoordinate.iQ);
		iMinimumY = std::min<int64_t>(iMinimumY, rCoordinate.iR);
		iMaximumY = std::max<int64_t>(iMaximumY, rCoordinate.iR);
	}

	iMinimumX = std::max<int64_t>(iMinimumX - 1, std::numeric_limits<decltype(GridCoord::iQ)>::min());
	iMaximumX = std::min<int64_t>(iMaximumX + 1, std::numeric_limits<decltype(GridCoord::iQ)>::max());
	iMinimumY = std::max<int64_t>(iMinimumY - 1, std::numeric_limits<decltype(GridCoord::iR)>::min());
	iMaximumY = std::min<int64_t>(iMaximumY + 1, std::numeric_limits<decltype(GridCoord::iR)>::max());

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

			GridCoord coordinate {.iQ = static_cast<int32_t>(j), .iR = static_cast<int32_t>(i)};
			bool bIsActive = std::ranges::any_of(game::gpGame->mActiveCoordinates, [&coordinate](const GridCoord& rActiveCoordinate)
			{
				return rActiveCoordinate == coordinate;
			});
			bool bIsSubscribed = false;
			int64_t iClientsInCell = 0;

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

			// Red interior border for subscribed cells
			if (bIsSubscribed && iCellSize >= 20)
			{
				static constexpr int64_t kiInset = 2;
				HPEN hpenOld2 = static_cast<HPEN>(SelectObject(hdcBuffer, sHpenRed));
				HBRUSH hbrushOld = static_cast<HBRUSH>(SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH)));
				Rectangle(hdcBuffer, static_cast<int>(iCellLeft + kiInset), static_cast<int>(iCellTop + kiInset), static_cast<int>(iCellLeft + iCellSize - kiInset + 1), static_cast<int>(iCellTop + iCellSize - kiInset + 1));
				SelectObject(hdcBuffer, hbrushOld);
				SelectObject(hdcBuffer, hpenOld2);
			}

			// Cell labels for active cells
			if (bIsActive && iCellSize >= 24)
			{
				game::ServerCellStats cellStatistics = game::GetServerCellStatistics((*game::gpGame->mCells.at(coordinate).pCurrent));
				int64_t iEntityCount = std::ranges::fold_left(cellStatistics.entityCounts, cellStatistics.iExplosions, std::plus {});

				SetTextColor(hdcBuffer, RGB(220, 220, 220));

				int64_t iLineLength = std::min<int64_t>(std::snprintf(buffer.data(), buffer.size(), "(%d,%d)", static_cast<int>(j), static_cast<int>(i)), static_cast<int64_t>(buffer.size()) - 1);
				TextOutA(hdcBuffer, static_cast<int>(iCellLeft + 2), static_cast<int>(iCellTop + 2), buffer.data(), static_cast<int>(iLineLength));

				iLineLength = std::min<int64_t>(std::snprintf(buffer.data(), buffer.size(), "%lld", iEntityCount), static_cast<int64_t>(buffer.size()) - 1);
				TextOutA(hdcBuffer, static_cast<int>(iCellLeft + 2), static_cast<int>(iCellTop + 28), buffer.data(), static_cast<int>(iLineLength));

				if (iClientsInCell > 0)
				{
					SetTextColor(hdcBuffer, RGB(150, 200, 255));
					iLineLength = std::min<int64_t>(std::snprintf(buffer.data(), buffer.size(), "%lld", iClientsInCell), static_cast<int64_t>(buffer.size()) - 1);
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
		int64_t iEnd = static_cast<int64_t>(text.find('\n', static_cast<size_t>(iStart)));
		if (iEnd == -1)
		{
			iEnd = static_cast<int64_t>(text.size());
		}

		std::string_view line = text.substr(static_cast<size_t>(iStart), static_cast<size_t>(iEnd - iStart));
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
			std::memcpy(pcDestination, sProfileText.data(), sProfileText.size());
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

void HandleServerClick(HWND hwnd, int64_t iX, int64_t iY)
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

static void PaintProfilePanel(HDC hdcBuffer, int64_t iLeft, int64_t iTop, [[maybe_unused]] int64_t iWidth, int64_t iHeight)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	int64_t iTextX = iLeft + 10;
	int64_t iTextY = iTop;
	int64_t iLineHeight = 32;

	// Heap: std::string operations for clipboard cache
	ScopedSuppressAllocationTracking suppress;
	sProfileText.clear();

	char pcLine[256] {};
	int64_t iLineLength = std::min<int64_t>(std::snprintf(pcLine, sizeof(pcLine), "FPS: %lld  Potential: %lld", gpProfileManager->mFullUpdatesInTheLastSecond.Get(), gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average() > 0 ? 1'000'000 / gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average() : 0i64), static_cast<int64_t>(sizeof(pcLine)) - 1);
	SetTextColor(hdcBuffer, RGB(100, 180, 255));
	TextOutA(hdcBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, static_cast<int>(iLineLength));
	sProfileText += pcLine;
	sProfileText += "\n";
	iTextY += iLineHeight + 4;

	// Re-evaluate row visibility on the shared 2s cadence (timers and counters together this paint).
	bool bReevaluate = gpProfileManager->TickVisibilityCadence();

	{
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		FormatCpuTimersText(rWorkbuffer, bReevaluate);
		std::string_view timers = rWorkbuffer.View();
		SetTextColor(hdcBuffer, RGB(220, 220, 220));
		PaintWorkbufferText(hdcBuffer, timers, iTextX, iTextY, iLineHeight);
		sProfileText += timers;
	}

	iTextY += 4;

	{
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		FormatCpuCountersText(rWorkbuffer, bReevaluate);
		std::string_view counters = rWorkbuffer.View();
		SetTextColor(hdcBuffer, RGB(150, 220, 150));
		PaintWorkbufferText(hdcBuffer, counters, iTextX, iTextY, iLineHeight);
		sProfileText += counters;
	}

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	iTextY += 4;
	SetTextColor(hdcBuffer, RGB(220, 180, 100));

	auto MemoryLine = [&](const char* pcFormat, int64_t iValue)
	{
		int64_t iMemoryLineLength = std::min<int64_t>(std::snprintf(pcLine, sizeof(pcLine), pcFormat, iValue), static_cast<int64_t>(sizeof(pcLine)) - 1);
		TextOutA(hdcBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, static_cast<int>(iMemoryLineLength));
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
	// Bind each tab name by value: std::views::enumerate over kpTabNames yields a const std::string_view&, which PREfast C26445 (gsl.view) rejects.
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

	// Bottom line across non-tab area
	HPEN hpenOld = static_cast<HPEN>(SelectObject(hdcBuffer, sHpenGray));
	int64_t iTabsEnd = kiTabBarLeft + kiTabCount * kiTabWidth;
	MoveToEx(hdcBuffer, static_cast<int>(iTabsEnd), kiTabHeight, nullptr);
	LineTo(hdcBuffer, static_cast<int>(iWidth), kiTabHeight);
	SelectObject(hdcBuffer, hpenOld);
}

void PaintServerDisplay(HWND hwnd)
{
	PAINTSTRUCT paintStructure {};
	HDC hdc = BeginPaint(hwnd, &paintStructure);

	RECT clientRectangle {};
	GetClientRect(hwnd, &clientRectangle);
	int64_t iWidth = static_cast<int64_t>(clientRectangle.right) - clientRectangle.left;
	int64_t iHeight = static_cast<int64_t>(clientRectangle.bottom) - clientRectangle.top;

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

	game::ServerCellStats originStatistics = game::GetServerCellStatistics((*game::gpGame->mCells.at(kOriginCoordinate).pCurrent));
	int64_t iTick = originStatistics.iTick;
	float fCurrentTime = originStatistics.durationCurrentTime.count();

	const std::vector<ClientConnection>& rClients = gpServer->mClients;
	int64_t iClientCount = std::ssize(rClients);

	int64_t iActiveCells = std::ssize(game::gpGame->mActiveCoordinates);

	// Left half: text stats
	int64_t iTextX = 10;
	int64_t iTextY = 10;
	int64_t iLineHeight = 36;
	char pcLine[256] {};

	auto TextLine = [&](const char* pcFormat, auto... arguments)
	{
		int64_t iTextLength = std::min<int64_t>(std::snprintf(pcLine, sizeof(pcLine), pcFormat, arguments...), static_cast<int64_t>(sizeof(pcLine)) - 1);
		TextOutA(hdcBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, static_cast<int>(iTextLength));
		iTextY += iLineHeight;
	};

	SetTextColor(hdcBuffer, RGB(100, 180, 255));
	TextLine("FPS: %lld", gpProfileManager->mFullUpdatesInTheLastSecond.Get());
	int64_t iFrameUpdateMicroseconds = gpProfileManager->GetCpuTimer(game::kCpuTimerFrameUpdate).smoothedMicroseconds.Average();
	int64_t iPotentialFramesPerSecond = iFrameUpdateMicroseconds > 0 ? 1'000'000 / iFrameUpdateMicroseconds : 0;
	TextLine("Potential: %lld", iPotentialFramesPerSecond);
	iTextY += iLineHeight;

	SetTextColor(hdcBuffer, RGB(200, 200, 200));

	TextLine("Tick: %lld", iTick);
	TextLine("Time: %.2f s", static_cast<double>(fCurrentTime));
	TextLine("Active cells: %lld", iActiveCells);
	int64_t iTextLength = std::min<int64_t>(std::snprintf(pcLine, sizeof(pcLine), "Clients: %lld", iClientCount), static_cast<int64_t>(sizeof(pcLine)) - 1);
	TextOutA(hdcBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, static_cast<int>(iTextLength));
	iTextY += iLineHeight;

	SetTextColor(hdcBuffer, RGB(150, 220, 150));

	for (int64_t i = 0; i < std::ssize(sTotalEntityCounts); ++i)
	{
		TextLine("%s: %lld", game::kServerEntityCountNames[i], sTotalEntityCounts[i]);
	}
	iTextLength = std::min<int64_t>(std::snprintf(pcLine, sizeof(pcLine), "Explosions: %lld", gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount), static_cast<int64_t>(sizeof(pcLine)) - 1);
	TextOutA(hdcBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, static_cast<int>(iTextLength));

#if !defined(ENABLE_CRT_DEBUG_HEAP)
	iTextY += iLineHeight * 2;
	SetTextColor(hdcBuffer, RGB(220, 180, 100));

	TextLine("Committed: %lld MiB", gpProfileManager->miMimallocCommittedMebibytes);
	TextLine("Peak cmtd: %lld MiB", gpProfileManager->miMimallocPeakCommittedMebibytes);
	TextLine("Heap used: %lld MiB", gpProfileManager->miMimallocHeapUsedMebibytes);
	iTextLength = std::min<int64_t>(std::snprintf(pcLine, sizeof(pcLine), "Peak heap: %lld MiB", gpProfileManager->miMimallocPeakHeapUsedMebibytes), static_cast<int64_t>(sizeof(pcLine)) - 1);
	TextOutA(hdcBuffer, static_cast<int>(iTextX), static_cast<int>(iTextY), pcLine, static_cast<int>(iTextLength));
#endif

	PaintTabBar(hdcBuffer, iWidth);

	// Right side content area (below tab bar)
	int64_t iContentLeft = kiTabBarLeft;
	int64_t iContentTop = kiTabHeight + 5;
	int64_t iContentWidth = iWidth - iContentLeft - 10;
	int64_t iContentHeight = iHeight - iContentTop - 10;

	if (seActiveTab == ServerTab::kMap)
	{
		PaintGridMap(hdcBuffer, pcLine, iContentLeft, iContentTop, iContentWidth, iContentHeight, rClients);
	}
	else if (seActiveTab == ServerTab::kProfile)
	{
		PaintProfilePanel(hdcBuffer, iContentLeft, iContentTop, iContentWidth, iContentHeight);
	}

	BitBlt(hdc, 0, 0, static_cast<int>(iWidth), static_cast<int>(iHeight), hdcBuffer, 0, 0, SRCCOPY);

	EndPaint(hwnd, &paintStructure);
}

} // namespace engine

#endif // BT_SERVER
