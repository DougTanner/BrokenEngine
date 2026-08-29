#include "StringUtils.h"

namespace common
{

std::string ToString(std::wstring_view wideChars)
{
	if (wideChars.empty())
	{
		return {};
	}

	int iSize = WideCharToMultiByte(CP_UTF8, 0, wideChars.data(), static_cast<int>(wideChars.size()), nullptr, 0, nullptr, nullptr);
	std::string result(iSize, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wideChars.data(), static_cast<int>(wideChars.size()), result.data(), iSize, nullptr, nullptr);
	return result;
}

std::string ToLower(std::string_view chars)
{
	std::string out(chars);
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char uiChar) { return static_cast<char>((uiChar >= 'A' && uiChar <= 'Z') ? uiChar + ('a' - 'A') : uiChar); });
	return out;
}

std::string PathToCppVariable(std::string_view path)
{
	std::string out(path);
	std::erase_if(out, [](char c)
	{
		unsigned char uiChar = static_cast<unsigned char>(c);
		return !((uiChar >= 'A' && uiChar <= 'Z') || (uiChar >= 'a' && uiChar <= 'z') || (uiChar >= '0' && uiChar <= '9') || uiChar == '_');
	});
	return out;
}

} // namespace common
