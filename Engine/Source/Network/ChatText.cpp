#include "Pch.h"

#include "Network/ChatText.h"

namespace engine
{

bool IsValidChatText(std::string_view text)
{
	if (text.empty() || std::ssize(text) > NetworkMessages::ClientChatMessage::kiMaxTextBytes)
	{
		return false;
	}

	int64_t iOffset = 0;
	while (iOffset < std::ssize(text))
	{
		uint32_t uiLead = static_cast<uint8_t>(text[iOffset]);
		int64_t iLength = 0;
		uint32_t uiCodePoint = 0;
		uint32_t uiMinimumCodePoint = 0;
		if (uiLead < 0x80)
		{
			iLength = 1;
			uiCodePoint = uiLead;
		}
		else if (uiLead >= 0xC0 && uiLead < 0xE0)
		{
			iLength = 2;
			uiCodePoint = uiLead & 0x1F;
			uiMinimumCodePoint = 0x80;
		}
		else if (uiLead >= 0xE0 && uiLead < 0xF0)
		{
			iLength = 3;
			uiCodePoint = uiLead & 0x0F;
			uiMinimumCodePoint = 0x800;
		}
		else if (uiLead >= 0xF0 && uiLead < 0xF8)
		{
			iLength = 4;
			uiCodePoint = uiLead & 0x07;
			uiMinimumCodePoint = 0x10000;
		}
		else
		{
			return false;
		}

		if (iLength > std::ssize(text) - iOffset)
		{
			return false;
		}
		for (int64_t i = 1; i < iLength; ++i)
		{
			uint32_t uiContinuation = static_cast<uint8_t>(text[iOffset + i]);
			if ((uiContinuation & 0xC0) != 0x80)
			{
				return false;
			}
			uiCodePoint = (uiCodePoint << 6) | (uiContinuation & 0x3F);
		}

		if (uiCodePoint < uiMinimumCodePoint || (uiCodePoint >= 0xD800 && uiCodePoint <= 0xDFFF) || uiCodePoint > 0x10FFFF)
		{
			return false;
		}
		if (uiCodePoint < 0x20 || (uiCodePoint >= 0x7F && uiCodePoint <= 0x9F))
		{
			return false;
		}

		iOffset += iLength;
	}
	return true;
}

} // namespace engine
