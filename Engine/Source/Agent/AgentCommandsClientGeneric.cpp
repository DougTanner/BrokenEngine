#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/AgentCommandsClientGeneric.h"

#include "Agent/Commands/PresentationContinuityProbe.h"
#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"
#include "Ui/WrapperBase.h"

#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

static std::filesystem::path PathFromParameter(const nlohmann::json& rValue)
{
	// Agent-supplied path is UTF-8; .get<std::string>() throws on a non-string.
	std::string utf8 = rValue.get<std::string>();
	return std::filesystem::path(reinterpret_cast<const char8_t*>(utf8.c_str()));
}

static int64_t FrameCountParameter(const nlohmann::json& rParameters, std::string_view command, const char* pcParameter, int64_t iDefault, int64_t iMax)
{
	if (!rParameters.contains(pcParameter))
	{
		return iDefault;
	}

	const nlohmann::json& rFrames = rParameters.at(pcParameter);
	std::string integerError = std::string(command) + " '" + pcParameter + "' must be an integer";
	if (!rFrames.is_number_integer())
	{
		throw std::runtime_error(integerError);
	}

	if (rFrames.is_number_unsigned())
	{
		uint64_t uiFrames = rFrames.get<uint64_t>();
		if (uiFrames > static_cast<uint64_t>(iMax))
		{
			throw std::runtime_error(integerError + " in [0," + std::to_string(iMax) + "]");
		}
		return static_cast<int64_t>(uiFrames);
	}

	int64_t iFrames = rFrames.get<int64_t>();
	if (iFrames < 0 || iFrames > iMax)
	{
		throw std::runtime_error(integerError + " in [0," + std::to_string(iMax) + "]");
	}
	return static_cast<int64_t>(iFrames);
}

enum class CaptureCommandPhase : uint8_t
{
	kAwaitRestore,
	kAwaitResult,
	kAwaitMinimize,
	kDone,
};

struct CaptureCommandState
{
	~CaptureCommandState()
	{
		// Deferred-response timeout/discard does not poll again. Restore the original minimized state when the
		// state object is released so every failure path cleans up before AgentCommandServer publishes or exits.
		if (bRestoreMinimized && ePhase != CaptureCommandPhase::kDone && IsWindow(windowHandle) != FALSE && IsIconic(windowHandle) == FALSE)
		{
			ShowWindow(windowHandle, SW_SHOWMINNOACTIVE);
		}
	}

	HWND windowHandle = nullptr;
	CaptureCommandPhase ePhase = CaptureCommandPhase::kAwaitResult;
	bool bRestoreMinimized = false;
	int64_t iCaptureToken = 0;
	std::optional<nlohmann::json> result;
};

enum class RenderDocCapturePhase : uint8_t
{
	kAwaitRestore,
	kAwaitCaptures,
	kAwaitMinimize,
	kDone,
};

struct RenderDocCaptureState
{
	~RenderDocCaptureState()
	{
		// Mirror CaptureCommandState: the deferred-response timeout/discard does not poll again, so restore the
		// original minimized state on release, cleaning up before AgentCommandServer publishes or exits.
		if (bRestoreMinimized && ePhase != RenderDocCapturePhase::kDone && IsWindow(windowHandle) != FALSE && IsIconic(windowHandle) == FALSE)
		{
			ShowWindow(windowHandle, SW_SHOWMINNOACTIVE);
		}
	}

	HWND windowHandle = nullptr;
	RenderDocCapturePhase ePhase = RenderDocCapturePhase::kAwaitCaptures;
	bool bRestoreMinimized = false;
	uint32_t uiBaselineCaptures = 0;
	int64_t iFrames = 1;
};

template <typename QUEUE_CAPTURE>
static void BeginCaptureAndDefer(QUEUE_CAPTURE QueueCapture)
{
	HWND windowHandle = engine::gpGraphics->mWindowHandle;
	bool bRestoreMinimized = IsIconic(windowHandle) != FALSE;
	if (!bRestoreMinimized && engine::gpGraphics->mbSwapchainRecreateDeferred)
	{
		// A non-minimized window with deferred swapchain recreation has no live capture target.
		throw std::runtime_error("window is minimized or swapchain recreate is deferred");
	}

	std::unique_ptr<CaptureCommandState> pState = std::make_unique<CaptureCommandState>();
	pState->windowHandle = windowHandle;
	pState->bRestoreMinimized = bRestoreMinimized;
	if (bRestoreMinimized)
	{
		pState->ePhase = CaptureCommandPhase::kAwaitRestore;
		ShowWindow(windowHandle, SW_SHOWNOACTIVATE);
	}
	else
	{
		pState->iCaptureToken = engine::ResetCaptureResult();
		QueueCapture(pState->iCaptureToken);
	}

	engine::gpAgentCommandServer->DeferResponse([pState = std::move(pState), QueueCapture = std::move(QueueCapture)]() mutable -> std::optional<nlohmann::json>
	{
		if (pState->ePhase == CaptureCommandPhase::kAwaitRestore)
		{
			if (IsIconic(pState->windowHandle) == FALSE && engine::gpGraphics->ExtentSettled())
			{
				pState->iCaptureToken = engine::ResetCaptureResult();
				QueueCapture(pState->iCaptureToken);
				pState->ePhase = CaptureCommandPhase::kAwaitResult;
			}
		}

		if (pState->ePhase == CaptureCommandPhase::kAwaitResult)
		{
			pState->result = engine::TakeCaptureResult(pState->iCaptureToken);
			if (pState->result.has_value())
			{
				if (!pState->bRestoreMinimized)
				{
					pState->ePhase = CaptureCommandPhase::kDone;
				}
				else
				{
					ShowWindow(pState->windowHandle, SW_SHOWMINNOACTIVE);
					pState->ePhase = CaptureCommandPhase::kAwaitMinimize;
				}
			}
		}

		if (pState->ePhase == CaptureCommandPhase::kAwaitMinimize)
		{
			if (IsIconic(pState->windowHandle) == FALSE)
			{
				return std::nullopt;
			}
			pState->ePhase = CaptureCommandPhase::kDone;
		}

		if (pState->ePhase != CaptureCommandPhase::kDone)
		{
			return std::nullopt;
		}

		nlohmann::json result = std::move(*pState->result);
		pState->result.reset();
		// A failed async save publishes an {"error":...} result; rethrow it so Drain's poll-exception path emits a
		// proper {"ok":false,"error"} envelope instead of wrapping the error JSON as ok:true.
		if (result.contains("error"))
		{
			throw std::runtime_error(result.at("error").get<std::string>());
		}
		return result;
	});
}

// screenshot: capture the live window to a downscaled JPG/PNG. Completes via the deferred-response mechanism once
// the async save records its result. Schema: {"path"?,"maxWidth"?:1568,"format"?:"jpg|png","quality"?:80}.
static void CommandScreenshot(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	// The capture consume site (RenderMainPresentAcquire) is compiled out under !kbScreenshots, so a deferred request
	// would never resolve — fail fast instead of blocking the agent channel until the liveness timeout.
	if constexpr (!kbScreenshots)
	{
		throw std::runtime_error("screenshot capture is compiled out (kbScreenshots is false)");
	}

	// BeginCaptureAndDefer temporarily restores an iconic window without activation, waits for its live swapchain,
	// then returns it to the minimized state after the capture result arrives.
	engine::ScreenshotRequest request;
	request.bPublishResult = true;
	if (rParameters.contains("path"))
	{
		request.path = PathFromParameter(rParameters.at("path"));
	}
	if (rParameters.contains("maxWidth"))
	{
		request.iMaxWidth = rParameters.at("maxWidth").get<int64_t>();
	}
	if (rParameters.contains("format"))
	{
		std::string format = rParameters.at("format").get<std::string>();
		if (format == "png")
		{
			request.bPng = true;
		}
		else if (format == "jpg" || format == "jpeg")
		{
			request.bPng = false;
		}
		else
		{
			throw std::runtime_error("screenshot 'format' must be 'jpg' or 'png'");
		}
	}
	if (rParameters.contains("quality"))
	{
		int64_t iQuality = rParameters.at("quality").get<int64_t>();
		if (iQuality < 1 || iQuality > 100)
		{
			throw std::runtime_error("screenshot 'quality' must be an integer in [1,100]");
		}
		request.iQuality = iQuality;
	}

	BeginCaptureAndDefer([request = std::move(request)](int64_t iCaptureToken) mutable
	{
		request.iCaptureToken = iCaptureToken;
		engine::gpGraphics->mScreenshotRequest = std::move(request);
	});
}

// renderdoc_capture: trigger RenderDoc capture(s) of the upcoming presented frame(s) and return the .rdc path(s).
// Requires a --renderdoc launch (mpRenderDocApi non-null). Like BeginCaptureAndDefer, an iconic window is temporarily
// restored for the live present, then re-minimized after RenderDoc serializes every capture. Completes via the
// deferred-response mechanism once GetNumCaptures() reaches the pre-trigger baseline plus the requested frame count.
// Schema: {"frames"?:1 (1..8)} -> {"paths":[absolute .rdc paths]}.
static void CommandRenderDocCapture(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	RENDERDOC_API_1_6_0* pRenderDocApi = engine::gpInstanceManager->mpRenderDocApi;
	if (pRenderDocApi == nullptr)
	{
		throw std::runtime_error("RenderDoc API not available (launch the client with --renderdoc)");
	}

	int64_t iFrames = 1;
	if (rParameters.contains("frames"))
	{
		if (!rParameters.at("frames").is_number_integer())
		{
			throw std::runtime_error("renderdoc_capture 'frames' must be an integer in [1,8]");
		}
		iFrames = rParameters.at("frames").get<int64_t>();
		if (iFrames < 1 || iFrames > 8)
		{
			throw std::runtime_error("renderdoc_capture 'frames' must be an integer in [1,8]");
		}
	}

	HWND windowHandle = engine::gpGraphics->mWindowHandle;
	bool bRestoreMinimized = IsIconic(windowHandle) != FALSE;
	if (!bRestoreMinimized && engine::gpGraphics->mbSwapchainRecreateDeferred)
	{
		// Mirror BeginCaptureAndDefer's fast-fail: a deferred swapchain recreate skips present, so a trigger on a
		// non-minimized window with no live target would only ever time out.
		throw std::runtime_error("window is minimized or swapchain recreate is deferred");
	}

	std::unique_ptr<RenderDocCaptureState> pState = std::make_unique<RenderDocCaptureState>();
	pState->windowHandle = windowHandle;
	pState->bRestoreMinimized = bRestoreMinimized;
	pState->iFrames = iFrames;
	if (bRestoreMinimized)
	{
		pState->ePhase = RenderDocCapturePhase::kAwaitRestore;
		ShowWindow(windowHandle, SW_SHOWNOACTIVATE);
	}
	else
	{
		pState->uiBaselineCaptures = pRenderDocApi->GetNumCaptures();
		if (iFrames == 1)
		{
			pRenderDocApi->TriggerCapture();
		}
		else
		{
			pRenderDocApi->TriggerMultiFrameCapture(static_cast<uint32_t>(iFrames));
		}
	}

	engine::gpAgentCommandServer->DeferResponse([pState = std::move(pState), pRenderDocApi]() -> std::optional<nlohmann::json>
	{
		if (pState->ePhase == RenderDocCapturePhase::kAwaitRestore)
		{
			if (IsIconic(pState->windowHandle) == FALSE && engine::gpGraphics->ExtentSettled())
			{
				pState->uiBaselineCaptures = pRenderDocApi->GetNumCaptures();
				if (pState->iFrames == 1)
				{
					pRenderDocApi->TriggerCapture();
				}
				else
				{
					pRenderDocApi->TriggerMultiFrameCapture(static_cast<uint32_t>(pState->iFrames));
				}
				pState->ePhase = RenderDocCapturePhase::kAwaitCaptures;
			}
		}

		if (pState->ePhase == RenderDocCapturePhase::kAwaitCaptures)
		{
			// RenderDoc increments GetNumCaptures() only after a capture is fully serialized; the Drain liveness
			// timeout bounds a capture that never lands (the dtor re-minimizes on that path).
			if (pRenderDocApi->GetNumCaptures() < pState->uiBaselineCaptures + static_cast<uint32_t>(pState->iFrames))
			{
				return std::nullopt;
			}
			if (pState->bRestoreMinimized)
			{
				ShowWindow(pState->windowHandle, SW_SHOWMINNOACTIVE);
				pState->ePhase = RenderDocCapturePhase::kAwaitMinimize;
			}
			else
			{
				pState->ePhase = RenderDocCapturePhase::kDone;
			}
		}

		if (pState->ePhase == RenderDocCapturePhase::kAwaitMinimize)
		{
			if (IsIconic(pState->windowHandle) == FALSE)
			{
				return std::nullopt;
			}
			pState->ePhase = RenderDocCapturePhase::kDone;
		}

		if (pState->ePhase != RenderDocCapturePhase::kDone)
		{
			return std::nullopt;
		}

		nlohmann::json paths = nlohmann::json::array();
		for (int64_t i = pState->uiBaselineCaptures; i < pState->uiBaselineCaptures + static_cast<uint32_t>(pState->iFrames); ++i)
		{
			// Two-call GetCapture: first with a null buffer to size the path (length includes the NUL), then read it.
			uint32_t uiPathLength = 0;
			if (pRenderDocApi->GetCapture(static_cast<uint32_t>(i), nullptr, &uiPathLength, nullptr) == 0)
			{
				throw std::runtime_error("renderdoc_capture: capture index unavailable");
			}
			std::string capturePath(uiPathLength, '\0');
			pRenderDocApi->GetCapture(static_cast<uint32_t>(i), capturePath.data(), &uiPathLength, nullptr);
			capturePath.resize(uiPathLength > 0 ? uiPathLength - 1 : 0);

			std::u8string capturePathUtf8(capturePath.begin(), capturePath.end());
			if (!std::filesystem::exists(std::filesystem::path(capturePathUtf8)))
			{
				throw std::runtime_error("renderdoc_capture: capture file missing");
			}
			paths.push_back(std::move(capturePath));
		}

		nlohmann::json result;
		result["paths"] = std::move(paths);
		return result;
	});
}

// resize: change the live client window/framebuffer size mid-session by driving the real HWND resize
// (SetWindowPos -> synchronous WM_SIZE -> gVkWantedFramebufferExtent2D -> swapchain recreate). Client dims are
// validated to [320x180, 16384x16384] then rounded up to a multiple of 8 (mirrors SetupWindow).
// The swapchain additionally clamps to surface caps, so the applied extent may differ from the requested one.
// Completes synchronously if already at the requested extent, else via the deferred-response mechanism once the
// swapchain has recreated. Never mutates the persisted gFullscreen setting. Schema: {"width","height"}.
static void CommandResize(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("width") || !rParameters.at("width").is_number() || !rParameters.contains("height") || !rParameters.at("height").is_number())
	{
		throw std::runtime_error("resize requires numeric 'width' and 'height'");
	}

	int64_t iWidth = rParameters.at("width").get<int64_t>();
	int64_t iHeight = rParameters.at("height").get<int64_t>();
	if (iWidth < 320 || iWidth > 16'384 || iHeight < 180 || iHeight > 16'384)
	{
		throw std::runtime_error("resize 'width'/'height' out of bounds [320x180, 16384x16384]");
	}

	// Multiple-of-8 rounding for reproducible geometry (matches SetupWindow's client-size rounding).
	int64_t iClientWidth = common::RoundUp<int64_t, 8>(iWidth);
	int64_t iClientHeight = common::RoundUp<int64_t, 8>(iHeight);

	// Reject while minimized: WM_SIZE never fires for a minimized window, so the deferred poll can't converge.
	if (IsIconic(engine::gpGraphics->mWindowHandle))
	{
		throw std::runtime_error("window is minimized");
	}

	// Style-aware client -> outer conversion: WS_OVERLAPPEDWINDOW grows the client rect by the frame via
	// AdjustWindowRect; WS_POPUP (borderless windowed-fullscreen) has no frame, so client size is the outer size.
	HWND windowHandle = engine::gpGraphics->mWindowHandle;
	int64_t iStyle = GetWindowLongPtr(windowHandle, GWL_STYLE);
	RECT outerRect {.left = 0, .top = 0, .right = static_cast<LONG>(iClientWidth), .bottom = static_cast<LONG>(iClientHeight)};
	if ((iStyle & WS_OVERLAPPEDWINDOW) != 0)
	{
		AdjustWindowRect(&outerRect, static_cast<DWORD>(iStyle), FALSE);
	}
	int64_t iOuterWidth = outerRect.right - outerRect.left;
	int64_t iOuterHeight = outerRect.bottom - outerRect.top;

	// On-screen clamping: a fixed top-left with a growing size can push the bottom/right edges off screen. Query
	// the window's current monitor (mirrors SetupWindow) and keep the outer rect fully within rcMonitor.
	RECT currentRect {};
	GetWindowRect(windowHandle, &currentRect);
	int64_t iPositionX = currentRect.left;
	int64_t iPositionY = currentRect.top;

	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST), &monitorInfo);
	const RECT& rMonitor = monitorInfo.rcMonitor;

	if (iClientWidth == rMonitor.right - rMonitor.left && iClientHeight == rMonitor.bottom - rMonitor.top)
	{
		// Rounded client size exactly the monitor's pixel size: land at the monitor origin (pixel-accurate).
		iPositionX = rMonitor.left;
		iPositionY = rMonitor.top;
	}
	else
	{
		// Otherwise keep the current position but shift left/up so the outer rect stays fully on the monitor;
		// if the window is larger than the monitor in a dimension, pin that axis to the monitor origin.
		if (iPositionX + iOuterWidth > rMonitor.right)
		{
			iPositionX = rMonitor.right - iOuterWidth;
		}
		if (iPositionX < rMonitor.left)
		{
			iPositionX = rMonitor.left;
		}
		if (iPositionY + iOuterHeight > rMonitor.bottom)
		{
			iPositionY = rMonitor.bottom - iOuterHeight;
		}
		if (iPositionY < rMonitor.top)
		{
			iPositionY = rMonitor.top;
		}
	}

	// Drain() runs on the WindowProcedure thread, so this SetWindowPos's WM_SIZE fires synchronously and writes
	// gVkWantedFramebufferExtent2D exactly as a human drag does. No z-order / activation change.
	SetWindowPos(windowHandle, nullptr, static_cast<int>(iPositionX), static_cast<int>(iPositionY), static_cast<int>(iOuterWidth), static_cast<int>(iOuterHeight), SWP_NOZORDER | SWP_NOACTIVATE);

	// Fast path: if the swapchain already sits at the requested (rounded) extent, answer synchronously.
	if (engine::gpGraphics->mFramebufferVkExtent2D.width == static_cast<uint32_t>(iClientWidth) && engine::gpGraphics->mFramebufferVkExtent2D.height == static_cast<uint32_t>(iClientHeight))
	{
		rResult["width"] = engine::gpGraphics->mFramebufferVkExtent2D.width;
		rResult["height"] = engine::gpGraphics->mFramebufferVkExtent2D.height;
		return;
	}

	engine::gpAgentCommandServer->DeferResponse([]() -> std::optional<nlohmann::json>
	{
		// Graphics::Refresh copies gVkWantedFramebufferExtent2D into mFramebufferVkExtent2D when it triggers the recreate
		// (Graphics.cpp), and CreateSwapchain's defined branch may then clamp gVkWantedFramebufferExtent2D down to the
		// surface-cap currentExtent (SwapchainManager.cpp), so the two converge only once the applied extent settles.
		// ExtentSettled() also requires no recreate be deferred — extent equality alone is satisfied the moment Refresh
		// copies the wanted extent, before the defer gate returns, which would report a false success no live swapchain
		// backs. Poll until settled, then report the applied extent (may differ from the requested one after clamping).
		if (!engine::gpGraphics->ExtentSettled())
		{
			return std::nullopt;
		}
		nlohmann::json result;
		result["width"] = engine::gpGraphics->mFramebufferVkExtent2D.width;
		result["height"] = engine::gpGraphics->mFramebufferVkExtent2D.height;
		return result;
	});
}

// fullscreen: toggle the live client between borderless windowed-fullscreen (WS_POPUP) and windowed
// (WS_OVERLAPPEDWINDOW) mid-session by driving the engine's WantedFullscreen() -> main-loop reconciliation style-switch path
// via an agent override. 'on' is validated to a bool. Idempotent — an already-in-state request
// answers synchronously. Never mutates the persisted gFullscreen setting; windowed restore returns to the launch
// extent (a mid-run resize is not preserved). Schema: {"on"}; result: {"fullscreen","width","height"}.
static void CommandFullscreen(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("on") || !rParameters.at("on").is_boolean())
	{
		throw std::runtime_error("fullscreen requires boolean 'on'");
	}

	bool bOn = rParameters.at("on").get<bool>();

	// Reject while minimized: the style toggle drives a swapchain recreate that can't converge off-screen, so the
	// deferred poll would never settle (mirrors resize).
	if (IsIconic(engine::gpGraphics->mWindowHandle))
	{
		throw std::runtime_error("window is minimized");
	}

	// Live style read: WS_POPUP set == borderless windowed-fullscreen; WS_OVERLAPPEDWINDOW == windowed.
	HWND windowHandle = engine::gpGraphics->mWindowHandle;
	bool bIsFullscreen = (GetWindowLongPtr(windowHandle, GWL_STYLE) & WS_POPUP) != 0;

	// Fast path: already in the requested mode — report the current extent synchronously (idempotence).
	if (bIsFullscreen == bOn)
	{
		rResult["fullscreen"] = bIsFullscreen;
		rResult["width"] = engine::gpGraphics->mFramebufferVkExtent2D.width;
		rResult["height"] = engine::gpGraphics->mFramebufferVkExtent2D.height;
		return;
	}

	// Install the override; the per-frame main-loop reconciliation (in MainThread, just after ProcessMessages()) picks it
	// up and performs the style swap + SetupWindow + swapchain recreate. The command completes via the deferred-response mechanism once it lands.
	engine::gAgentFullscreenOverride = bOn;

	engine::gpAgentCommandServer->DeferResponse([bOn]() -> std::optional<nlohmann::json>
	{
		// Poll until BOTH the live WS_POPUP bit matches the request AND the extent has settled. ExtentSettled() folds
		// extent equality with !mbSwapchainRecreateDeferred, so it can't report a false success while a recreate is
		// still deferred (the style flips a frame before the swapchain finishes). Then report the applied state.
		bool bNowFullscreen = (GetWindowLongPtr(engine::gpGraphics->mWindowHandle, GWL_STYLE) & WS_POPUP) != 0;
		if (bNowFullscreen != bOn || !engine::gpGraphics->ExtentSettled())
		{
			return std::nullopt;
		}
		nlohmann::json result;
		result["fullscreen"] = bNowFullscreen;
		result["width"] = engine::gpGraphics->mFramebufferVkExtent2D.width;
		result["height"] = engine::gpGraphics->mFramebufferVkExtent2D.height;
		return result;
	});
}

// window_state: minimize the live client window or restore it mid-session by driving ShowWindow on the real HWND.
// 'minimized' is validated to a bool. Idempotent — an already-in-state request answers synchronously.
// Minimize uses SW_MINIMIZE; restore uses SW_SHOWNOACTIVATE (a no-activate restore — never steal foreground focus, per
// the agent-mode convention in Main.cpp). Never mutates the persisted gFullscreen setting or any .bin. This command
// produces the minimized/recreate-deferred state that resize/fullscreen reject; captures temporarily restore it. Completes
// synchronously if already in state, else via the deferred-response mechanism once the window state settles. Schema:
// {"minimized"}; result: {"minimized"}, plus {"width","height"} of the settled extent on restore.
static void CommandWindowState(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("minimized") || !rParameters.at("minimized").is_boolean())
	{
		throw std::runtime_error("window_state requires boolean 'minimized'");
	}

	bool bMinimized = rParameters.at("minimized").get<bool>();

	HWND windowHandle = engine::gpGraphics->mWindowHandle;

	// Fast path: already in the requested state — report it synchronously (idempotence), including the current extent
	// on restore (matches resize/fullscreen).
	if ((IsIconic(windowHandle) != FALSE) == bMinimized)
	{
		rResult["minimized"] = bMinimized;
		if (!bMinimized)
		{
			rResult["width"] = engine::gpGraphics->mFramebufferVkExtent2D.width;
			rResult["height"] = engine::gpGraphics->mFramebufferVkExtent2D.height;
		}
		return;
	}

	if (bMinimized)
	{
		// Minimize, then poll until the window reports iconic.
		ShowWindow(windowHandle, SW_MINIMIZE);
		engine::gpAgentCommandServer->DeferResponse([]() -> std::optional<nlohmann::json>
		{
			if (IsIconic(engine::gpGraphics->mWindowHandle) == FALSE)
			{
				return std::nullopt;
			}
			nlohmann::json result;
			result["minimized"] = true;
			return result;
		});
		return;
	}

	// No-activate restore (never steal foreground focus, per Main.cpp's agent-mode convention). Poll until the window
	// is no longer iconic AND the extent has settled — a bare !IsIconic restore would report success before the deferred
	// swapchain recreate lands, the false-success class the sibling polls already guard against.
	ShowWindow(windowHandle, SW_SHOWNOACTIVATE);
	engine::gpAgentCommandServer->DeferResponse([]() -> std::optional<nlohmann::json>
	{
		if (IsIconic(engine::gpGraphics->mWindowHandle) != FALSE || !engine::gpGraphics->ExtentSettled())
		{
			return std::nullopt;
		}
		nlohmann::json result;
		result["minimized"] = false;
		result["width"] = engine::gpGraphics->mFramebufferVkExtent2D.width;
		result["height"] = engine::gpGraphics->mFramebufferVkExtent2D.height;
		return result;
	});
}

// audio_resume: resume client audio, which an agent launch boots suspended (Main.cpp) and which only a real OS focus
// gain would otherwise resume — something the harness never produces. Runs on the client main thread via Drain(), the
// thread AudioManager::Update runs on. Repeat calls re-apply the same state. No parameters; no result fields.
static void CommandAudioResume([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	engine::gpAudioManager->Resume();
}

// dump_render_target: read back an offscreen render target and encode it (normalized grayscale PNG for
// single-channel, direct PNG for 4x8-bit, optional raw .bin). Schema: {"name","index"?:0,"channel"?:0,"path"?,"raw"?:false}.
static void CommandDumpRenderTarget(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	// As CommandScreenshot: the readback consume site is compiled out under !kbScreenshots, so fail fast.
	if constexpr (!kbScreenshots)
	{
		throw std::runtime_error("dump_render_target is compiled out (kbScreenshots is false)");
	}

	// As CommandScreenshot: BeginCaptureAndDefer temporarily restores an iconic window for the live readback.
	if (!rParameters.contains("name") || !rParameters.at("name").is_string())
	{
		throw std::runtime_error("dump_render_target requires string 'name'");
	}

	engine::DumpRenderTargetRequest request;
	request.bPublishResult = true;
	request.name = rParameters.at("name").get<std::string>();
	if (rParameters.contains("index"))
	{
		request.iIndex = rParameters.at("index").get<int64_t>();
	}
	if (rParameters.contains("channel"))
	{
		request.iChannel = rParameters.at("channel").get<int64_t>();
	}
	if (rParameters.contains("path"))
	{
		request.path = PathFromParameter(rParameters.at("path"));
	}
	if (rParameters.contains("raw"))
	{
		request.bRaw = rParameters.at("raw").get<bool>();
	}

	// Validate now (unknown name / bad index / non-encodable format) so errors report synchronously.
	engine::ValidateDumpRenderTargetRequest(request);

	BeginCaptureAndDefer([request = std::move(request)](int64_t iCaptureToken) mutable
	{
		request.iCaptureToken = iCaptureToken;
		engine::gpGraphics->mDumpRenderTargetRequest = std::move(request);
	});
}

// Full structured dump of the last completed ImGui frame (registry read table) plus game UI state.
static nlohmann::json BuildDescribeUi()
{
	nlohmann::json result;
	result["uiState"] = UiStateName(game::gpGame->meUiState);
	result["tweaksVisible"] = game::gpGame->mbShowImGui;
	result["gameFlags"] = GameFlagNames(game::gpGame->mGameFlags);

	result["framebuffer"] = {engine::gpGraphics->mFramebufferVkExtent2D.width, engine::gpGraphics->mFramebufferVkExtent2D.height};

	ImVec2 mousePosition = (ImGui::GetCurrentContext() != nullptr) ? ImGui::GetIO().MousePos : ImVec2(0.0f, 0.0f);
	result["mouse"] = {mousePosition.x, mousePosition.y};

	nlohmann::json windows = nlohmann::json::array();
	nlohmann::json items = nlohmann::json::array();
	if (engine::gpAgentUiRegistry != nullptr)
	{
		for (int64_t i = 0; i < engine::gpAgentUiRegistry->miWindowCount[engine::gpAgentUiRegistry->miRead]; ++i)
		{
			const engine::AgentUiWindow& rWindow = engine::gpAgentUiRegistry->mWindows[engine::gpAgentUiRegistry->miRead][i];
			windows.push_back({{"name", rWindow.pcName}, {"rect", {rWindow.f4Rectangle.x, rWindow.f4Rectangle.y, rWindow.f4Rectangle.z, rWindow.f4Rectangle.w}}, {"focused", rWindow.bFocused}});
		}
		for (int64_t i = 0; i < engine::gpAgentUiRegistry->miItemCount[engine::gpAgentUiRegistry->miRead]; ++i)
		{
			const engine::AgentUiItem& rItem = engine::gpAgentUiRegistry->mItems[engine::gpAgentUiRegistry->miRead][i];
			if (rItem.pcLabel[0] == '\0')
			{
				continue; // no label recorded (invisible / no ItemInfo) — not addressable, omit
			}
			nlohmann::json item =
			{
				{"label", rItem.pcLabel},
				{"window", rItem.pcWindow},
				{"rect", {rItem.f4Rectangle.x, rItem.f4Rectangle.y, rItem.f4Rectangle.z, rItem.f4Rectangle.w}},
				{"disabled", rItem.bDisabled},
				{"checked", (rItem.iStatusFlags & ImGuiItemStatusFlags_Checked) != 0},
				{"inputable", (rItem.iStatusFlags & ImGuiItemStatusFlags_Inputable) != 0},
				{"hovered", (rItem.iStatusFlags & ImGuiItemStatusFlags_HoveredRect) != 0},
				{"visible", (rItem.iStatusFlags & ImGuiItemStatusFlags_Visible) != 0},
			};
			if (rItem.pcValue[0] != '\0')
			{
				item["value"] = rItem.pcValue;
			}
			items.push_back(std::move(item));
		}
	}
	result["windows"] = std::move(windows);
	result["items"] = std::move(items);
	return result;
}

// Comma-joined list of candidate labels (optionally filtered to one window) for a not-found / ambiguous error.
static std::string CandidateLabels(const char* pcWindow)
{
	std::string candidates;
	if (engine::gpAgentUiRegistry == nullptr)
	{
		return candidates;
	}
	for (int64_t i = 0; i < engine::gpAgentUiRegistry->miItemCount[engine::gpAgentUiRegistry->miRead]; ++i)
	{
		const engine::AgentUiItem& rItem = engine::gpAgentUiRegistry->mItems[engine::gpAgentUiRegistry->miRead][i];
		if (rItem.pcLabel[0] == '\0')
		{
			continue;
		}
		if (pcWindow != nullptr && std::strcmp(rItem.pcWindow, pcWindow) != 0)
		{
			continue;
		}
		if (!candidates.empty())
		{
			candidates += ", ";
		}
		candidates += rItem.pcLabel;
		if (candidates.size() > 1'024) // bound the error message
		{
			candidates += ", ...";
			break;
		}
	}
	return candidates;
}

// Win32 VK code for a named key (letters/digits directly; a small symbolic table for the game bindings).
static int64_t ParseVirtualKey(std::string_view name)
{
	if (name.size() == 1)
	{
		char cChar = name[0];
		if (cChar >= 'a' && cChar <= 'z')
		{
			cChar = static_cast<char>(cChar - 'a' + 'A');
		}
		if ((cChar >= 'A' && cChar <= 'Z') || (cChar >= '0' && cChar <= '9'))
		{
			return static_cast<int64_t>(static_cast<unsigned char>(cChar));
		}
	}
	if (name == "ESC" || name == "ESCAPE")
	{
		return VK_ESCAPE;
	}
	if (name == "SPACE")
	{
		return VK_SPACE;
	}
	if (name == "TAB")
	{
		return VK_TAB;
	}
	if (name == "ENTER" || name == "RETURN")
	{
		return VK_RETURN;
	}
	if (name == "UP")
	{
		return VK_UP;
	}
	if (name == "DOWN")
	{
		return VK_DOWN;
	}
	if (name == "LEFT")
	{
		return VK_LEFT;
	}
	if (name == "RIGHT")
	{
		return VK_RIGHT;
	}
	if (name.size() >= 2 && (name[0] == 'F' || name[0] == 'f'))
	{
		int64_t iNumber = std::atoi(std::string(name.substr(1)).c_str());
		if (iNumber >= 1 && iNumber <= 24)
		{
			return VK_F1 + (iNumber - 1);
		}
	}
	throw std::runtime_error("unknown key");
}

// Fill a bounded char buffer from a string param (.get<std::string>() throws on non-string).
static void CopyStringParameter(std::span<char> destination, std::string_view source)
{
	int64_t i = 0;
	for (; i < static_cast<int64_t>(destination.size()) - 1 && i < static_cast<int64_t>(source.size()); ++i)
	{
		destination[i] = source[i];
	}
	destination[i] = '\0';
}

// Shared label-target parse for click / hover / set_slider.
static void FillLabelTarget(const nlohmann::json& rParameters, engine::AgentScript& rScript)
{
	if (!rParameters.contains("label") || !rParameters.at("label").is_string())
	{
		throw std::runtime_error("command requires string 'label'");
	}
	CopyStringParameter(rScript.pcLabel, rParameters.at("label").get<std::string>());
	if (rParameters.contains("window"))
	{
		CopyStringParameter(rScript.pcWindow, rParameters.at("window").get<std::string>());
		rScript.bHasWindow = true;
	}
}

// Begin a script (throwing "busy" if one is already running) and defer the response until the script completes.
// For label-based scripts, a not-found / ambiguous error's candidate list is limited to errorWindow only when bHasWindow is set.
static void BeginScriptAndDefer(const engine::AgentScript& rScript, bool bDescribeUiAfter, bool bLabelBased, bool bHasWindow, std::string errorWindow)
{
	if (!engine::gpAgentInput->BeginScript(rScript))
	{
		throw std::runtime_error("busy");
	}

	engine::gpAgentCommandServer->DeferResponse([bDescribeUiAfter, bLabelBased, bHasWindow, errorWindow = std::move(errorWindow)]() -> std::optional<nlohmann::json>
	{
		engine::AgentScriptStatus eStatus = engine::gpAgentInput->meStatus;
		if (eStatus == engine::AgentScriptStatus::kPending)
		{
			return std::nullopt;
		}
		if (eStatus == engine::AgentScriptStatus::kTimeout)
		{
			throw std::runtime_error("timed out");
		}
		if (eStatus == engine::AgentScriptStatus::kNotFound)
		{
			throw std::runtime_error("no widget matches label; candidates: " + CandidateLabels(bHasWindow ? errorWindow.c_str() : nullptr));
		}
		if (eStatus == engine::AgentScriptStatus::kAmbiguous)
		{
			throw std::runtime_error("ambiguous label; candidates: " + CandidateLabels(bHasWindow ? errorWindow.c_str() : nullptr));
		}
		if (eStatus == engine::AgentScriptStatus::kNotInputable)
		{
			throw std::runtime_error("target is not inputable");
		}
		if (eStatus == engine::AgentScriptStatus::kClipped)
		{
			throw std::runtime_error("target not visible (scrolled out of view)");
		}

		nlohmann::json result;
		if (bLabelBased)
		{
			result["found"] = true;
			result["enabled"] = !engine::gpAgentInput->mbResolvedDisabled;
		}
		else
		{
			result["ok"] = true;
		}
		if (bDescribeUiAfter)
		{
			result["ui"] = BuildDescribeUi();
		}
		return result;
	});
}

// Label-based scripts act only through ImGui and the published registry, which a non-rendering client never updates,
// so they would settle on a frozen snapshot and report success for input ImGui never received.
static void ThrowIfClientNotRendering()
{
	if (IsIconic(engine::gpGraphics->mWindowHandle) != FALSE || engine::gpGraphics->mbSwapchainRecreateDeferred)
	{
		throw std::runtime_error("client is not rendering (minimized or swapchain recreate deferred); restore with window_state {\"minimized\":false}");
	}
}

static void CommandDescribeUi([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	rResult = BuildDescribeUi();
}

// click {label, window?, timeoutFrames?=120, describeUiAfter?=true}: stabilize target rect, press/release the left
// mouse button at its center through ImGui IO, then optionally dump the post-click UI.
static void CommandClick(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	engine::AgentScript script;
	script.eKind = engine::AgentScriptKind::kClick;
	FillLabelTarget(rParameters, script);
	// Latest stabilization lands on advance timeoutFrames - 1; press, release, and settle add 4 more advances.
	script.iTimeoutFrames = FrameCountParameter(rParameters, "click", "timeoutFrames", script.iTimeoutFrames, engine::AgentCommandServer::kiDeferredTimeoutDrains - 3);
	bool bDescribeUiAfter = !rParameters.contains("describeUiAfter") || rParameters.at("describeUiAfter").get<bool>();
	ThrowIfClientNotRendering();
	BeginScriptAndDefer(script, bDescribeUiAfter, true, script.bHasWindow, script.pcWindow);
}

// hover {label, window?, holdFrames?=2}: move to and stabilize on the target, hold, then dump the UI.
static void CommandHover(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	engine::AgentScript script;
	script.eKind = engine::AgentScriptKind::kHover;
	FillLabelTarget(rParameters, script);
	// Latest stabilization lands on advance iTimeoutFrames - 1, and the hold then finishes at least holdFrames advances later.
	script.iHoldFrames = FrameCountParameter(rParameters, "hover", "holdFrames", script.iHoldFrames, engine::AgentCommandServer::kiDeferredTimeoutDrains - script.iTimeoutFrames + 1);
	ThrowIfClientNotRendering();
	BeginScriptAndDefer(script, true, true, script.bHasWindow, script.pcWindow);
}

// set_slider {label, window?, value}: Ctrl+Click the slider to open ImGui temp-input, type the value, commit (Enter).
static void CommandSetSlider(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if (!rParameters.contains("value"))
	{
		throw std::runtime_error("set_slider requires 'value'");
	}
	engine::AgentScript script;
	script.eKind = engine::AgentScriptKind::kSetSlider;
	FillLabelTarget(rParameters, script);
	double fValue = rParameters.at("value").get<double>();
	std::snprintf(script.pcValueText, sizeof(script.pcValueText), "%g", fValue);
	ThrowIfClientNotRendering();
	BeginScriptAndDefer(script, false, true, script.bHasWindow, script.pcWindow);
}

// get_wrapper {key}: read a Tweaks-registered wrapper by its slider map key; set_slider is the write path.
static void CommandGetWrapper(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("key") || !rParameters.at("key").is_string())
	{
		throw std::runtime_error("get_wrapper requires string 'key'");
	}

	std::string key = rParameters.at("key").get<std::string>();
	const std::unordered_map<std::string_view, Wrapper*>& rSliderMap = TweaksSliderMap::Get();
	auto it = rSliderMap.find(key);
	if (it == rSliderMap.end())
	{
		throw std::runtime_error("get_wrapper: no Tweaks slider key '" + key + "'");
	}

	char pcText[32] {};
	std::snprintf(pcText, sizeof(pcText), "%.6f", it->second->mfCurrent);
	rResult["value"] = pcText;
	std::snprintf(pcText, sizeof(pcText), "%.6f", it->second->mfMin);
	rResult["min"] = pcText;
	std::snprintf(pcText, sizeof(pcText), "%.6f", it->second->mfMax);
	rResult["max"] = pcText;
}

// key {key, holdFrames?=1}: hold then release a named VK through the RawInput overlay, driving KeyboardPressed edges.
static void CommandKey(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if (!rParameters.contains("key") || !rParameters.at("key").is_string())
	{
		throw std::runtime_error("key requires string 'key'");
	}
	engine::AgentScript script;
	script.eKind = engine::AgentScriptKind::kKey;
	script.iVirtualKey = ParseVirtualKey(rParameters.at("key").get<std::string>());
	// The key script adds a press advance before the hold and a finish advance after it.
	script.iHoldFrames = FrameCountParameter(rParameters, "key", "holdFrames", 1, engine::AgentCommandServer::kiDeferredTimeoutDrains - 2);
	BeginScriptAndDefer(script, false, false, false, std::string());
}

// mouse {x, y, action:"move|down|up|click|wheel", button?="left", notches?}: raw pixel coords feeding both the ImGui
// IO sink and the RawInput overlay (normalized) for world clicks / unlabeled targets and camera-zoom wheel.
// (x/y optional for wheel — both or neither)
static void CommandMouse(const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	engine::AgentScript script;
	script.eKind = engine::AgentScriptKind::kMouse;

	std::string action = rParameters.contains("action") ? rParameters.at("action").get<std::string>() : "move";
	if (action == "move")
	{
		script.eMouseAction = engine::AgentMouseAction::kMove;
	}
	else if (action == "down")
	{
		script.eMouseAction = engine::AgentMouseAction::kDown;
	}
	else if (action == "up")
	{
		script.eMouseAction = engine::AgentMouseAction::kUp;
	}
	else if (action == "click")
	{
		script.eMouseAction = engine::AgentMouseAction::kClick;
	}
	else if (action == "wheel")
	{
		script.eMouseAction = engine::AgentMouseAction::kWheel;
	}
	else
	{
		throw std::runtime_error("mouse 'action' must be move|down|up|click|wheel");
	}

	if (script.eMouseAction == engine::AgentMouseAction::kWheel)
	{
		int64_t iNotches = 1;
		if (rParameters.contains("notches"))
		{
			const nlohmann::json& rNotches = rParameters.at("notches");
			bool bValid = rNotches.is_number_integer();
			if (bValid && rNotches.is_number_unsigned())
			{
				bValid = std::in_range<int64_t>(rNotches.get<uint64_t>());
			}
			if (!bValid)
			{
				throw std::runtime_error("mouse 'notches' must be an integer");
			}
			iNotches = rNotches.get<int64_t>();
		}
		// The default single notch is checked too: the lifetime total can sit within one notch of the int limit.
		if (!engine::gpAgentInput->WheelNotchesFit(iNotches))
		{
			throw std::runtime_error("mouse 'notches' wheel delta or lifetime wheel total would exceed int32");
		}
		script.iWheelNotches = iNotches;

		// Optional target coords: both present routes the ImGui wheel to the window under (x,y); neither supplies a new
		// ImGui target or preserves a pin from an earlier script. Camera zoom is suppressed when the hovered window can
		// actually scroll, ImGui's wheeling lock holds an earlier target, or a widget owns ImGuiKey_MouseWheelY (for
		// example, an ImPlot). ImPlot ownership remains visible for the first two input polls after leaving a plot; a
		// coordinate mouse move runs two active frames and, once complete, establishes its new target. A direct
		// coordinate-bearing background wheel immediately after plot hover may remain UI-owned; a subsequent notch after
		// that command completes reaches the camera.
		bool bHasX = rParameters.contains("x");
		bool bHasY = rParameters.contains("y");
		if (bHasX != bHasY)
		{
			throw std::runtime_error("mouse wheel requires both 'x' and 'y' or neither");
		}
		if (bHasX)
		{
			script.f2CoordinatePixels[0] = static_cast<float>(rParameters.at("x").get<double>());
			script.f2CoordinatePixels[1] = static_cast<float>(rParameters.at("y").get<double>());
			script.bHasCoordinate = true;
		}
	}
	else
	{
		if (!rParameters.contains("x") || !rParameters.contains("y"))
		{
			throw std::runtime_error("mouse requires 'x' and 'y'");
		}
		script.f2CoordinatePixels[0] = static_cast<float>(rParameters.at("x").get<double>());
		script.f2CoordinatePixels[1] = static_cast<float>(rParameters.at("y").get<double>());
		script.bHasCoordinate = true;

		std::string button = rParameters.contains("button") ? rParameters.at("button").get<std::string>() : "left";
		if (button == "left")
		{
			script.iImGuiMouseButton = 0;
			script.uiOverlayMouseButtonBit = std::to_underlying(engine::MouseButtons::kMouseButtonLeft);
		}
		else if (button == "right")
		{
			script.iImGuiMouseButton = 1;
			script.uiOverlayMouseButtonBit = std::to_underlying(engine::MouseButtons::kMouseButtonRight);
		}
		else if (button == "middle")
		{
			script.iImGuiMouseButton = 2;
			script.uiOverlayMouseButtonBit = std::to_underlying(engine::MouseButtons::kMouseButtonMiddle);
		}
		else
		{
			throw std::runtime_error("mouse 'button' must be left|right|middle");
		}
	}

	BeginScriptAndDefer(script, false, false, false, std::string());
}

static void CommandQueryProfile(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object() || !rParameters.empty())
	{
		throw std::runtime_error("query_profile requires empty params");
	}

	engine::GpuTimer* pGpuTimers = gpProfileManager->mGpuTimers;
	nlohmann::json gpuTimers = nlohmann::json::array();
	for (int64_t i = 0; i < engine::kGpuTimerCount; ++i)
	{
		engine::GpuTimer& rGpuTimer = pGpuTimers[i];
		nlohmann::json gpuTimer;
		gpuTimer["index"] = i;
		gpuTimer["name"] = std::string(engine::kGpuTimerNames[i]);
		gpuTimer["currentUs"] = rGpuTimer.smoothedMicroseconds.Current();
		gpuTimer["averageUs"] = rGpuTimer.smoothedMicroseconds.Average();
		gpuTimer["maxUs"] = rGpuTimer.smoothedMicroseconds.Maximum();
		gpuTimers.push_back(std::move(gpuTimer));
	}
	rResult["gpuTimers"] = std::move(gpuTimers);
	const engine::GpuShadowSample& rShadowSample = gpProfileManager->mGpuShadowSample;
	rResult["shadowSample"] =
	{
		{"sequence", rShadowSample.iSequence},
		{"currentUs", rShadowSample.iCurrentMicroseconds},
	};
	int64_t iClockOffset = gpProfileManager->mSmoothedClockOffset.mSmoothedValue;
	int64_t iClockTargetBehind = gpProfileManager->mSmoothedClockTarget.mSmoothedValue;
	int64_t iClockError = gpProfileManager->mSmoothedClockError.mSmoothedValue;
	rResult["clock"] =
	{
		{"offsetTicks", iClockOffset},
		{"targetBehindTicks", iClockTargetBehind},
		{"errorTicks", iClockError},
	};
	rResult["fps"] = engine::gpGraphics->mRendersInTheLastSecond.Get();

	nlohmann::json counters = nlohmann::json::array();
	for (int64_t i = 0; i < gpProfileManager->miCpuCounterCount; ++i)
	{
		engine::CpuCounter& rCounter = gpProfileManager->GetCpuCounter(i);
		nlohmann::json counter;
		counter["index"] = i;
		counter["name"] = std::string(gpProfileManager->GetCpuCounterName(i));
		counter["count"] = rCounter.iCount;
		counters.push_back(std::move(counter));
	}
	rResult["counters"] = std::move(counters);
}

const char* UiStateName(UiState eState)
{
	switch (eState)
	{
		case UiState::kNone: return "kNone";
		case UiState::kGameSettings: return "kGameSettings";
		case UiState::kGraphicsSettings: return "kGraphicsSettings";
		case UiState::kModal: return "kModal";
		case UiState::kPause: return "kPause";
		case UiState::kSound: return "kSound";
	}
	return "kNone";
}

nlohmann::json GameFlagNames(GameFlags_t flags)
{
	nlohmann::json names = nlohmann::json::array();
	if (flags & GameFlags::kQuit)
	{
		names.push_back("kQuit");
	}
	if (flags & GameFlags::kSaveReplay)
	{
		names.push_back("kSaveReplay");
	}
	if (flags & GameFlags::kLoadReplay)
	{
		names.push_back("kLoadReplay");
	}
	if (flags & GameFlags::kMainMenu)
	{
		names.push_back("kMainMenu");
	}
	if (flags & GameFlags::kPaused)
	{
		names.push_back("kPaused");
	}
	return names;
}

bool ExecuteClientAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "screenshot")
	{
		CommandScreenshot(rParameters, rResult);
		return true;
	}
	if (command == "renderdoc_capture")
	{
		CommandRenderDocCapture(rParameters, rResult);
		return true;
	}
	if (command == "resize")
	{
		CommandResize(rParameters, rResult);
		return true;
	}
	if (command == "fullscreen")
	{
		CommandFullscreen(rParameters, rResult);
		return true;
	}
	if (command == "window_state")
	{
		CommandWindowState(rParameters, rResult);
		return true;
	}
	if (command == "audio_resume")
	{
		CommandAudioResume(rParameters, rResult);
		return true;
	}
	if (command == "dump_render_target")
	{
		CommandDumpRenderTarget(rParameters, rResult);
		return true;
	}
	if (command == "describe_ui")
	{
		CommandDescribeUi(rParameters, rResult);
		return true;
	}
	if (command == "click")
	{
		CommandClick(rParameters, rResult);
		return true;
	}
	if (command == "hover")
	{
		CommandHover(rParameters, rResult);
		return true;
	}
	if (command == "set_slider")
	{
		CommandSetSlider(rParameters, rResult);
		return true;
	}
	if (command == "get_wrapper")
	{
		CommandGetWrapper(rParameters, rResult);
		return true;
	}
	if (command == "key")
	{
		CommandKey(rParameters, rResult);
		return true;
	}
	if (command == "mouse")
	{
		CommandMouse(rParameters, rResult);
		return true;
	}
	if (command == "query_profile")
	{
		CommandQueryProfile(rParameters, rResult);
		return true;
	}
	if (command == "presentation_continuity_probe")
	{
		CommandPresentationContinuityProbe(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace engine

#endif // defined(BT_CLIENT)
