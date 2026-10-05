#pragma once

namespace common
{

template<typename T>
inline std::pair<bool, std::string> GetFileOrStringContent(const T& rSource)
{
	if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
	{
		return {true, rSource};
	}
	else if constexpr (std::is_same_v<std::decay_t<T>, std::filesystem::path>)
	{
		if (!std::filesystem::exists(rSource))
		{
			return {false, {}};
		}

		int64_t iFileSize = static_cast<int64_t>(std::filesystem::file_size(rSource));
		std::string fileContents;
		fileContents.resize(static_cast<std::string::size_type>(iFileSize));

		std::fstream fileStream(rSource, std::ios::in | std::ios::binary);
		fileStream.read(fileContents.data(), static_cast<std::streamsize>(iFileSize));
		bool bReadOk = static_cast<bool>(fileStream) && fileStream.gcount() == iFileSize;
		fileStream.close();

		if (!bReadOk)
		{
			return {false, {}};
		}

		return {true, std::move(fileContents)};
	}
	else
	{
		static_assert(false, "GetFileOrStringContent only supports std::string and std::filesystem::path");
	}
}

template<typename T1, typename T2>
inline bool ContentsEqual(const T1& rOne, const T2& rTwo)
{
	auto [bOneValid, oneContent] = GetFileOrStringContent(rOne);
	auto [bTwoValid, twoContent] = GetFileOrStringContent(rTwo);

	if (!bOneValid || !bTwoValid)
	{
		return false;
	}

	return oneContent == twoContent;
}

// Offline/tool-side use only.
// Named ReadEntireFile (not ReadFile) to avoid colliding with the Win32 ReadFile API inside namespace common.
inline std::vector<std::byte> ReadEntireFile(const std::filesystem::path& rPath)
{
	std::vector<std::byte> data(std::filesystem::file_size(rPath));
	std::fstream fileStream(rPath, std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		throw std::runtime_error("ReadEntireFile failed to open file");
	}
	fileStream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(std::ssize(data)));
	if (fileStream.gcount() != std::ssize(data))
	{
		throw std::runtime_error("ReadEntireFile failed to read complete file");
	}
	return data;
}

// Reachable from the SIGABRT handler during heap corruption, where std::ofstream cannot be used at all — its
// construction allocates (new locale), which re-enters allocation tracking and can deadlock on the non-recursive log mutex.
struct CrashFileWriter
{
	// Deny-none sharing: a reader already holding the crash report open must not block this handle's creation.
	explicit CrashFileWriter(const wchar_t* pcPath)
	: handle(CreateFileW(pcPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr))
	{
	}

	~CrashFileWriter()
	{
		if (handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(handle);
		}
	}

	CrashFileWriter(const CrashFileWriter&) = delete;
	CrashFileWriter& operator=(const CrashFileWriter&) = delete;

	void Write(std::string_view text)
	{
		if (handle == INVALID_HANDLE_VALUE)
		{
			return;
		}

		DWORD uiWritten = 0;
		// Nothing on the crash path may recover or log a failed write.
		WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &uiWritten, nullptr);
	}

	HANDLE handle = INVALID_HANDLE_VALUE;
};

} // namespace common
