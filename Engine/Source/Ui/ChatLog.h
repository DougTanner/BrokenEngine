#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct ChatLog
{
	struct Entry
	{
		int64_t iSenderId = 0;
		char pcText[NetworkMessages::ClientChatMessage::kiMaxTextBytes + 1] = {};
		std::chrono::steady_clock::time_point receivedTime {};
	};

	static constexpr int64_t kiCapacity = 100;

	// Ring of received messages: iHead indexes the oldest of the iCount valid entries.
	Entry pEntries[kiCapacity] {};
	int64_t iHead = 0;
	int64_t iCount = 0;

	bool bInputOpen = false;
	char pcInput[NetworkMessages::ClientChatMessage::kiMaxTextBytes + 1] = {};

	// Overwrites the oldest entry once the ring is full.
	void Append(int64_t iSenderId, std::string_view text)
	{
		Entry& rEntry = pEntries[(iHead + iCount) % kiCapacity];
		rEntry.iSenderId = iSenderId;
		ASSERT(std::ssize(text) < std::ssize(rEntry.pcText));
		std::memcpy(rEntry.pcText, text.data(), text.size());
		rEntry.pcText[text.size()] = '\0';
		rEntry.receivedTime = std::chrono::steady_clock::now();

		if (iCount == kiCapacity)
		{
			iHead = (iHead + 1) % kiCapacity;
		}
		else
		{
			++iCount;
		}
	}
};

} // namespace engine

#endif // BT_CLIENT
