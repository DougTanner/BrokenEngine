#include "CrashReport.h"

#include "Game.h"

namespace engine
{

static std::string sDxDiag;
static constinit std::atomic<bool> sbDxDiagComplete(false);
static_assert(decltype(sbDxDiagComplete)::is_always_lock_free);
static wchar_t spcAppDataOverride[MAX_PATH + 1] {};
static constinit wchar_t spcDesktopReportPath[MAX_PATH + 1] {};
static constinit wchar_t spcUserReportPath[MAX_PATH + 1] {};
constexpr wchar_t kpcFallbackReportPath[] = L"Crash-Report.txt";

void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory)
{
	spcAppDataOverride[0] = L'\0';

	wchar_t pcGameName[64] {};
	swprintf_s(pcGameName, std::size(pcGameName), L"%hs", game::kGameName.data());
	wchar_t pcCrashReportFile[128] {};
	swprintf_s(pcCrashReportFile, std::size(pcCrashReportFile), L"\\%s-Crash-Report.txt", pcGameName);

	int64_t iDirectoryLength = wcsnlen_s(pcDirectory, std::size(spcAppDataOverride));
	int64_t iSuffixLength = 1 + static_cast<int64_t>(std::wcslen(pcGameName)) + static_cast<int64_t>(std::wcslen(pcCrashReportFile));
	if (iDirectoryLength >= std::ssize(spcAppDataOverride))
	{
		return;
	}

	if (iSuffixLength >= std::ssize(spcAppDataOverride) - iDirectoryLength)
	{
		return;
	}

	wcscpy_s(spcAppDataOverride, std::size(spcAppDataOverride), pcDirectory);
}

// Appends only after proving the result fits: wcscat_s on an overflowing input invokes the invalid-parameter handler,
// which would terminate this otherwise healthy process at startup. A non-fitting append empties the candidate so its
// buffer is either the complete intended path or empty, never a partial one the crash handler would write to.
static bool AppendReportPath(wchar_t (&rBuffer)[MAX_PATH + 1], const wchar_t* pcText)
{
	int64_t iUsedLength = wcsnlen_s(rBuffer, std::size(rBuffer));
	int64_t iTextLength = wcsnlen_s(pcText, std::size(rBuffer));
	if (iTextLength >= std::ssize(rBuffer) - iUsedLength)
	{
		rBuffer[0] = L'\0';
		return false;
	}

	wcscat_s(rBuffer, std::size(rBuffer), pcText);
	return true;
}

void ResolveCrashReportPaths()
{
	spcDesktopReportPath[0] = L'\0';
	spcUserReportPath[0] = L'\0';

	wchar_t pcGameName[64] {};
	swprintf_s(pcGameName, std::size(pcGameName), L"%hs", game::kGameName.data());
	wchar_t pcCrashReportFile[128] {};
	swprintf_s(pcCrashReportFile, std::size(pcCrashReportFile), L"%s-Crash-Report.txt", pcGameName);
	std::replace(std::begin(pcCrashReportFile), std::end(pcCrashReportFile), L' ', L'-');

	wchar_t pcDesktopDirectory[MAX_PATH + 1] {};
	bool bDesktopFound = SHGetSpecialFolderPathW(HWND_DESKTOP, pcDesktopDirectory, CSIDL_DESKTOP, FALSE) != FALSE;
	if (bDesktopFound && AppendReportPath(spcDesktopReportPath, pcDesktopDirectory) && AppendReportPath(spcDesktopReportPath, L"\\"))
	{
		AppendReportPath(spcDesktopReportPath, pcCrashReportFile);
	}

	if (spcAppDataOverride[0] != L'\0')
	{
		AppendReportPath(spcUserReportPath, spcAppDataOverride);
	}
	else
	{
		PWSTR pcWideCharacter = nullptr;
		int64_t iHresult = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &pcWideCharacter);
		if (SUCCEEDED(static_cast<HRESULT>(iHresult)) && pcWideCharacter != nullptr)
		{
			AppendReportPath(spcUserReportPath, pcWideCharacter);
		}
		else if (bDesktopFound)
		{
			// OS failure on the roaming lookup: fall back to the Desktop so the report still lands somewhere writable.
			AppendReportPath(spcUserReportPath, pcDesktopDirectory);
		}
		CoTaskMemFree(pcWideCharacter);
	}

	if (spcUserReportPath[0] != L'\0' && AppendReportPath(spcUserReportPath, L"\\") && AppendReportPath(spcUserReportPath, pcGameName))
	{
		// Allocator-free single-level create (parent guaranteed present above): FileManager creates the same directory but
		// is constructed later, so a crash before that still needs it. Ignore the result — ERROR_ALREADY_EXISTS is fine.
		CreateDirectoryW(spcUserReportPath, nullptr);

		if (AppendReportPath(spcUserReportPath, L"\\"))
		{
			AppendReportPath(spcUserReportPath, pcCrashReportFile);
		}
	}
}

void HandleException(std::optional<const std::exception*> pException)
{
	// Non-logging break: this runs from the SIGABRT handler during heap corruption, possibly while this thread already
	// holds the log mutex, so logging here would deadlock or re-fault before any crash-report bytes are written.
	DEBUG_BREAK_NO_LOG();

	// An agent-launched instance must never block on a modal dialog — take the unprompted branch so the report still saves.
	int64_t iResult = (gLaunchOptions.iAgentPort != 0) ? IDNO : MessageBox(nullptr, "Save crash report to desktop?", game::kGameName.data(), MB_YESNO | MB_SYSTEMMODAL);

	// Select an already-resolved path: this runs from the SIGABRT handler during heap corruption, so no path lookup,
	// directory creation, or string building may happen here — ResolveCrashReportPaths did all of it at startup.
	const wchar_t* pcPath = iResult == IDYES ? spcDesktopReportPath : spcUserReportPath;
	if (pcPath[0] == L'\0')
	{
		// Startup resolution failed for this candidate; the fallback path is relative to the working directory.
		pcPath = kpcFallbackReportPath;
	}

	common::CrashFileWriter writer(pcPath);

	writer.Write("\n\n\nPlease send this crash report to brokenteapotstudios@gmail.com, and if possible describe exactly what you were doing when it occurred.\n");

	char pcLine[256] {};
	std::snprintf(pcLine, std::size(pcLine), "Game name: %.*s\n", static_cast<int>(game::kGameName.size()), game::kGameName.data());
	writer.Write(pcLine);
	std::snprintf(pcLine, std::size(pcLine), "Game version: %lld\n", static_cast<long long>(game::kiGameVersion));
	writer.Write(pcLine);

	writer.Write("\n\n\n");
	if (pException.has_value())
	{
		writer.Write(pException.value()->what());
	}
	else
	{
		writer.Write("Unknown exception");
	}
	writer.Write("\n");

	writer.Write("\n\n\n<Begin callstack>\n");
	common::CrashFileStackWalker crashFileStackWalker(StackWalker::AfterCatch, &writer);
	crashFileStackWalker.ShowCallstack();
	writer.Write("<End callstack>\n");

	writer.Write("\n\n\n<Begin DxDiag>\n");
	// The DxDiag thread writes sDxDiag without a lock. The agent harness rig and SIGABRT handler can run before it completes,
	// so read the string only after completion is published; the section markers remain unconditional.
	if (sbDxDiagComplete.load(std::memory_order_acquire))
	{
		writer.Write(sDxDiag.c_str());
	}
	writer.Write("<End DxDiag>\n");

	common::LogDumpBuffers(writer);
}

static bool DxDiagCallFailed(int64_t iHresult, std::string_view call)
{
	if (SUCCEEDED(static_cast<HRESULT>(iHresult)))
	{
		return false;
	}

	// Heap: HresultToString returns a std::string, and this thread participates in main-loop allocation tracking.
	ScopedSuppressAllocationTracking suppress;
	LOG(kDefault, kError, "Failed to read DxDiag: {} failed: {}", call, common::HresultToString(static_cast<HRESULT>(iHresult)).data());
	return true;
}

void ReadDxDiag()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	// Declared before the try so every ComPtr releases first and completion publishes once, after the last sDxDiag write.
	common::ScopedLambda publishComplete([]()
	{
		sbDxDiagComplete.store(true, std::memory_order_release);
	});

	try
	{
		if (DxDiagCallFailed(CoInitialize(nullptr), "CoInitialize"))
		{
			return;
		}

		Microsoft::WRL::ComPtr<IDxDiagProvider> pIdxDiagProvider;
		if (DxDiagCallFailed(CoCreateInstance(CLSID_DxDiagProvider, nullptr, CLSCTX_INPROC_SERVER, IID_IDxDiagProvider, reinterpret_cast<void**>(pIdxDiagProvider.GetAddressOf())), "CoCreateInstance"))
		{
			return;
		}

		DXDIAG_INIT_PARAMS dxdiagInitParams
		{
			.dwSize = sizeof(DXDIAG_INIT_PARAMS),
			.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION,
			.bAllowWHQLChecks = FALSE,
			.pReserved = nullptr,
		};
		if (DxDiagCallFailed(pIdxDiagProvider->Initialize(&dxdiagInitParams), "IDxDiagProvider::Initialize"))
		{
			return;
		}

		Microsoft::WRL::ComPtr<IDxDiagContainer> pRoot;
		if (DxDiagCallFailed(pIdxDiagProvider->GetRootContainer(&pRoot), "GetRootContainer"))
		{
			return;
		}

		Microsoft::WRL::ComPtr<IDxDiagContainer> pDisplayDevices;
		if (DxDiagCallFailed(pRoot->GetChildContainer(L"DxDiag_DisplayDevices", &pDisplayDevices), "GetChildContainer(DxDiag_DisplayDevices)"))
		{
			return;
		}

		DWORD uiChildCount = 0;
		if (DxDiagCallFailed(pDisplayDevices->GetNumberOfChildContainers(&uiChildCount), "GetNumberOfChildContainers"))
		{
			return;
		}
		int64_t iChildCount = uiChildCount;
		LOG(kDefault, kDebug, "DxDiag found {} children", iChildCount);
		for (int64_t i = 0; i < iChildCount; ++i)
		{
			WCHAR pcChildName[256] {};
			if (DxDiagCallFailed(pDisplayDevices->EnumChildContainerNames(static_cast<DWORD>(i), pcChildName, 256), "EnumChildContainerNames"))
			{
				return;
			}
			Microsoft::WRL::ComPtr<IDxDiagContainer> pChild;
			if (DxDiagCallFailed(pDisplayDevices->GetChildContainer(pcChildName, &pChild), "GetChildContainer"))
			{
				return;
			}

			// Best-effort per-prop reads (GetNumberOfProps / EnumPropNames / GetProp / VariantClear) leave results
			// unchecked; zero-initializing each output keeps a failed call harmless, and only a GetProp result of VT_BSTR is
			// appended to the report.
			DWORD uiPropCount = 0;
			pChild->GetNumberOfProps(&uiPropCount);
			int64_t iPropCount = uiPropCount;
			LOG(kDefault, kDebug, "    {} props", iPropCount);
			for (int64_t j = 0; j < iPropCount; ++j)
			{
				WCHAR pcPropName[256] {};
				pChild->EnumPropNames(static_cast<DWORD>(j), pcPropName, static_cast<DWORD>(std::size(pcPropName) - 1));

				VARIANT variant {};
				pChild->GetProp(pcPropName, &variant);
				if (variant.vt == VT_BSTR && variant.bstrVal != nullptr)
				{
					// Heap: one-shot background startup enumeration on this dedicated thread, not main-loop work — it builds a
					// persistent crash-report string whose length depends on the machine's device properties, so neither the
					// ToString conversions nor sDxDiag's growth can be sized up front or moved to a Workbuffer.
					ScopedSuppressAllocationTracking suppress;

					sDxDiag += common::ToString(pcPropName);
					sDxDiag += ": ";
					sDxDiag += common::ToString(variant.bstrVal);
					sDxDiag += "\n";
				}
				VariantClear(&variant);
			}
		}
	}
	catch ([[maybe_unused]] const std::exception& rException)
	{
		LOG(kDefault, kError, "Failed to read DxDiag: {}", rException.what());
	}
}

} // namespace engine
