#include "DiagnosticReporter.h"

namespace diagnostic
{


static std::atomic<bool> sbValidatedLinkedWorktree = false;

static std::string PathToUtf8(const std::filesystem::path& rPath)
{
	std::u8string value = rPath.u8string();
	return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

static std::wstring Utf8ToWide(std::string_view value)
{
	if (value.empty())
	{
		return {};
	}
	int64_t iCharacters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
	if (iCharacters == 0)
	{
		iCharacters = MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		std::wstring wideValue(static_cast<size_t>(iCharacters), L'\0');
		MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), wideValue.data(), static_cast<int>(iCharacters));
		return wideValue;
	}
	std::wstring wideValue(static_cast<size_t>(iCharacters), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wideValue.data(), static_cast<int>(iCharacters));
	return wideValue;
}

static std::string NormalizeUtf8(std::string_view value)
{
	std::wstring wideValue = Utf8ToWide(value);
	if (wideValue.empty())
	{
		return {};
	}
	int64_t iBytes = WideCharToMultiByte(CP_UTF8, 0, wideValue.data(), static_cast<int>(wideValue.size()), nullptr, 0, nullptr, nullptr);
	std::string utf8Value(static_cast<size_t>(iBytes), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wideValue.data(), static_cast<int>(wideValue.size()), utf8Value.data(), static_cast<int>(iBytes), nullptr, nullptr);
	return utf8Value;
}

static std::string BuildModalText(const Record& rRecord)
{
	if (rRecord.exportFailures.empty())
	{
		return NormalizeUtf8(rRecord.message);
	}

	std::string text;
	for (int64_t i = 0; i < std::ssize(rRecord.exportFailures); ++i)
	{
		if (i > 0)
		{
			text.append("\n\n");
		}
		const ExportFailure& rFailure = rRecord.exportFailures.at(static_cast<size_t>(i));
		if (rFailure.assetPath)
		{
			text.append("Asset: ");
			text.append(PathToUtf8(*rFailure.assetPath));
			text.append("\n\n");
		}
		text.append(NormalizeUtf8(rFailure.message));
	}
	return text;
}


void MarkValidatedLinkedWorktree()
{
	sbValidatedLinkedWorktree.store(true, std::memory_order_release);
}

ButtonResult Report(const Record& rRecord)
{
	std::string text = BuildModalText(rRecord);
	if (rRecord.eSeverity == Severity::kError)
	{
		LOG(kDefault, kError, "{}: {}", rRecord.title, text);
	}
	else
	{
		LOG(kDefault, kWarning, "{}: {}", rRecord.title, text);
	}
	// Automated runs skip modal input; cancelling a locked pack prompt fails the affected asset type.
	static const bool sbNoninteractiveEnvironment = []()
	{
		wchar_t pcNoninteractive[2] {};
		int64_t iNoninteractiveLength = GetEnvironmentVariableW(L"BT_DATAPACKER_NONINTERACTIVE", pcNoninteractive, static_cast<DWORD>(std::size(pcNoninteractive)));
		return iNoninteractiveLength == 1 && pcNoninteractive[0] == L'1';
	}();
	if (sbNoninteractiveEnvironment)
	{
		return rRecord.eButtons == ButtonContract::kOk ? ButtonResult::kAcknowledged : ButtonResult::kCancelled;
	}

	if (sbValidatedLinkedWorktree.load(std::memory_order_acquire))
	{
		return rRecord.eButtons == ButtonContract::kOk ? ButtonResult::kAcknowledged : ButtonResult::kCancelled;
	}

	fflush(stdout);
	UINT uiFlags = MB_SYSTEMMODAL;
	uiFlags |= rRecord.eButtons == ButtonContract::kOk ? MB_OK : MB_OKCANCEL;
	switch (rRecord.eIcon)
	{
		case ModalIcon::kNone:
			break;
		case ModalIcon::kError:
			uiFlags |= MB_ICONERROR;
			break;
		case ModalIcon::kWarning:
			uiFlags |= MB_ICONWARNING;
			break;
	}
	std::wstring title = Utf8ToWide(rRecord.title);
	std::wstring message = Utf8ToWide(text);
	int64_t iResult = MessageBoxW(nullptr, message.c_str(), title.c_str(), uiFlags);
	return rRecord.eButtons == ButtonContract::kOk || iResult == IDOK ? ButtonResult::kAcknowledged : ButtonResult::kCancelled;
}

DiskSpaceDecision ReportMaterializationDiskSpace(uint64_t uiAllocation, uint64_t uiAvailable, const std::filesystem::path& rSource, const std::filesystem::path& rDestination)
{
	uint64_t uiReserve = (std::max)(1ui64 << 30, (uiAllocation * 5 + 99) / 100);
	uint64_t uiRequired = uiAllocation + uiReserve;
	if (uiRequired > uiAvailable)
	{
		Record record
		{
			.eSeverity = Severity::kError,
			.title = "DataPacker - Insufficient Disk Space",
			.message = std::format("Insufficient disk space copying \"{}\" to \"{}\". Required: {} bytes. Available: {} bytes.", rSource.string(), rDestination.string(), uiRequired, uiAvailable),
			.eButtons = ButtonContract::kOk,
			.eIcon = ModalIcon::kError,
		};
		Report(record);
		return DiskSpaceDecision::kFailed;
	}

	uint64_t uiProjected = uiAvailable - uiAllocation;
	uint64_t uiWarning = 10ui64 << 30;
	if (uiProjected < uiWarning)
	{
		Record record
		{
			.eSeverity = Severity::kWarning,
			.title = "DataPacker - Low Disk Space",
			.message = std::format("Copy-on-write of \"{}\" to \"{}\" needs approximately {} bytes. Available: {} bytes. Projected remaining: {} bytes.", rSource.string(), rDestination.string(), uiAllocation, uiAvailable, uiProjected),
			.eButtons = ButtonContract::kOkCancel,
			.eIcon = ModalIcon::kWarning,
		};
		return Report(record) == ButtonResult::kAcknowledged ? DiskSpaceDecision::kProceed : DiskSpaceDecision::kCancelled;
	}
	return DiskSpaceDecision::kProceed;
}

} // namespace diagnostic
