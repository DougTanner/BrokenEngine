#include "File/PackChunks.h"
#include "Graphics/Camera.h"
#include "Graphics/Screenshot.h"
#include "Memory/GlobalAllocator.h"
#include "Ui/GraphicsSettings.h"
#include "CrashReport.h"
#include "Game.h"
#include "ModelViewerApplication.h"

namespace game
{

static bool sbQuit = false;
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
	if (ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler(hwnd, uiMessage, uiWordParameter, iLongParameter) != 0)
	{
		return TRUE;
	}
	switch (uiMessage)
	{
		case WM_KEYDOWN:
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
		MessageBox(nullptr, rException.what(), kGameName.data(), MB_OK | MB_ICONERROR);
		return 1;
	}
	engine::gpIslandTerrain->WaitForElevationMaps();
	engine::LoadGraphicsSettings();

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
	ShowWindow(hwnd, SW_SHOWDEFAULT);
	common::ScopedLambda hideWindow([&]()
	{
		ShowWindow(hwnd, SW_HIDE);
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
			MessageBox(nullptr, rException.what(), kGameName.data(), MB_OK | MB_ICONERROR);
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
