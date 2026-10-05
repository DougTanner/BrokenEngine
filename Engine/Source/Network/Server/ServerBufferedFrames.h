#pragma once

#if defined(BT_SERVER)

#include "Frame/GridCoord.h"
#include "Network/Server/ServerTypes.h"

namespace engine
{

struct ClientConnection;
struct FrameStaticData;
class Server;

} // namespace engine

namespace game
{

struct Frame;

} // namespace game

namespace engine
{

// Per-coord ring buffer entry for re-send support
struct PerCoordBufferedFrame
{
	int64_t iTick = 0;
	common::crc_t uiSharedCrc = 0;
	// Heap: variable-size compressed status change data per frame
	std::vector<uint8_t> compressedData;
};

struct BufferedFullFrame
{
	int64_t iTick = 0;
	// Heap: serialized frame data per grid coordinate for debug frame requests
	std::unordered_map<GridCoord, std::string> serializedFrames;
};

// Server main thread only. Set mpTarget to the destination before writing; appending retains its
// capacity across serializations.
class StringAppendStreamBuf : public std::streambuf
{
public:

	std::string* mpTarget = nullptr;

protected:

	int_type overflow(int_type iChar) override
	{
		if (iChar != traits_type::eof())
		{
			mpTarget->push_back(static_cast<char>(iChar));
		}
		return traits_type::not_eof(iChar);
	}

	std::streamsize xsputn(const char_type* pData, std::streamsize iCount) override
	{
		mpTarget->append(pData, static_cast<size_t>(iCount));
		return iCount;
	}
};

class ServerBufferedFrames
{
public:

	explicit ServerBufferedFrames(Server& rServer);

	void BufferFrame(int64_t iTick, std::span<const std::pair<GridCoord, GridUpdateData>> gridUpdates);
	void BufferFullFrame(int64_t iTick, std::span<const std::pair<GridCoord, const game::Frame*>> frames);
	void ClearBufferedFrames();
	void SendUpdate(ClientConnection& rClient, int64_t iTick);
	void SendResends(ClientConnection& rClient, int64_t iTick);
	void SendCoordinateFullState(int64_t iClientId, int64_t iSlot, int64_t iTick, GridCoord coordinate, const game::Frame* pFrame);
	void SendCoordinateStaticData(int64_t iClientId, int64_t iSlot, GridCoord coordinate, const FrameStaticData& rStaticData);
	void ApplyAckStream(ClientConnection& rClient, const NetworkMessages::ClientAckStreamMessage& rMessage, int64_t iClientId);
	void SendDebugFrame(ENetPeer* pPeer, int64_t iTick, GridCoord coordinate);

private:

	void WriteBufferedFramePacket(common::Workbuffer& rWorkbuffer, PacketType eType, int64_t iSlot, int64_t iEpoch, const PerCoordBufferedFrame& rBuffered, int64_t iTimestampNanoseconds);
	const PerCoordBufferedFrame* FindBufferedFrame(GridCoord coordinate, int64_t iTick) const;
	int64_t CompressToBuffer(std::span<const char> data);

	void UpdateResendLogState(ClientConnection& rClient, int64_t iSlot, int64_t iSlotResendCount, GridCoord coordinate);

	Server& mrServer;

	// Per-coord ring buffers for re-sends
	std::unordered_map<GridCoord, std::deque<PerCoordBufferedFrame>> mPerCoordinateBufferedFrames;
	int64_t miLatestBufferedTick = -1;

	// Ring buffer for debug frame requests
	std::deque<BufferedFullFrame> mBufferedFullFrames;

	// Compression scratch buffer (reused across BufferFrame calls)
	std::vector<uint8_t> mCompressionBuffer;

	// Server main thread only. mFrameStream writes to mFrameStreamBuffer.mpTarget: recycled full-frame pool
	// entries or mSendScratch for transient coordinate sends.
	StringAppendStreamBuf mFrameStreamBuffer;
	std::ostream mFrameStream = std::ostream(&mFrameStreamBuffer);
	std::string mSendScratch;
	// Heap: recycled per-coord buffers for the full-frame ring, reused across BufferFullFrame calls
	std::vector<std::string> mFullFramePool;
};

} // namespace engine

#endif // BT_SERVER
