#include "Network/Client/Client.h"

#if defined(BT_CLIENT)

#include "CrashReport.h"

namespace engine
{

// A report counts as sent only when it ends with this trailer. HandleException truncates the report it writes, so
// every new report starts unsent.
constexpr char kpcCrashReportSentTrailer[] = "\nThis crash report was sent to the server.\n";
constexpr int64_t kiCrashReportSentTrailerSize = std::ssize(kpcCrashReportSentTrailer) - 1;

// Set by OnCrashReportPacketFreed once the server has acknowledged a whole crash report packet. Main thread only.
static bool sbPendingCrashReportDelivered = false;

// Runs inside ENet calls (Client::Poll, the UploadCrashReport wait, teardown), so it only sets the flag. ENet sets
// ENET_PACKET_FLAG_SENT only when the acknowledgment of the packet's last fragment frees it; a reset or a timeout
// frees it without the flag.
static void OnCrashReportPacketFreed(ENetPacket* pPacket)
{
	if ((pPacket->flags & ENET_PACKET_FLAG_SENT) != 0)
	{
		sbPendingCrashReportDelivered = true;
	}
}

// Returns the number of leading report bytes read into buffer; 0 when the report is missing or empty.
static int64_t ReadCrashReport(const wchar_t* pcReportPath, std::span<uint8_t> buffer)
{
	std::fstream reportStream(std::filesystem::path(pcReportPath), std::ios::in | std::ios::binary);
	reportStream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(std::ssize(buffer)));
	return static_cast<int64_t>(reportStream.gcount());
}

static bool AppendCrashReportSentTrailer(const wchar_t* pcReportPath)
{
	std::fstream reportStream(std::filesystem::path(pcReportPath), std::ios::out | std::ios::binary | std::ios::app);
	reportStream.write(kpcCrashReportSentTrailer, kiCrashReportSentTrailerSize);
	reportStream.close();
	return !reportStream.fail();
}

void TryUploadCrashReport()
{
	try
	{
		if (gpClient == nullptr)
		{
			return;
		}

		if (!(gpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted))
		{
			return;
		}

		// The server counts a second report on one connection as a rate violation and does not store it; this report
		// stays unsent and goes on the next accepted connection.
		if (gpClient->mStateFlags & Client::ClientStateFlags::kCrashReportSent)
		{
			return;
		}

		// Heap: the report path conversion, file streams, and ENet servicing allocate; this runs once, on the crash path.
		ScopedSuppressAllocationTracking suppress;
		common::ScopedWorkbufferAllocation<uint8_t*> reportAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<uint8_t*>(kiMaximumCrashReportSize);
		int64_t iReadSize = ReadCrashReport(ReportPath(), std::span<uint8_t>(reportAllocation.mpData, static_cast<size_t>(kiMaximumCrashReportSize)));
		if (iReadSize == 0)
		{
			return;
		}

		sbPendingCrashReportDelivered = false;
		gpClient->UploadCrashReport(std::span<const uint8_t>(reportAllocation.mpData, static_cast<size_t>(iReadSize)), &OnCrashReportPacketFreed);
		if (!sbPendingCrashReportDelivered)
		{
			LOG(kNetwork, kWarning, "TryUploadCrashReport Not delivered Bytes: {}", iReadSize);
			return;
		}

		LOG(kNetwork, kInfo, "TryUploadCrashReport Delivered Bytes: {}", iReadSize);
		if (!AppendCrashReportSentTrailer(ReportPath()))
		{
			LOG(kNetwork, kWarning, "TryUploadCrashReport Failed to tag the report as sent");
		}
	}
	// A failed upload must not replace the crash being handled.
	catch (...)
	{
	}
}

void PollPendingCrashReport(Client* pClient)
{
	// Runs before the pClient check: the delivery is observed inside a Client::Poll after this point, and the session
	// may destroy that Client before the next call.
	if (sbPendingCrashReportDelivered)
	{
		sbPendingCrashReportDelivered = false;
		// Heap: the report path conversion and file stream allocate; this runs at most once per delivered report.
		ScopedSuppressAllocationTracking suppress;
		if (AppendCrashReportSentTrailer(PendingReportPath()))
		{
			LOG(kNetwork, kInfo, "PollPendingCrashReport Tagged the report as sent");
		}
		else
		{
			LOG(kNetwork, kWarning, "PollPendingCrashReport Failed to tag the report as sent");
		}
	}

	if (pClient == nullptr)
	{
		return;
	}

	if (!(pClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted))
	{
		return;
	}

	if (pClient->mStateFlags & Client::ClientStateFlags::kCrashReportChecked)
	{
		return;
	}
	pClient->mStateFlags.Set(Client::ClientStateFlags::kCrashReportChecked);

	// Heap: the report path conversion and file streams allocate; this runs at most once per accepted connection.
	ScopedSuppressAllocationTracking suppress;
	std::fstream reportStream(std::filesystem::path(PendingReportPath()), std::ios::in | std::ios::binary | std::ios::ate);
	// tellg is -1 when no report exists.
	int64_t iFileSize = static_cast<int64_t>(reportStream.tellg());
	if (iFileSize <= 0)
	{
		return;
	}

	if (iFileSize >= kiCrashReportSentTrailerSize)
	{
		char pcTail[kiCrashReportSentTrailerSize] {};
		reportStream.seekg(iFileSize - kiCrashReportSentTrailerSize);
		reportStream.read(pcTail, kiCrashReportSentTrailerSize);
		if (reportStream.fail())
		{
			LOG(kNetwork, kWarning, "PollPendingCrashReport Failed to read the report");
			return;
		}

		if (std::string_view(pcTail, std::size(pcTail)) == std::string_view(kpcCrashReportSentTrailer, std::size(pcTail)))
		{
			return;
		}
	}
	reportStream.close();

	common::ScopedWorkbufferAllocation<uint8_t*> reportAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<uint8_t*>(kiMaximumCrashReportSize);
	int64_t iReadSize = ReadCrashReport(PendingReportPath(), std::span<uint8_t>(reportAllocation.mpData, static_cast<size_t>(kiMaximumCrashReportSize)));
	if (iReadSize == 0)
	{
		LOG(kNetwork, kWarning, "PollPendingCrashReport Failed to read the report");
		return;
	}

	// The server rejects a report without this build's header as corrupt input; it stays here until the next rename.
	if (!ParseCrashReportHeader(std::string_view(reinterpret_cast<const char*>(reportAllocation.mpData), static_cast<size_t>(iReadSize))).has_value())
	{
		LOG(kNetwork, kWarning, "PollPendingCrashReport Not sent, no crash report header of this version File: {}", std::filesystem::path(PendingReportPath()).filename());
		return;
	}

	pClient->SendCrashReport(std::span<const uint8_t>(reportAllocation.mpData, static_cast<size_t>(iReadSize)), &OnCrashReportPacketFreed);
	LOG(kNetwork, kInfo, "PollPendingCrashReport Sent unsent report Bytes: {}", iReadSize);
}

} // namespace engine

#endif // BT_CLIENT
