#include "File/PackChunks.h"
#include "Graphics/Camera.h"
#include "Graphics/Screenshot.h"
#include "Memory/GlobalAllocator.h"
#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettings.h"
#include "CrashReport.h"
#include "Game.h"
#include "ModelViewerApplication.h"

namespace game
{

static bool sbQuit = false;
static bool sbComparisonHeld = false;
static bool sbScreenshotRequested = false;
static int64_t siScreenshotToken = 0;

static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uiMessage, WPARAM uiWordParameter, LPARAM iLongParameter)
{
	if (uiMessage == WM_APP + 1)
	{
		if (ImGui::GetCurrentContext() != nullptr)
		{
			ImGuiIO& rInputOutput = ImGui::GetIO();
			rInputOutput.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
			rInputOutput.AddMousePosEvent(static_cast<float>(static_cast<int16_t>(LOWORD(iLongParameter))), static_cast<float>(static_cast<int16_t>(HIWORD(iLongParameter))));
		}
		return 0;
	}
	if (uiMessage == WM_APP + 2)
	{
		if (ImGui::GetCurrentContext() != nullptr && (iLongParameter == 0 || iLongParameter == 1))
		{
			ImGuiIO& rInputOutput = ImGui::GetIO();
			rInputOutput.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
			rInputOutput.AddMouseButtonEvent(static_cast<int>(iLongParameter), std::cmp_not_equal(uiWordParameter, 0));
		}
		return 0;
	}
	if (ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler(hwnd, uiMessage, uiWordParameter, iLongParameter) != 0)
	{
		return TRUE;
	}
	switch (uiMessage)
	{
		case WM_KEYDOWN:
			if (uiWordParameter == VK_F4 && (GetKeyState(VK_SHIFT) & 0x8000) == 0 && (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_MENU) & 0x8000) == 0 && (GetKeyState(VK_LWIN) & 0x8000) == 0 && (GetKeyState(VK_RWIN) & 0x8000) == 0)
			{
				sbQuit = true;
				return 0;
			}
			if (uiWordParameter == 'Q')
			{
				sbComparisonHeld = true;
			}
			if ((uiWordParameter == 'W' || uiWordParameter == 'E' || uiWordParameter == 'R' || uiWordParameter == VK_BACK) && (iLongParameter & (1i64 << 30)) == 0 && gpModelViewerApplication != nullptr)
			{
				gpModelViewerApplication->UpdateDefaultComparison(sbComparisonHeld);
				if (uiWordParameter == VK_BACK)
				{
					gpModelViewerApplication->ResetLightingPreset();
				}
				else
				{
					gpModelViewerApplication->SelectLightingPreset(uiWordParameter == 'W' ? 0 : uiWordParameter == 'E' ? 1 : 2);
				}
				return 0;
			}
			if (uiWordParameter == VK_F9 && (iLongParameter & (1i64 << 30)) == 0)
			{
				if (sbScreenshotRequested || siScreenshotToken != 0)
				{
					LOG(kGraphics, kInfo, "ModelViewer screenshot busy token={}", siScreenshotToken);
				}
				else
				{
					sbScreenshotRequested = true;
				}
				return 0;
			}
			break;
		case WM_KEYUP:
			if (uiWordParameter == 'Q')
			{
				sbComparisonHeld = false;
			}
			break;
		case WM_KILLFOCUS:
			sbComparisonHeld = false;
			break;
		case WM_SIZE:
			engine::gVkWantedFramebufferExtent2D = {.width = LOWORD(iLongParameter), .height = HIWORD(iLongParameter)};
			break;
		case WM_CLOSE:
			sbQuit = true;
			return 0;
		case WM_DESTROY:
			sbQuit = true;
			break;
		case WM_SYSCOMMAND:
			if ((uiWordParameter & 0xFFF0) == SC_KEYMENU)
			{
				return 0;
			}
			break;
		default:
			break;
	}
	return DefWindowProc(hwnd, uiMessage, uiWordParameter, iLongParameter);
}

int RunModelViewer(HINSTANCE hinstance)
{
	if (!XMVerifyCPUSupport() || _set_FMA3_enable(0) != 0)
	{
		throw std::runtime_error("ModelViewer requires SSE4.1 and disabled FMA3");
	}
	if (SetProcessDPIAware() == 0)
	{
		throw std::runtime_error("ModelViewer SetProcessDPIAware failed");
	}
	auto pMultithreading = std::make_unique<common::Multithreading>(std::max(1i64, common::HardwareCoreCount() - 2));
	auto pProfileManager = std::make_unique<engine::ProfileManager>();
	auto pTerrain = std::make_unique<engine::IslandTerrain>();
	auto pFlightPlanes = std::make_unique<engine::FlightPlanes>();
	auto pResidency = std::make_unique<engine::IslandTerrainResidency>();
	try
	{
		std::ignore = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	}
	catch (const std::system_error&)
	{
		throw;
	}
	catch (const std::runtime_error& rException)
	{
		LOG(kDefault, kError, "ModelViewer failed: {}", rException.what());
		if (engine::gLaunchOptions.iAgentPort == 0)
		{
			std::ignore = MessageBox(nullptr, rException.what(), kGameName.data(), MB_OK | MB_ICONERROR);
		}
		return 1;
	}
	engine::gpIslandTerrain->WaitForElevationMaps();
	engine::LoadGraphicsSettings();
	// The viewer never publishes a native water source grid.
	engine::gWaterLevel.Reset(engine::GraphicsQualityLevel::kLow);

	WNDCLASSEX windowClass
	{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WindowProcedure,
		.hInstance = hinstance,
		.hIcon = LoadIcon(nullptr, IDI_APPLICATION),
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
		.lpszClassName = kGameName.data(),
	};
	if (RegisterClassEx(&windowClass) == 0)
	{
		throw std::runtime_error("ModelViewer RegisterClassEx failed");
	}
	common::ScopedLambda unregisterWindow([&]()
	{
		UnregisterClass(kGameName.data(), hinstance);
	});
	VkExtent2D vkExtent = engine::gLaunchOptions.vkWindowedExtent;
	if (vkExtent.width == 0)
	{
		vkExtent = {.width = 1'280, .height = 800};
	}
	engine::gVkWantedFramebufferExtent2D = vkExtent;
	RECT rectangle
	{
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(vkExtent.width),
		.bottom = static_cast<LONG>(vkExtent.height),
	};
	if (AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE) == 0)
	{
		throw std::runtime_error("ModelViewer AdjustWindowRect failed");
	}
	HWND hwnd = CreateWindow(kGameName.data(), kGameName.data(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top, nullptr, nullptr, hinstance, nullptr);
	if (hwnd == nullptr)
	{
		throw std::runtime_error("ModelViewer CreateWindow failed");
	}
	common::ScopedLambda destroyWindow([&]()
	{
		DestroyWindow(hwnd);
	});
	auto pApplication = std::make_unique<ModelViewerApplication>();
	auto pGraphics = std::make_unique<engine::Graphics>(hinstance, hwnd);
	auto pCamera = std::make_unique<Camera>();
	engine::gpTextureManager->WaitForTextures(engine::TextureManager::kPriorityTextures);
	for (int64_t i = 0, iCount = std::ssize(engine::gpCommandBufferManager->mPerFramebufferCommandBuffers); i < iCount; ++i)
	{
		pApplication->Render();
	}
	if (engine::gLaunchOptions.iAgentPort == 0)
	{
		std::ignore = ShowWindow(hwnd, engine::gLaunchOptions.vkWindowedExtent.width != 0 ? SW_SHOWNORMAL : SW_SHOWMAXIMIZED);
		std::ignore = SetForegroundWindow(hwnd);
		std::ignore = BringWindowToTop(hwnd);
		std::ignore = SetFocus(hwnd);
	}
	common::ScopedLambda hideWindow([&]()
	{
		if (engine::gLaunchOptions.iAgentPort == 0)
		{
			std::ignore = ShowWindow(hwnd, SW_HIDE);
		}
	});
	pProfileManager->BootLog();
	common::ScopedLambda disableTracking([]()
	{
		gbAllocationTrackingReady.store(false, std::memory_order_relaxed);
	});
	gbAllocationTrackingReady.store(true, std::memory_order_relaxed);
	int64_t iScreenshotSequence = 0;
	while (!sbQuit)
	{
		MSG message {};
		while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (sbQuit)
		{
			break;
		}
		pApplication->UpdateDefaultComparison(sbComparisonHeld);
		try
		{
			if (sbScreenshotRequested)
			{
				sbScreenshotRequested = false;
				if (pGraphics->mOptionalScreenshotRequest.has_value())
				{
					LOG(kGraphics, kInfo, "ModelViewer screenshot busy token={}", siScreenshotToken);
				}
				else
				{
					// Heap: the screenshot path escapes to the asynchronous encoder.
					ScopedSuppressAllocationTracking suppress;
					std::filesystem::path directory = engine::gpFileManager->mAppDataDirectory / "Screenshots";
					std::filesystem::create_directories(directory);
					engine::ScreenshotRequest request
					{
						.path = directory / std::format("viewer_{}.png", ++iScreenshotSequence),
						.iMaxWidth = 0,
						.bPng = true,
						.bPublishResult = true,
						.iCaptureToken = engine::ResetCaptureResult(),
					};
					siScreenshotToken = request.iCaptureToken;
					pGraphics->mOptionalScreenshotRequest = std::move(request);
					LOG(kGraphics, kInfo, "ModelViewer screenshot queued sequence={} token={}", iScreenshotSequence, siScreenshotToken);
				}
			}
			engine::gpTextureUploadManager->RethrowException();
			if (!pApplication->Render())
			{
				if (MsgWaitForMultipleObjectsEx(0, nullptr, 32, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_FAILED)
				{
					throw std::runtime_error("ModelViewer message wait failed");
				}
			}
		}
		catch (const std::system_error& rException)
		{
			if (rException.code() != engine::VkErrorCode(VK_ERROR_DEVICE_LOST))
			{
				throw;
			}
			LOG(kGraphics, kInfo, "ModelViewer recreating Graphics after device loss: {}", rException.what());
			// Heap: device recovery replaces the Graphics owner and all its GPU managers.
			ScopedSuppressAllocationTracking suppress;
			std::optional<engine::ScreenshotRequest> optionalScreenshotRequest = std::move(pGraphics->mOptionalScreenshotRequest);
			pGraphics.reset();
			pGraphics = std::make_unique<engine::Graphics>(hinstance, hwnd);
			pGraphics->mOptionalScreenshotRequest = std::move(optionalScreenshotRequest);
		}
		if (siScreenshotToken != 0)
		{
			// Heap: serializing the asynchronous capture result creates the completion log payload.
			ScopedSuppressAllocationTracking suppress;
			if (std::optional<nlohmann::json> optionalResult = engine::TakeCaptureResult(siScreenshotToken))
			{
				LOG(kGraphics, kInfo, "ModelViewer screenshot complete sequence={} token={} result={}", iScreenshotSequence, siScreenshotToken, optionalResult->dump());
				siScreenshotToken = 0;
			}
		}
	}
	gbAllocationTrackingReady.store(false, std::memory_order_relaxed);
	pApplication->UpdateDefaultComparison(false);
	engine::SaveGraphicsSettings();
	return 0;
}

static int ProcessMain(HINSTANCE hinstance)
{
	common::SetupExceptionHandling();
	engine::gLaunchOptions.dataDirectory = kpcDefaultDataDirectory;
	if (!engine::ParseLaunchOptions())
	{
		return 1;
	}
	if (!engine::gLaunchOptions.logFile.empty())
	{
		common::EnableLogFile(engine::gLaunchOptions.logFile);
	}
	if (!engine::gLaunchOptions.appDataDirectory.empty())
	{
		engine::SetCrashReportAppDataDirectory(engine::gLaunchOptions.appDataDirectory.c_str());
	}
	engine::ResolveCrashReportPaths();
	using ScopedHandle = std::unique_ptr<void, decltype([](HANDLE handle) noexcept
	{
		std::ignore = CloseHandle(handle);
	})>;
	HANDLE handleMutex = CreateMutex(nullptr, TRUE, "Broken Engine Sandbox");
	DWORD uiMutexError = GetLastError();
	ScopedHandle pMutex(handleMutex);
	if (handleMutex == nullptr)
	{
		LOG(kDefault, kError, "ModelViewer CreateMutex failed: {}", uiMutexError);
		return 1;
	}
	if (uiMutexError == ERROR_ALREADY_EXISTS)
	{
		LOG(kDefault, kError, "Another instance is already running");
		if (engine::gLaunchOptions.iAgentPort == 0)
		{
			std::ignore = MessageBox(nullptr, "Another instance is already running", kGameName.data(), MB_OK | MB_ICONERROR);
		}
		return 1;
	}
	auto pUploads = std::make_unique<engine::TextureUploadManager>();
	std::unique_ptr<engine::FileManager> pFiles;
	try
	{
		CHECK_HRESULT(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED));
		common::ScopedLambda uninitializeFoundation([]()
		{
			Windows::Foundation::Uninitialize();
		});
		try
		{
			pFiles = std::make_unique<engine::FileManager>();
		}
		catch (const std::system_error&)
		{
			throw;
		}
		catch (const std::runtime_error& rException)
		{
			LOG(kDefault, kError, "ModelViewer failed: {}", rException.what());
			if (engine::gLaunchOptions.iAgentPort == 0)
			{
			std::ignore = MessageBox(nullptr, rException.what(), kGameName.data(), MB_OK | MB_ICONERROR);
			}
			return 1;
		}
		return RunModelViewer(hinstance);
	}
	catch (const std::exception& rException)
	{
		if (IsDebuggerPresent() != 0) [[unlikely]]
		{
			throw;
		}
		engine::HandleException(&rException);
		engine::PromptCrashReportDesktopCopy();
	}
	catch (...)
	{
		if (IsDebuggerPresent() != 0) [[unlikely]]
		{
			throw;
		}
		engine::HandleException();
		engine::PromptCrashReportDesktopCopy();
	}
	return 1;
}

} // namespace game

int WINAPI wWinMain(_In_ HINSTANCE hinstance, [[maybe_unused]] _In_opt_ HINSTANCE hinstancePrevious, [[maybe_unused]] _In_ LPWSTR pcCommandLine, [[maybe_unused]] _In_ int iShowCommand)
{
	return common::ThreadLocal::Entry(game::ProcessMain, 10 * 1'024 * 1'024, std::nullopt, false)(hinstance);
}
