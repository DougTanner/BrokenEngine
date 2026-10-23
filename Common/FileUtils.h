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
	// Deny-none sharing: a reader already holding the crash report open must not block this handle's creation. Write
	// replaces each ASCII-case-insensitive occurrence of a non-empty replaced prefix with %USERPROFILE%; the prefixes
	// must outlive the writer.
	CrashFileWriter(const wchar_t* pcPath, std::string_view firstPrefix, std::string_view secondPrefix)
	: handle(CreateFileW(pcPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr))
	, firstReplacedPrefix(firstPrefix)
	, secondReplacedPrefix(secondPrefix)
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

		int64_t iUnwrittenStart = 0;
		int64_t i = 0;
		while (i < std::ssize(text))
		{
			std::string_view rest = text.substr(static_cast<size_t>(i));
			int64_t iPrefixSize = 0;
			if (StartsWithIgnoringAsciiCase(rest, firstReplacedPrefix))
			{
				iPrefixSize = std::ssize(firstReplacedPrefix);
			}
			else if (StartsWithIgnoringAsciiCase(rest, secondReplacedPrefix))
			{
				iPrefixSize = std::ssize(secondReplacedPrefix);
			}

			if (iPrefixSize == 0)
			{
				++i;
				continue;
			}

			WriteUnchanged(text.substr(static_cast<size_t>(iUnwrittenStart), static_cast<size_t>(i - iUnwrittenStart)));
			WriteUnchanged("%USERPROFILE%");
			i += iPrefixSize;
			iUnwrittenStart = i;
		}
		WriteUnchanged(text.substr(static_cast<size_t>(iUnwrittenStart)));
	}

	HANDLE handle = INVALID_HANDLE_VALUE;
	std::string_view firstReplacedPrefix;
	std::string_view secondReplacedPrefix;

private:

	static bool StartsWithIgnoringAsciiCase(std::string_view text, std::string_view prefix)
	{
		if (prefix.empty() || text.size() < prefix.size())
		{
			return false;
		}

		auto LowerAscii = [](char cCharacter)
		{
			return (cCharacter >= 'A' && cCharacter <= 'Z') ? static_cast<char>(cCharacter - 'A' + 'a') : cCharacter;
		};
		return std::ranges::equal(text.substr(0, prefix.size()), prefix, {}, LowerAscii, LowerAscii);
	}

	void WriteUnchanged(std::string_view text)
	{
		DWORD uiWritten = 0;
		// Nothing on the crash path may recover or log a failed write.
		WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &uiWritten, nullptr);
	}
};

} // namespace common
