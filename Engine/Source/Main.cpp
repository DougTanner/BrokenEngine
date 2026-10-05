#include "File/PackChunks.h"
#include "Input/Input.h"
#include "Memory/GlobalAllocator.h"
#include "Server/ServerDisplay.h"
#include "Ui/GameSettings.h"
#include "Ui/GraphicsSettings.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SoundSettings.h"
#include "Ui/SoundSettingsWrappersBase.h"
#include "CrashReport.h"

#include "Frame/TerrainUtils.h"
#include "Profile/ProfileManager.h"
#include "Ui/Screens/TweaksScreen/TweaksScreen.h"
#include "Game.h"

namespace engine
{

#if defined(BT_CLIENT)
static HCURSOR sHcursorArrow = nullptr;
static HCURSOR sHcursorCrosshair = nullptr;
#endif

static HWND sHWindow = nullptr;
static HMONITOR sHmonitor = nullptr;
static MONITORINFO sMonitorInfo {};

static bool sbHasFocus = false;

#if defined(BT_CLIENT)
static int64_t siWindowStyle = 0;
static RECT sWindowRectangle {};

#endif

static LRESULT CALLBACK WindowProcedure(HWND hWindow, UINT uiMessage, WPARAM uiWordParameter, LPARAM iLongParameter);

#if defined(BT_CLIENT)
static void FindMonitor(bool bUseCurrentRectangle);
static VkExtent2D SetupWindow(bool bFullscreen, int64_t& riWindowStyle, RECT& rWindowRectangle);

// Effective fullscreen precedence: the agent fullscreen override (runtime, in-memory) wins first; else false when
// --windowed WxH is set (reproducible agent capture geometry); else the saved gFullscreen preference. Override only
// at the read sites — never gFullscreen.Set() (it is persisted to GraphicsSettings.bin, so mutating it would silently
// rewrite the user's saved fullscreen preference).
static bool WantedFullscreen()
{
	if (gAgentFullscreenOverride.has_value())
	{
		return *gAgentFullscreenOverride;
	}
	if (gLaunchOptions.vkWindowedExtent.width != 0 && gLaunchOptions.vkWindowedExtent.height != 0)
	{
		return false;
	}
	return gFullscreen.Get<bool>();
}

#endif
static bool ProcessMessages();

static int64_t HandleEagerLoadCompletion()
{
	try
	{
		std::ignore = gpFileManager->mpPackChunks->GetEagerChunkMap();
	}
	catch (const std::system_error&)
	{
		throw;
	}
	catch (const std::runtime_error& rException)
	{
		if ((gLaunchOptions.iAgentPort == 0))
		{
			MessageBox(nullptr, rException.what(), game::kGameName.data(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
		}
		return 1;
	}

	return 0;
}

static int64_t MainThread(HINSTANCE hInstance)
{
	common::SetupExceptionHandling();

	LOG(kDefault, kInfo, "\nGame name: {}", game::kGameName);
	LOG(kDefault, kInfo, "Game version: {}", game::kiGameVersion);
	LOG(kDefault, kInfo, "Compiled with Windows 10 SDK version: {}.{}", VER_PRODUCTBUILD, VER_PRODUCTBUILD_QFE);
#if defined(BT_CLIENT)
	LOG(kDefault, kInfo, "Compiled with Vulkan SDK version: {}\n", VK_HEADER_VERSION);
	static_assert(VK_HEADER_VERSION >= 341, "Update the Vulkan SDK");
#endif

	if (!XMVerifyCPUSupport()) [[unlikely]]
	{
		throw std::runtime_error("Your CPU does not support SSE4.1 instructions");
	}

	// Disable CRT FMA3 auto-detection for deterministic math across CPUs
	if (_set_FMA3_enable(0) != 0) [[unlikely]]
	{
		throw std::runtime_error("Unable to disable FMA3");
	}

	if (SetProcessDPIAware() == 0) [[unlikely]]
	{
		throw std::runtime_error("SetProcessDPIAware failed");
	}

#if defined(BT_CLIENT)
	sHcursorArrow = LoadCursor(nullptr, IDC_ARROW);
	sHcursorCrosshair = LoadCursor(nullptr, IDC_CROSS);
	common::ScopedLambda destroyCursor([]()
	{
		DestroyCursor(sHcursorArrow);
		DestroyCursor(sHcursorCrosshair);
	});
#endif // BT_CLIENT

#if defined(BT_CLIENT)
	// Save one core for the main thread, and one core for the render thread
	int64_t iBackgroundThreadCount = std::max(1i64, common::HardwareCoreCount() - 1 - 1);
#else
	// Server has no render thread
	int64_t iBackgroundThreadCount = std::max(1i64, common::HardwareCoreCount() - 1);
#endif
	auto pMultithreading = std::make_unique<common::Multithreading>(iBackgroundThreadCount);

	auto pProfileManager = std::make_unique<game::ProfileManager>();

	std::future<void> readDxDiag;
	if constexpr (kbDxDiag)
	{
		if (IsDebuggerPresent() == 0) [[likely]]
		{
			readDxDiag = std::async(std::launch::async, common::ThreadLocal::Entry(ReadDxDiag, common::kiMinWorkbufferSize, common::kThreadDxDiag));
		}
	}

#if defined(BT_CLIENT)
	auto pRawInputManager = std::make_unique<RawInputManager>();

	auto pAudioManager = std::make_unique<AudioManager>();
#endif

	auto pNetworkManager = std::make_unique<NetworkManager>();

	// Agent command channel (loopback JSON control, drained on the main thread). Constructed after NetworkManager
	// so WSAStartup (via ENet init) is done. Dormant unless --agent-port is passed; RAII teardown in reverse order.
	std::unique_ptr<AgentCommandServer> pAgentCommandServer;
	common::ScopedLambda clearAgentCommandServer([]()
	{
		gpAgentCommandServer = nullptr;
	});
#if defined(BT_CLIENT)
	// Client-only synthetic-input + UI-registry layer, active alongside the agent channel. Ctors set their gp*
	// globals; RAII teardown (reverse order) nulls them before the command server tears down.
	std::unique_ptr<AgentUiRegistry> pAgentUiRegistry;
	std::unique_ptr<AgentInput> pAgentInput;
#endif
	if constexpr (kbAgent)
	{
		if (gLaunchOptions.iAgentPort != 0)
		{
			try
			{
				pAgentCommandServer = std::make_unique<AgentCommandServer>(gLaunchOptions.iAgentPort);
			}
			catch (const std::system_error&)
			{
				throw;
			}
			catch (const std::runtime_error&)
			{
				// Ctor already logged the concrete error. Drain eager startup work while managers are still alive so a
				// pending required-asset failure is handled before normal cleanup.
				std::ignore = HandleEagerLoadCompletion();
				return 1;
			}
			gpAgentCommandServer = pAgentCommandServer.get();
#if defined(BT_CLIENT)
			pAgentUiRegistry = std::make_unique<AgentUiRegistry>();
			pAgentInput = std::make_unique<AgentInput>();
#endif
		}
	}

	WNDCLASSEX windowClassExtended
	{
		.cbSize = sizeof(windowClassExtended),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WindowProcedure,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInstance,
		.hIcon = LoadIcon(nullptr, IDI_APPLICATION),
		.hCursor = nullptr,
		.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
		.lpszMenuName = nullptr,
		.lpszClassName = game::kGameName.data(),
		.hIconSm = LoadIcon(nullptr, IDI_APPLICATION),
	};
	int64_t iAtom = RegisterClassEx(&windowClassExtended);
	if (iAtom == 0)
	{
		throw std::runtime_error("RegisterClassEx failed");
	}
	common::ScopedLambda unregisterClass([&hInstance]()
	{
		LOG(kDefault, kDebug, "Unregister class");
		UnregisterClass(game::kGameName.data(), hInstance);
	});

#if defined(BT_CLIENT)
	LoadGraphicsSettings();
	gVkWantedFramebufferExtent2D = SetupWindow(WantedFullscreen(), siWindowStyle, sWindowRectangle);
#else
	int64_t iWindowStyle = WS_POPUP;
	RECT windowRectangle {};
	SystemParametersInfo(SPI_GETWORKAREA, 0, &windowRectangle, 0);
#endif // BT_CLIENT

#if defined(BT_CLIENT)
	sHWindow = CreateWindow(game::kGameName.data(), game::kGameName.data(), static_cast<DWORD>(siWindowStyle), sWindowRectangle.left, sWindowRectangle.top, sWindowRectangle.right - sWindowRectangle.left, sWindowRectangle.bottom - sWindowRectangle.top, nullptr, nullptr, hInstance, nullptr);
#else
	sHWindow = CreateWindow(game::kGameName.data(), game::kGameName.data(), static_cast<DWORD>(iWindowStyle), windowRectangle.left, windowRectangle.top, windowRectangle.right - windowRectangle.left, windowRectangle.bottom - windowRectangle.top, nullptr, nullptr, hInstance, nullptr);
#endif
	if (sHWindow == nullptr)
	{
		throw std::runtime_error("CreateWindow failed");
	}
	common::ScopedLambda destroyWindow([]()
	{
#if defined(BT_SERVER)
		// Set the shutdown flag before the teardown message drain, including exception unwinding, so clicks cannot request a repaint.
		gbQuit = true;

		// Drop any paint region an external event (uncover, resize, DPI change) added after the main loop's final
		// drain: the server display reads game state, and by here the game object is already gone. ValidateRect
		// leaves nothing for this drain to dispatch WM_PAINT for, and nothing invalidates the window afterwards.
		// ValidateRect(nullptr) redraws every window in the system.
		if (sHWindow != nullptr)
		{
			ValidateRect(sHWindow, nullptr);
		}
		ProcessMessages();
#endif
		// No client pump here: Game, Graphics/ImGui, and Input are already destroyed, and the client WindowProcedure
		// dereferences game::gpGame on WM_SETFOCUS.

		if (sHWindow != nullptr)
		{
			LOG(kDefault, kDebug, "Destroy window");
			DestroyWindow(sHWindow);
			sHWindow = nullptr;
		}
	});

#if defined(BT_CLIENT)
	LoadSoundSettings();
	LoadGameSettings();
	if (gMuteInBackground.Get<bool>() || gLaunchOptions.iAgentPort != 0)
	{
		gpAudioManager->Suspend();
	}

	// Create terrain collision data (before Graphics, which creates Islands that reads beach elevation)
	auto pIslandTerrain = std::make_unique<IslandTerrain>();
	auto pIslandTerrainResidency = std::make_unique<IslandTerrainResidency>();

	// Wait for islands to load and initialize heightmaps before Graphics ctor. Terrain mesh CPU
	// slices are reclaimed immediately afterward; Graphics records its stable empty arena at boot.
	gpProfileManager->BootStart(kBootTimerWaitForIslands);
	if (HandleEagerLoadCompletion() != 0)
	{
		return 1;
	}
	gpIslandTerrain->WaitForElevationMaps();
	gpProfileManager->BootStop(kBootTimerWaitForIslands);

	// Register tweaks sections before the Graphics ctor builds ImGuiManager
	RegisterEngineTweakSections();
	game::RegisterGameTweakSections();

	gpProfileManager->BootStart(kBootTimerVulkan);
	auto pGraphics = std::make_unique<Graphics>(hInstance, sHWindow);

	auto pCamera = std::make_unique<game::Camera>();
	auto pGame = std::make_unique<game::Game>();

	auto pInput = std::make_unique<Input>();
	gpInput = pInput.get();

	// Load tweaks settings (requires both Game and ImGuiManager)
	game::LoadTweaksSettings();

	// Load persistent client state (focused fleet/ship + zoom). Requires engine::gpCamera and gpGame; both exist by here.
	game::LoadClientState();

	gpProfileManager->BootStop(kBootTimerVulkan);

	gpProfileManager->BootStart(kBootTimerWaitForPriorityTextures);
	gpTextureManager->WaitForTextures(TextureManager::kPriorityTextures);
	gpProfileManager->BootStop(kBootTimerWaitForPriorityTextures);

	{
		// Heap: try_emplace may allocate the origin entry.
		ScopedSuppressAllocationTracking suppress;
		game::FrameInterpolate::AllocateAndCopy(pGame->mRenderInterpolates.try_emplace(kOriginCoordinate).first->second, pGame->RenderFrame(pGame->mClientGridCoordinate).interpolate);
	}
	gpCamera->Update(pGame->mRenderInterpolates.at(kOriginCoordinate), static_cast<float>(pGame->mfLastRenderFrameSeconds));

	// Render and present once per framebuffer, then show window. An iteration whose head check recreates or defers the
	// swapchain is skipped, not retried; the count is captured first because a swapchain-tier Destroy() resets the
	// command buffer manager.
	gpProfileManager->BootStart(kBootTimerRenderPresent);
	std::vector<GridCoord> bootActiveCoordinates = {kOriginCoordinate};
	int64_t iBootFramebufferCount = std::ssize(gpCommandBufferManager->mPerFramebufferCommandBuffers);
	for (int64_t i = 0; i < iBootFramebufferCount; ++i)
	{
		if (pGame->HandleDeferredSwapchain())
		{
			continue;
		}
		gpGraphics->RenderGlobal(std::chrono::duration<float>(pGame->RenderFrame(pGame->mClientGridCoordinate).interpolate.fCurrentTime));
		gpGraphics->RenderMainPresentAcquire(gpSwapchainManager->miFramebufferIndex, pGame->mRenderInterpolates, bootActiveCoordinates, kOriginCoordinate, std::chrono::duration<float>(pGame->RenderFrame(pGame->mClientGridCoordinate).interpolate.fCurrentTime));
	}
	gpProfileManager->BootStop(kBootTimerRenderPresent);

	// Agent-mode launch stays minimized and must not steal focus from the user's active app
	ShowWindow(sHWindow, gLaunchOptions.iAgentPort != 0 ? SW_SHOWMINNOACTIVE : SW_SHOWDEFAULT);
#else
	auto pIslandTerrain = std::make_unique<IslandTerrain>();
	if (HandleEagerLoadCompletion() != 0)
	{
		return 1;
	}
	gpIslandTerrain->WaitForElevationMaps(game::NavigationThresholdElevation(gBaseHeight.mfCurrent), game::NavigationClearanceMeters());

	auto pGame = std::make_unique<game::Game>();

	ShowWindow(sHWindow, gLaunchOptions.iAgentPort != 0 ? SW_SHOWMINNOACTIVE : SW_SHOWNOACTIVATE);
#endif // BT_CLIENT
	common::ScopedLambda hideWindow([]()
	{
		LOG(kDefault, kDebug, "Hide window");
		ShowWindow(sHWindow, SW_HIDE);
	});
#if defined(BT_CLIENT)
	// Agent-mode launch must not steal focus from the user's active app
	if (gLaunchOptions.iAgentPort == 0)
	{
		SetForegroundWindow(sHWindow);
		BringWindowToTop(sHWindow);
		SetFocus(sHWindow);
	}
#else
	if (gLaunchOptions.iAgentPort == 0)
	{
		SetFocus(sHWindow);
	}
#endif
	ProcessMessages();

	gpProfileManager->BootLog();

	common::ScopedLambda disableAllocationTracking([]()
	{
		gbAllocationTrackingReady.store(false, std::memory_order_relaxed);
	});
	gbAllocationTrackingReady.store(true, std::memory_order_relaxed);
	pGame->mTimeStep.mRealTime.Reset();

	LOG(kDefault, kInfo, "\nEnter main loop");
	LogIndent(1);
	while (true)
	{
		gpProfileManager->CpuStart(kCpuTimerMessagesAndInput);

		[[maybe_unused]] bool bLostFocus = ProcessMessages();
		if (gbQuit) [[unlikely]]
		{
			break;
		}

#if defined(BT_CLIENT)
		// Reconcile a pending fullscreen toggle here, in the main loop only — never from the teardown / boot
		// ProcessMessages() calls, so a mid-shutdown mismatch can't SetWindowPos a dying window. Read through
		// WantedFullscreen() so the --windowed / agent overrides fold in and gFullscreen is never mutated.
		bool bWantedFullscreen = WantedFullscreen();
		bool bIsFullscreen = (siWindowStyle & WS_POPUP) != 0;
		if (bIsFullscreen != bWantedFullscreen)
		{
			SetupWindow(bWantedFullscreen, siWindowStyle, sWindowRectangle);
			SetWindowLongPtr(sHWindow, GWL_STYLE, static_cast<LONG_PTR>(static_cast<LONG>(siWindowStyle)));
			// SWP_NOZORDER | SWP_NOACTIVATE: the agent fullscreen command reaches this path, and the harness must never steal foreground focus.
			SetWindowPos(sHWindow, nullptr, sWindowRectangle.left, sWindowRectangle.top, sWindowRectangle.right - sWindowRectangle.left, sWindowRectangle.bottom - sWindowRectangle.top, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		// Drain agent commands before input so injected input scripts act on the same frame.
		// The server drains in GameBase::ServerUpdate instead, matching the debug-control packet ordering.
		if (gpAgentCommandServer != nullptr) [[unlikely]]
		{
			gpAgentCommandServer->Drain();

			// Advance the active synthetic-input script one step before input/ImGui so its ImGui IO events and
			// RawInput-overlay state land in this frame (RawInputManager::Update runs later via ProcessInput).
			if (gpAgentInput != nullptr) [[unlikely]]
			{
				gpAgentInput->AdvanceFrame();
			}
		}
#endif

#if defined(BT_CLIENT)
		pGame->ProcessInput(bLostFocus);
		if (pGame->mGameFlags & engine::GameFlags::kQuit) [[unlikely]]
		{
			break;
		}
#endif

		gpProfileManager->CpuStop(kCpuTimerMessagesAndInput);

#if defined(BT_CLIENT)
		pGame->ClientUpdate();
#else
		pGame->ServerUpdate();
#endif // BT_CLIENT

#if defined(BT_CLIENT)
		try
		{
			gpTextureUploadManager->RethrowException();
			pGame->Render();
		}
		catch (const std::system_error& rException)
		{
			if (rException.code() != VkErrorCode(VK_ERROR_DEVICE_LOST))
			{
				throw;
			}
			LOG(kDefault, kDebug, "Caught Vulkan device loss: {}", rException.what());
			pGraphics.reset();
			pGraphics = std::make_unique<Graphics>(hInstance, sHWindow);
		}

		auto it = pGame->mCoordinateFrames.find(game::gpGame->mClientGridCoordinate);
		pAudioManager->Update(it != pGame->mCoordinateFrames.end() && it->second.iSnapshotCount > 0 ? &pGame->RenderFrame(game::gpGame->mClientGridCoordinate) : nullptr);
#else
		{
			// Heap: Win32 InvalidateRect may trigger internal GDI allocations
			ScopedSuppressAllocationTracking suppress;
			ServerUpdateDisplayStatistics();

			// Throttle full-window GDI repaint to a fraction of the tick rate — paint cost dwarfs stat aggregation, and the window shows only coarse stats/map. Clicks still repaint immediately via WM_LBUTTONDOWN.
			static constexpr int64_t kiServerDisplayRepaintTicks = 8;
			static int64_t siServerDisplayRepaintCounter = 0;
			if (++siServerDisplayRepaintCounter >= kiServerDisplayRepaintTicks)
			{
				siServerDisplayRepaintCounter = 0;

				// Skip the repaint when it would blit nothing new: a minimized/hidden window paints offscreen for no benefit, and a visible window whose displayed stats/map are unchanged need not repaint an identical frame.
				// Keep a slow heartbeat while visible-but-unchanged so the free-running tick/timer text (deliberately outside the content hash) stays visibly alive instead of reading as a hung server.
				static constexpr int64_t kiServerDisplayHeartbeatWindows = 4; // 4 x 8-tick windows = 1 Hz at the 32 Hz tick rate
				static int64_t siServerDisplayHeartbeatCounter = 0;
				if (!IsIconic(sHWindow) && IsWindowVisible(sHWindow))
				{
					bool bHeartbeat = ++siServerDisplayHeartbeatCounter >= kiServerDisplayHeartbeatWindows;
					if (ServerDisplayContentChanged() || bHeartbeat)
					{
						siServerDisplayHeartbeatCounter = 0;
						InvalidateRect(sHWindow, nullptr, FALSE);
					}
				}
			}
		}
#endif // BT_CLIENT
	}
	LogIndent(-1);
	LOG(kDefault, kInfo, "Exit main loop\n\n");

	gbAllocationTrackingReady.store(false, std::memory_order_relaxed);

#if defined(BT_SERVER)
	if (!pGame->mGameSaveLoad.Autosave())
	{
		LOG(kDefault, kError, "Final autosave failed during shutdown");
	}
#endif

#if defined(BT_CLIENT)
	game::SaveTweaksSettings();
	SaveSoundSettings();
	SaveGraphicsSettings();
	SaveGameSettings();
	game::SaveClientState();
#endif

	PostQuitMessage(0);
	ProcessMessages();
	return 0;
}

#if defined(BT_CLIENT)
static int64_t siMonitorCount = 0;
static bool sbUseCurrentRectangle = false;

static void FindMonitor(bool bUseCurrentRectangle)
{
	sHmonitor = nullptr;
	sbUseCurrentRectangle = bUseCurrentRectangle;

	if (sbUseCurrentRectangle)
	{
		GetWindowRect(sHWindow, &sWindowRectangle);
		LOG(kDefault, kDebug, "Window left top: {}, {}", sWindowRectangle.left, sWindowRectangle.top);
	}

	LOG(kDefault, kDebug, "Monitors:");
	siMonitorCount = 0;
	EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, [[maybe_unused]] HDC hDeviceContext, [[maybe_unused]] LPRECT pRectangle, [[maybe_unused]] LPARAM iLongParameter) -> BOOL
	{
		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);
		GetMonitorInfo(hMonitor, &monitorInfo);
		bool bPrimary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;
		bool bRectangleIsInMonitor = sWindowRectangle.left >= monitorInfo.rcMonitor.left && sWindowRectangle.left <= monitorInfo.rcMonitor.right && sWindowRectangle.top >= monitorInfo.rcMonitor.top && sWindowRectangle.top <= monitorInfo.rcMonitor.bottom;

		[[maybe_unused]] int64_t iWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
		[[maybe_unused]] int64_t iHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
		LOG(kDefault, kDebug, "  {}: {} x {}{}{}", siMonitorCount++, iWidth, iHeight, bPrimary ? " (Primary)" : "", bRectangleIsInMonitor ? " (Monitor)" : "");

		if (sHmonitor == nullptr || (sbUseCurrentRectangle && bRectangleIsInMonitor) || (!sbUseCurrentRectangle && bPrimary))
		{
			sHmonitor = hMonitor;
			sMonitorInfo = monitorInfo;
		}

		return TRUE;
	}, 0);
	LOG(kDefault, kDebug, "");
}

static VkExtent2D SetupWindow(bool bFullscreen, int64_t& riWindowStyle, RECT& rWindowRectangle)
{
	if (sHWindow == nullptr)
	{
		riWindowStyle = 0;
	}
	else
	{
		riWindowStyle = GetWindowLong(sHWindow, GWL_STYLE) & ~(WS_OVERLAPPEDWINDOW | WS_POPUP);
	}

	FindMonitor(sHWindow != nullptr);

	if (bFullscreen)
	{
		riWindowStyle |= WS_POPUP;
		rWindowRectangle = sMonitorInfo.rcMonitor;
	}
	else
	{
		riWindowStyle |= WS_OVERLAPPEDWINDOW;

		int64_t iX = common::RoundUp<int64_t, 8>(static_cast<int64_t>(0.05f * static_cast<float>(sMonitorInfo.rcMonitor.right)));
		int64_t iY = common::RoundUp<int64_t, 8>(static_cast<int64_t>(0.05f * static_cast<float>(sMonitorInfo.rcMonitor.bottom)));

		if (gLaunchOptions.vkWindowedExtent.width != 0 && gLaunchOptions.vkWindowedExtent.height != 0)
		{
			// Reproducible agent capture geometry: use the requested client size (multiple-of-8 rounded, matching
			// the default inset path), anchored at the monitor's top-left inset.
			int64_t iClientWidth = common::RoundUp<int64_t, 8>(static_cast<int64_t>(gLaunchOptions.vkWindowedExtent.width));
			int64_t iClientHeight = common::RoundUp<int64_t, 8>(static_cast<int64_t>(gLaunchOptions.vkWindowedExtent.height));
			rWindowRectangle.left = static_cast<LONG>(sMonitorInfo.rcMonitor.left + iX);
			rWindowRectangle.top = static_cast<LONG>(sMonitorInfo.rcMonitor.top + iY);
			rWindowRectangle.right = static_cast<LONG>(rWindowRectangle.left + iClientWidth);
			rWindowRectangle.bottom = static_cast<LONG>(rWindowRectangle.top + iClientHeight);
		}
		else
		{
			rWindowRectangle.left = static_cast<LONG>(sMonitorInfo.rcMonitor.left + iX);
			rWindowRectangle.right = static_cast<LONG>(sMonitorInfo.rcMonitor.right - iX);
			rWindowRectangle.top = static_cast<LONG>(sMonitorInfo.rcMonitor.top + iY);
			rWindowRectangle.bottom = static_cast<LONG>(sMonitorInfo.rcMonitor.bottom - iY);
		}
	}

	int64_t iFramebufferWidth = rWindowRectangle.right - rWindowRectangle.left;
	int64_t iFramebufferHeight = rWindowRectangle.bottom - rWindowRectangle.top;
	LOG(kDefault, kDebug, "Set {} window {} x {} at ({}, {})", (riWindowStyle & WS_OVERLAPPEDWINDOW) != 0 ? "WS_OVERLAPPEDWINDOW" : "WS_POPUP", iFramebufferWidth, iFramebufferHeight, rWindowRectangle.left, rWindowRectangle.top);

	if ((riWindowStyle & WS_OVERLAPPEDWINDOW) != 0)
	{
		AdjustWindowRect(&rWindowRectangle, static_cast<DWORD>(riWindowStyle), FALSE);
	}

	return {.width = static_cast<uint32_t>(iFramebufferWidth), .height = static_cast<uint32_t>(iFramebufferHeight)};
}
#endif // BT_CLIENT

static bool ProcessMessages()
{
	// Process messages with PeekMessage() which doesn't block
	MSG message {};
	bool bHasMessage = PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) == TRUE;
	while (bHasMessage)
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
		bHasMessage = PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) == TRUE;
	}

	return !sbHasFocus;
}

static LRESULT CALLBACK WindowProcedure(HWND hWindow, UINT uiMessage, WPARAM uiWordParameter, LPARAM iLongParameter)
{
#if defined(BT_CLIENT)
	// Game handles cursor when ImGui doesn't want the mouse. Guard GetIO(): the ImGui context can be destroyed across a
	// multi-frame deferred swapchain recreate (minimized client), and this fires on the restore frame over the client area.
	if (uiMessage == WM_SETCURSOR && LOWORD(iLongParameter) == HTCLIENT && ImGui::GetCurrentContext() != nullptr && !ImGui::GetIO().WantCaptureMouse)
	{
		SetCursor(game::gpGame->ShouldUseCrosshair() ? sHcursorCrosshair : sHcursorArrow);
		return TRUE;
	}

	bool bInputSuppressed = PhysicalInputSuppressed();

	// When suppressed, keep the ImGui Win32 backend running for lifecycle bookkeeping (focus, tracking) but starve it
	// of physical input messages (mouse/keyboard/char/wheel ranges) so real human activity never becomes ImGui IO.
	// Non-client mouse messages (WM_NCMOUSEMOVE etc.) are deliberately NOT added to the bypass ranges: the backend may
	// queue a physical pos from them, but ImGuiManager::Prepare's re-pin/sentinel is always the last mouse-pos event
	// before NewFrame.
	bool bInputMessage = (uiMessage >= WM_MOUSEFIRST && uiMessage <= WM_MOUSELAST) || (uiMessage >= WM_KEYFIRST && uiMessage <= WM_KEYLAST);
	if (!(bInputSuppressed && bInputMessage))
	{
		if (ImGui_ImplWin32_WndProcHandler(hWindow, uiMessage, uiWordParameter, iLongParameter) != 0)
		{
			return TRUE;
		}
	}

	switch (uiMessage)
	{
		// WM_ACTIVATEAPP stays ungated even when suppressed — DirectXTK Mouse focus bookkeeping, not a physical input feed.
		case WM_ACTIVATEAPP:
			Mouse::ProcessMessage(uiMessage, uiWordParameter, iLongParameter);
			break;

		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_MOUSEHOVER:
			if (!bInputSuppressed)
			{
				Mouse::ProcessMessage(uiMessage, uiWordParameter, iLongParameter);
			}
			break;

		default:
			break;
	}
#endif // BT_CLIENT

	switch (uiMessage)
	{
#if defined(BT_SERVER)
		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
		{
			// Heap: GDI painting creates/destroys kernel objects that may trigger CRT allocations
			ScopedSuppressAllocationTracking suppress;
			PaintServerDisplay(hWindow);
			return 0;
		}

		case WM_KEYDOWN:
		{
			if constexpr (kbDebugInput)
			{
				if (uiWordParameter == VK_F4)
				{
					gbQuit = true;
				}
			}
			return 0;
		}

		case WM_LBUTTONDOWN:
		{
			// Once shutdown has begun, ignore the click instead of invalidating: the teardown drain after the game
			// object is gone would dispatch the resulting WM_PAINT into the game-state-reading server display.
			if (gbQuit)
			{
				return 0;
			}

			HandleServerClick(hWindow, LOWORD(iLongParameter), HIWORD(iLongParameter));
			InvalidateRect(hWindow, nullptr, FALSE);
			return 0;
		}
#endif // BT_SERVER

		case WM_SYSCOMMAND:
		{
			// Suppress system commands that enter modal loops and block the main thread
			WORD uiSystemCommand = uiWordParameter & 0xFFF0;
			if (uiSystemCommand == SC_KEYMENU
#if defined(BT_SERVER)
				|| uiSystemCommand == SC_MOVE
				|| uiSystemCommand == SC_SIZE
				|| uiSystemCommand == SC_MAXIMIZE
				|| uiSystemCommand == SC_RESTORE
#endif
				)
			{
				return 0;
			}
			break;
		}

		case WM_SETFOCUS:
		{
			LOG(kDefault, kInfo, "WM_SETFOCUS");

			if (!sbHasFocus)
			{
				sbHasFocus = true;

#if defined(BT_CLIENT)
				SetCursor(game::gpGame->ShouldUseCrosshair() ? sHcursorCrosshair : sHcursorArrow);

				gpAudioManager->Resume();

				gpRawInputManager->UpdateFocus(true, sHWindow);
#endif // BT_CLIENT
			}

			break;
		}

		case WM_KILLFOCUS:
		{
			LOG(kDefault, kInfo, "WM_KILLFOCUS");

			if (sbHasFocus)
			{
				sbHasFocus = false;

#if defined(BT_CLIENT)
				if (gMuteInBackground.Get<bool>())
				{
					gpAudioManager->Suspend();
				}

				gpRawInputManager->UpdateFocus(false, sHWindow);
#endif // BT_CLIENT
			}

			break;
		}

		case WM_INPUT:
		{
#if defined(BT_CLIENT)
			gpRawInputManager->HandleRawInput(iLongParameter);
#endif
			break; // WM_INPUT must reach DefWindowProc so the system can free the RAWINPUT handle
		}

		case WM_SIZE:
		{
#if defined(BT_CLIENT)
			gVkWantedFramebufferExtent2D = {.width = static_cast<uint32_t>(iLongParameter) & 0xFFFF, .height = static_cast<uint32_t>(iLongParameter) >> 16};
			LOG(kDefault, kDebug, "WM_SIZE: {} x {}", gVkWantedFramebufferExtent2D.width, gVkWantedFramebufferExtent2D.height);
#endif
			break;
		}

		case WM_CLOSE:
		{
			LOG(kDefault, kDebug, "WM_CLOSE");
			gbQuit = true;
			return 0;
		}

		case WM_DESTROY:
		{
			LOG(kDefault, kDebug, "WM_DESTROY");
			gbQuit = true;
			sHWindow = nullptr;
			break;
		}

		default:
		{
			break;
		}
	}

	return DefWindowProc(hWindow, uiMessage, uiWordParameter, iLongParameter);
}

} // namespace engine

static int64_t ProcessMain(HINSTANCE hInstance)
{
	if (!engine::ParseLaunchOptions())
	{
		return 0; // Fatal launch-option error (e.g. --agent-port out of range); already logged kError.
	}

	if (!engine::gLaunchOptions.logFile.empty())
	{
		common::EnableLogFile(engine::gLaunchOptions.logFile);
	}

	if (!engine::gLaunchOptions.appDataDirectory.empty())
	{
		engine::SetCrashReportAppDataDirectory(engine::gLaunchOptions.appDataDirectory.c_str());
	}

	// Must follow the override above: the crash handler only selects between the buffers this fills, so it runs as early
	// as the launch options allow.
	engine::ResolveCrashReportPaths();

	std::unique_ptr<void, decltype(&CloseHandle)> pMutex(nullptr, &CloseHandle);
	if constexpr (kbSingleInstance)
	{
		HANDLE hMutex = CreateMutex(nullptr, TRUE, "BrokenEngineSandboxServer");
		pMutex.reset(hMutex);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// An agent-launched instance must never block on a modal dialog — fail fast so AgentHarness sees the exit.
			if ((engine::gLaunchOptions.iAgentPort != 0))
			{
				LOG(kDefault, kError, "Another instance is already running; --agent-port launch aborting");
				return 0;
			}

			MessageBox(nullptr, "Server is already running.", game::kGameName.data(), MB_OK | MB_SYSTEMMODAL);
			return 0;
		}
	}

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

#if defined(BT_CLIENT)
	// Windows::Foundation::Initialize is required for XAudio2
	int64_t iFoundationResult = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
	if (iFoundationResult != S_OK) [[unlikely]]
	{
		LOG(kDefault, kError, "Windows::Foundation::Initialize failed: {:#x}", static_cast<uint32_t>(iFoundationResult));
		if ((engine::gLaunchOptions.iAgentPort == 0))
		{
			MessageBox(nullptr, common::HresultToString(static_cast<HRESULT>(iFoundationResult)).data(), "Windows::Foundation::Initialize", MB_OK | MB_SYSTEMMODAL);
		}
		return 0;
	}
#endif

#if defined(BT_CLIENT)
	auto pTextureUploadManager = std::make_unique<engine::TextureUploadManager>();
#endif
	std::unique_ptr<engine::FileManager> pFileManager;
	int64_t iResult = 0;
	try
	{
		try
		{
			pFileManager = std::make_unique<engine::FileManager>();
		}
		catch (const std::system_error&)
		{
			throw;
		}
		catch (const std::runtime_error& rException)
		{
			if ((engine::gLaunchOptions.iAgentPort == 0))
			{
				MessageBox(nullptr, rException.what(), game::kGameName.data(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
			}
			iResult = 1;
		}

		if (pFileManager != nullptr)
		{
			iResult = engine::MainThread(hInstance);
		}
	}
	catch (const std::exception& rException)
	{
		if (IsDebuggerPresent() != 0) [[unlikely]]
		{
			throw;
		}
		engine::HandleException(&rException);
	}
	catch (...)
	{
		if (IsDebuggerPresent() != 0) [[unlikely]]
		{
			throw;
		}
		engine::HandleException();
	}

#if defined(BT_CLIENT)
	LOG(kDefault, kDebug, "Windows foundation uninitialize");
	Windows::Foundation::Uninitialize();
#endif

	return iResult;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, [[maybe_unused]] _In_opt_ HINSTANCE hPreviousInstance, [[maybe_unused]] _In_ LPWSTR pcCommandLine, [[maybe_unused]] _In_ int iShowCommand)
{
	// No exception handling here: MainThread installs it as its first statement.
	return static_cast<int>(common::ThreadLocal::Entry(ProcessMain, 10 * 1'024 * 1'024, std::nullopt, false)(hInstance));
}
