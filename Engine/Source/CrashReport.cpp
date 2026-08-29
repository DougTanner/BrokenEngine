#include "CrashReport.h"

#include "Game.h"

namespace engine
{

static std::string sDxDiag;
static std::atomic<bool> sbDxDiagComplete { false };
static wchar_t spcAppDataOverride[MAX_PATH + 1] {};
static wchar_t spcDesktopReportPath[MAX_PATH + 1] {};
static wchar_t spcUserReportPath[MAX_PATH + 1] {};
static constexpr wchar_t kpcFallbackReportPath[] = L"Crash-Report.txt";

void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory)
{
	spcAppDataOverride[0] = L'\0';

	wchar_t pcGameName[64] {};
	swprintf_s(pcGameName, std::size(pcGameName), L"%hs", game::kGameName.data());
	wchar_t pcCrashReportFile[128] {};
	swprintf_s(pcCrashReportFile, std::size(pcCrashReportFile), L"\\%s-Crash-Report.txt", pcGameName);

	const size_t uiDirectoryLength = wcsnlen_s(pcDirectory, std::size(spcAppDataOverride));
	const size_t uiSuffixLength = 1 + wcslen(pcGameName) + wcslen(pcCrashReportFile);
	if (uiDirectoryLength >= std::size(spcAppDataOverride))
	{
		return;
	}

	if (uiSuffixLength >= std::size(spcAppDataOverride) - uiDirectoryLength)
	{
		return;
	}

	if (wcscpy_s(spcAppDataOverride, std::size(spcAppDataOverride), pcDirectory) != 0)
	{
		spcAppDataOverride[0] = L'\0';
	}
}

// Appends only after proving the result fits: wcscat_s on an overflowing input invokes the invalid-parameter handler,
// which would terminate this otherwise healthy process at startup. A non-fitting append empties the candidate so its
// buffer is either the complete intended path or empty, never a partial one the crash handler would write to.
static bool AppendReportPath(wchar_t (&rBuffer)[MAX_PATH + 1], const wchar_t* pcText)
{
	const size_t uiUsedLength = wcsnlen_s(rBuffer, std::size(rBuffer));
	const size_t uiTextLength = wcsnlen_s(pcText, std::size(rBuffer));
	if (uiTextLength >= std::size(rBuffer) - uiUsedLength)
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
	const bool bDesktopFound = SHGetSpecialFolderPathW(HWND_DESKTOP, pcDesktopDirectory, CSIDL_DESKTOP, FALSE) != FALSE;
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
		PWSTR pWideChar = nullptr;
		HRESULT hresult = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &pWideChar);
		if (SUCCEEDED(hresult) && pWideChar != nullptr)
		{
			AppendReportPath(spcUserReportPath, pWideChar);
		}
		else if (bDesktopFound)
		{
			// OS failure on the roaming lookup: fall back to the Desktop so the report still lands somewhere writable.
			AppendReportPath(spcUserReportPath, pcDesktopDirectory);
		}
		CoTaskMemFree(pWideChar);
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
	int iResult = AgentLaunched() ? IDNO : MessageBox(nullptr, "Save crash report to desktop?", game::kGameName.data(), MB_YESNO | MB_SYSTEMMODAL);

	// Select an already-resolved path: this runs from the SIGABRT handler during heap corruption, so no path lookup,
	// directory creation, or string building may happen here — ResolveCrashReportPaths did all of it at startup.
	const wchar_t* pcPath = iResult == IDYES ? spcDesktopReportPath : spcUserReportPath;
	if (pcPath[0] == L'\0')
	{
		// Startup resolution failed for this candidate: write beside the working directory rather than nowhere.
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
	// The DxDiag thread appends to sDxDiag without a lock. The real crash path always joins it first, but the agent crash-report
	// fixture calls this mid-main-loop, so skip the still-growing string rather than race it; the markers are always written.
	if (sbDxDiagComplete.load(std::memory_order_acquire))
	{
		writer.Write(sDxDiag.c_str());
	}
	writer.Write("<End DxDiag>\n");

	common::LogDumpBuffers(writer);
}

void ReadDxDiag()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	common::ThreadLocal threadLocal(1024, common::kThreadDxDiag);

	try
	{
		CHECK_HRESULT(CoInitialize(nullptr));

		Microsoft::WRL::ComPtr<IDxDiagProvider> pIdxDiagProvider;
		CHECK_HRESULT(CoCreateInstance(CLSID_DxDiagProvider, nullptr, CLSCTX_INPROC_SERVER, IID_IDxDiagProvider, reinterpret_cast<void**>(pIdxDiagProvider.GetAddressOf())));

		DXDIAG_INIT_PARAMS dxdiagInitParams
		{
			.dwSize = sizeof(DXDIAG_INIT_PARAMS),
			.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION,
			.bAllowWHQLChecks = FALSE,
			.pReserved = nullptr,
		};
		CHECK_HRESULT(pIdxDiagProvider->Initialize(&dxdiagInitParams));

		Microsoft::WRL::ComPtr<IDxDiagContainer> pRoot;
		CHECK_HRESULT(pIdxDiagProvider->GetRootContainer(&pRoot));

		Microsoft::WRL::ComPtr<IDxDiagContainer> pDisplayDevices;
		CHECK_HRESULT(pRoot->GetChildContainer(L"DxDiag_DisplayDevices", &pDisplayDevices));

		DWORD uiChildCount = 0;
		CHECK_HRESULT(pDisplayDevices->GetNumberOfChildContainers(&uiChildCount));
		LOG(kDefault, kDebug, "DxDiag found {} children", uiChildCount);
		for (DWORD i = 0; i < uiChildCount; ++i)
		{
			WCHAR pcChildName[256] {};
			CHECK_HRESULT(pDisplayDevices->EnumChildContainerNames(i, pcChildName, 256));
			Microsoft::WRL::ComPtr<IDxDiagContainer> pChild;
			CHECK_HRESULT(pDisplayDevices->GetChildContainer(pcChildName, &pChild));

			// Best-effort per-prop reads (GetNumberOfProps / EnumPropNames / GetProp / VariantClear): results deliberately
			// unchecked. Each output is zero-initialized and used only under the VT_BSTR guard, so a failed read skips the
			// prop safely; CHECK_HRESULT here would throw and abort the whole remaining best-effort DxDiag capture.
			DWORD uiPropCount = 0;
			pChild->GetNumberOfProps(&uiPropCount);
			LOG(kDefault, kDebug, "    {} props", uiPropCount);
			for (DWORD j = 0; j < uiPropCount; ++j)
			{
				WCHAR pcPropName[256] {};
				pChild->EnumPropNames(j, pcPropName, static_cast<DWORD>(std::size(pcPropName) - 1));

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
	catch (...)
	{
		LOG(kDefault, kError, "Failed to read DxDiag");
	}

	sbDxDiagComplete.store(true, std::memory_order_release);
}

} // namespace engine
