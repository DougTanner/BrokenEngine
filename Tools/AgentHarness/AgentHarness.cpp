// AgentHarness — loopback client/server command transport and harness lock owner.

#include "HarnessLockCommands.h"
#include "ToolCliCommon.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace toolcli
{
	constexpr uint32_t kuiMaxRequestBytes = 1u * 1'024u * 1'024u;
	constexpr uint32_t kuiMaxResponseBytes = 16u * 1'024u * 1'024u;
	constexpr int64_t kiConnectAttemptTimeoutMilliseconds = 500; // per connect try; retried until the --timeout-ms deadline
	constexpr int64_t kiConnectRetrySleepMilliseconds = 150; // brief pause between connect tries
	constexpr int64_t kiDefaultResponseTimeoutMilliseconds = 15'000;
	constexpr int64_t kiHeartbeatIntervalMilliseconds = 60'000;
	constexpr int64_t kiReadinessWaitCapMilliseconds = 30'000;

	enum class SocketOperationResult
	{
		kSuccess,
		kTransportFailure,
		kOwnershipLoss,
		kReadinessFailure,
	};

	struct SocketCommandArguments
	{
		int64_t iPort = 0;
		int64_t iTimeoutMilliseconds = kiDefaultResponseTimeoutMilliseconds;
		std::wstring owner;
		std::string inlineRequest;
		bool bReadStandardInput = false;
	};

	class ScopedWindowsSockets
	{
	public:
		ScopedWindowsSockets() = default;
		~ScopedWindowsSockets()
		{
			if (mbInitialized)
			{
				::WSACleanup();
			}
		}

		ScopedWindowsSockets(const ScopedWindowsSockets&) = delete;
		ScopedWindowsSockets& operator=(const ScopedWindowsSockets&) = delete;

		bool Initialize()
		{
			WSADATA data {};
			mbInitialized = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
			return mbInitialized;
		}

	private:
		bool mbInitialized = false;
	};

	class ScopedSocket
	{
	public:
		ScopedSocket() = default;
		~ScopedSocket()
		{
			Reset();
		}

		ScopedSocket(const ScopedSocket&) = delete;
		ScopedSocket& operator=(const ScopedSocket&) = delete;

		bool Create()
		{
			Reset();
			mSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			return mSocket != INVALID_SOCKET;
		}

		void Reset()
		{
			if (mSocket != INVALID_SOCKET)
			{
				::closesocket(mSocket);
				mSocket = INVALID_SOCKET;
			}
		}

		SOCKET mSocket = INVALID_SOCKET;
	};

	static void PrintUsage(std::ostream& rOutput)
	{
		rOutput << "Usage: AgentHarness.exe --owner TOKEN --port N [--timeout-ms 15000] -\n";
		rOutput << "       AgentHarness.exe --owner TOKEN --port N [--timeout-ms 15000] \"<json>\"\n";
		rOutput << "       AgentHarness.exe lock <token|claim|status|release|steal|heartbeat> ...\n";
		rOutput << "       AgentHarness.exe --help\n";
	}

	static bool ReadAllStandardInput(std::string& rInput)
	{
		char pBuffer[4'096] {};
		size_t uiRead = 0;
		while ((uiRead = std::fread(pBuffer, 1, sizeof(pBuffer), stdin)) > 0)
		{
			if (rInput.size() > kuiMaxRequestBytes || uiRead > kuiMaxRequestBytes - rInput.size())
			{
				Fail("request exceeds 1 MiB");
				return false;
			}
			rInput.append(pBuffer, uiRead);
		}
		if (std::ferror(stdin))
		{
			Fail("standard input read failed");
			return false;
		}
		return true;
	}

	static bool RefreshHeartbeatIfDue(std::wstring_view owner, std::chrono::steady_clock::time_point& rNextHeartbeatDue, std::optional<std::chrono::steady_clock::time_point> operationDeadline = std::nullopt)
	{
		if (std::chrono::steady_clock::now() < rNextHeartbeatDue)
		{
			return true;
		}
		if (operationDeadline)
		{
			int64_t iRemainingMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(*operationDeadline - std::chrono::steady_clock::now()).count();
			if (!RefreshHarnessHeartbeat(owner, iRemainingMilliseconds > 0 ? iRemainingMilliseconds : 0))
			{
				return false;
			}
		}
		else if (!RefreshHarnessHeartbeat(owner))
		{
			return false;
		}
		rNextHeartbeatDue = std::chrono::steady_clock::now() + std::chrono::milliseconds(kiHeartbeatIntervalMilliseconds);
		return true;
	}

	static SocketOperationResult CheckDeadlineAndRefreshHeartbeatAfterNoProgress(const std::chrono::steady_clock::time_point& rOperationDeadline, std::wstring_view owner, std::chrono::steady_clock::time_point& rNextHeartbeatDue)
	{
		if (std::chrono::steady_clock::now() >= rOperationDeadline)
		{
			return SocketOperationResult::kTransportFailure;
		}
		if (!RefreshHeartbeatIfDue(owner, rNextHeartbeatDue, rOperationDeadline))
		{
			return SocketOperationResult::kOwnershipLoss;
		}
		return SocketOperationResult::kSuccess;
	}

	static SocketOperationResult WaitForSocketReadiness(SOCKET socket, bool bWrite, const std::chrono::steady_clock::time_point& rOperationDeadline, std::wstring_view owner, std::chrono::steady_clock::time_point& rNextHeartbeatDue)
	{
		for (;;)
		{
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			if (now >= rOperationDeadline)
			{
				return SocketOperationResult::kTransportFailure;
			}
			if (now >= rNextHeartbeatDue)
			{
				if (!RefreshHeartbeatIfDue(owner, rNextHeartbeatDue, rOperationDeadline))
				{
					return SocketOperationResult::kOwnershipLoss;
				}
				continue;
			}

			std::chrono::milliseconds waitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(rOperationDeadline - now);
			std::chrono::milliseconds heartbeatDuration = std::chrono::duration_cast<std::chrono::milliseconds>(rNextHeartbeatDue - now);
			if (heartbeatDuration < waitDuration)
			{
				waitDuration = heartbeatDuration;
			}
			if (waitDuration > std::chrono::milliseconds(kiReadinessWaitCapMilliseconds))
			{
				waitDuration = std::chrono::milliseconds(kiReadinessWaitCapMilliseconds);
			}

			fd_set readSet {};
			FD_ZERO(&readSet);
			fd_set writeSet {};
			FD_ZERO(&writeSet);
			if (bWrite)
			{
				FD_SET(socket, &writeSet);
			}
			else
			{
				FD_SET(socket, &readSet);
			}
			timeval timeout {};
			timeout.tv_sec = static_cast<long>(waitDuration.count() / 1'000);
			timeout.tv_usec = static_cast<long>((waitDuration.count() % 1'000) * 1'000);
			int iReady = ::select(0, bWrite ? nullptr : &readSet, bWrite ? &writeSet : nullptr, nullptr, &timeout);
			if (iReady == SOCKET_ERROR)
			{
				return SocketOperationResult::kReadinessFailure;
			}
			if (iReady > 0)
			{
				std::chrono::steady_clock::time_point readyTime = std::chrono::steady_clock::now();
				if (readyTime >= rOperationDeadline)
				{
					return SocketOperationResult::kTransportFailure;
				}
				if (readyTime >= rNextHeartbeatDue)
				{
					if (!RefreshHeartbeatIfDue(owner, rNextHeartbeatDue, rOperationDeadline))
					{
						return SocketOperationResult::kOwnershipLoss;
					}
					continue;
				}
				return SocketOperationResult::kSuccess;
			}
		}
	}

	static SocketOperationResult SendAll(SOCKET socket, std::span<const char> data, int64_t iTimeoutMilliseconds, std::wstring_view owner, std::chrono::steady_clock::time_point& rNextHeartbeatDue)
	{
		std::chrono::steady_clock::time_point operationDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(iTimeoutMilliseconds);
		while (!data.empty())
		{
			SocketOperationResult eWaitResult = WaitForSocketReadiness(socket, true, operationDeadline, owner, rNextHeartbeatDue);
			if (eWaitResult != SocketOperationResult::kSuccess)
			{
				return eWaitResult;
			}
			int iChunk = ::send(socket, data.data(), static_cast<int>(data.size()), 0);
			int iSocketError = iChunk == SOCKET_ERROR ? ::WSAGetLastError() : 0;
			if (iChunk > 0)
			{
				data = data.subspan(static_cast<size_t>(iChunk));
				operationDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(iTimeoutMilliseconds);
				if (!RefreshHeartbeatIfDue(owner, rNextHeartbeatDue, operationDeadline))
				{
					return SocketOperationResult::kOwnershipLoss;
				}
				continue;
			}
			SocketOperationResult ePostOperationResult = CheckDeadlineAndRefreshHeartbeatAfterNoProgress(operationDeadline, owner, rNextHeartbeatDue);
			if (ePostOperationResult != SocketOperationResult::kSuccess)
			{
				return ePostOperationResult;
			}
			if (iChunk == SOCKET_ERROR && iSocketError == WSAEWOULDBLOCK)
			{
				continue;
			}
			return SocketOperationResult::kTransportFailure;
		}
		return SocketOperationResult::kSuccess;
	}

	static SocketOperationResult ReceiveAll(SOCKET socket, std::span<char> data, int64_t iTimeoutMilliseconds, std::wstring_view owner, std::chrono::steady_clock::time_point& rNextHeartbeatDue)
	{
		std::chrono::steady_clock::time_point operationDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(iTimeoutMilliseconds);
		while (!data.empty())
		{
			SocketOperationResult eWaitResult = WaitForSocketReadiness(socket, false, operationDeadline, owner, rNextHeartbeatDue);
			if (eWaitResult != SocketOperationResult::kSuccess)
			{
				return eWaitResult;
			}
			int iChunk = ::recv(socket, data.data(), static_cast<int>(data.size()), 0);
			int iSocketError = iChunk == SOCKET_ERROR ? ::WSAGetLastError() : 0;
			if (iChunk > 0)
			{
				data = data.subspan(static_cast<size_t>(iChunk));
				operationDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(iTimeoutMilliseconds);
				if (!RefreshHeartbeatIfDue(owner, rNextHeartbeatDue, operationDeadline))
				{
					return SocketOperationResult::kOwnershipLoss;
				}
				continue;
			}
			SocketOperationResult ePostOperationResult = CheckDeadlineAndRefreshHeartbeatAfterNoProgress(operationDeadline, owner, rNextHeartbeatDue);
			if (ePostOperationResult != SocketOperationResult::kSuccess)
			{
				return ePostOperationResult;
			}
			if (iChunk == SOCKET_ERROR && iSocketError == WSAEWOULDBLOCK)
			{
				continue;
			}
			return SocketOperationResult::kTransportFailure;
		}
		return SocketOperationResult::kSuccess;
	}

	static void FailSocketOperation(SocketOperationResult eResult, std::string_view transportFailure)
	{
		if (eResult == SocketOperationResult::kOwnershipLoss)
		{
			Fail("harness heartbeat refresh failed");
		}
		else if (eResult == SocketOperationResult::kReadinessFailure)
		{
			Fail("socket readiness wait failed");
		}
		else
		{
			Fail(transportFailure);
		}
	}

	static bool ConnectWithTimeout(SOCKET socket, const sockaddr_in& rAddress, int64_t iTimeoutMilliseconds)
	{
		u_long uiNonBlocking = 1;
		if (::ioctlsocket(socket, FIONBIO, &uiNonBlocking) != 0)
		{
			return false;
		}

		int iResult = ::connect(socket, reinterpret_cast<const sockaddr*>(&rAddress), sizeof(rAddress));
		if (iResult != 0)
		{
			if (::WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return false;
			}
			fd_set writeSet;
			FD_ZERO(&writeSet);
			FD_SET(socket, &writeSet);
			fd_set errorSet;
			FD_ZERO(&errorSet);
			FD_SET(socket, &errorSet);
			timeval timeout {};
			timeout.tv_sec = static_cast<long>(iTimeoutMilliseconds / 1'000);
			timeout.tv_usec = static_cast<long>((iTimeoutMilliseconds % 1'000) * 1'000);
			int iReady = ::select(0, nullptr, &writeSet, &errorSet, &timeout);
			if (iReady <= 0 || FD_ISSET(socket, &errorSet))
			{
				return false;
			}
			int iSocketError = 0;
			int iOptionLength = sizeof(iSocketError);
			if (::getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&iSocketError), &iOptionLength) != 0 || iSocketError != 0)
			{
				return false;
			}
		}

		u_long uiBlocking = 0;
		return ::ioctlsocket(socket, FIONBIO, &uiBlocking) == 0;
	}

	static bool ParseSocketCommandArguments(int iArgumentCount, wchar_t* const pArgumentValues[], SocketCommandArguments& rArguments)
	{
		bool bHaveRequestSource = false;

		for (int64_t i = 1; i < iArgumentCount; ++i)
		{
			std::wstring_view argument = pArgumentValues[i];
			if (argument == L"--port" || argument == L"--timeout-ms" || argument == L"--owner")
			{
				if (++i >= iArgumentCount)
				{
					Fail("socket option requires a value");
					PrintUsage(std::cerr);
					return false;
				}
				if (argument == L"--port")
				{
					wchar_t* pEnd = nullptr;
					rArguments.iPort = std::wcstoll(pArgumentValues[i], &pEnd, 10);
					if (pEnd == pArgumentValues[i] || *pEnd != L'\0')
					{
						Fail("--port must be in the range 1..65535");
						PrintUsage(std::cerr);
						return false;
					}
				}
				else if (argument == L"--timeout-ms")
				{
					wchar_t* pEnd = nullptr;
					rArguments.iTimeoutMilliseconds = std::wcstoll(pArgumentValues[i], &pEnd, 10);
					if (pEnd == pArgumentValues[i] || *pEnd != L'\0')
					{
						Fail("--timeout-ms must be in the range 1..600000");
						PrintUsage(std::cerr);
						return false;
					}
				}
				else
				{
					rArguments.owner = pArgumentValues[i];
				}
			}
			else if (argument == L"-")
			{
				if (bHaveRequestSource)
				{
					Fail("exactly one request JSON source is required");
					PrintUsage(std::cerr);
					return false;
				}
				rArguments.bReadStandardInput = true;
				bHaveRequestSource = true;
			}
			else
			{
				if (bHaveRequestSource)
				{
					Fail("exactly one request JSON source is required");
					PrintUsage(std::cerr);
					return false;
				}
				rArguments.inlineRequest = WideToUtf8(argument);
				bHaveRequestSource = true;
			}
		}

		if (rArguments.iPort <= 0 || rArguments.iPort > 65'535)
		{
			Fail("--port must be in the range 1..65535");
			PrintUsage(std::cerr);
			return false;
		}
		if (rArguments.iTimeoutMilliseconds <= 0 || rArguments.iTimeoutMilliseconds > 600'000)
		{
			Fail("--timeout-ms must be in the range 1..600000");
			PrintUsage(std::cerr);
			return false;
		}
		if (rArguments.owner.empty())
		{
			Fail("--owner is required");
			PrintUsage(std::cerr);
			return false;
		}
		if (!bHaveRequestSource)
		{
			Fail("no request JSON provided");
			PrintUsage(std::cerr);
			return false;
		}
		return true;
	}

	static std::optional<std::string> ExchangeFramedSocketCommand(ScopedSocket& rSocket, const SocketCommandArguments& rArguments, std::string_view request, std::chrono::steady_clock::time_point& rNextHeartbeatDue)
	{
		sockaddr_in address {};
		address.sin_family = AF_INET;
		address.sin_port = ::htons(static_cast<uint16_t>(rArguments.iPort));
		::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

		// Retry the connect until the overall --timeout-ms deadline so a peer whose listen socket is not yet
		// bound (freshly launched) is tolerated. Each try gets its own short timeout, and a failed non-blocking
		// connect leaves the socket unusable, so it is closed and recreated every attempt. A dead port therefore
		// consumes the full budget by design.
		std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point deadline = startTime + std::chrono::milliseconds(rArguments.iTimeoutMilliseconds);
		bool bConnected = false;
		bool bSocketCreateFailed = false;
		bool bHeartbeatFailed = false;
		for (;;)
		{
			if (std::chrono::steady_clock::now() >= deadline)
			{
				break;
			}
			if (!RefreshHeartbeatIfDue(rArguments.owner, rNextHeartbeatDue, deadline))
			{
				bHeartbeatFailed = true;
				break;
			}
			if (std::chrono::steady_clock::now() >= deadline)
			{
				break;
			}
			if (!rSocket.Create())
			{
				bSocketCreateFailed = true;
				break;
			}
			// A whole attempt timeout can outlast the remaining budget, so the last attempt is shortened to it.
			std::chrono::milliseconds remainingBeforeConnect = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
			if (remainingBeforeConnect.count() <= 0)
			{
				break;
			}
			std::chrono::milliseconds attemptTimeout(kiConnectAttemptTimeoutMilliseconds);
			if (remainingBeforeConnect < attemptTimeout)
			{
				attemptTimeout = remainingBeforeConnect;
			}
			if (ConnectWithTimeout(rSocket.mSocket, address, attemptTimeout.count()))
			{
				bConnected = true;
				break;
			}
			rSocket.Reset();
			if (std::chrono::steady_clock::now() >= deadline)
			{
				break;
			}
			if (!RefreshHeartbeatIfDue(rArguments.owner, rNextHeartbeatDue, deadline))
			{
				bHeartbeatFailed = true;
				break;
			}
			std::chrono::milliseconds retrySleep(kiConnectRetrySleepMilliseconds);
			std::chrono::milliseconds heartbeatSleep = std::chrono::duration_cast<std::chrono::milliseconds>(rNextHeartbeatDue - std::chrono::steady_clock::now());
			if (heartbeatSleep < retrySleep)
			{
				retrySleep = heartbeatSleep;
			}
			std::chrono::milliseconds remainingBeforeSleep = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
			if (remainingBeforeSleep < retrySleep)
			{
				retrySleep = remainingBeforeSleep;
			}
			if (retrySleep.count() > 0)
			{
				std::this_thread::sleep_for(retrySleep);
			}
		}
		if (!bConnected)
		{
			if (bHeartbeatFailed)
			{
				Fail("harness heartbeat refresh failed");
			}
			else if (bSocketCreateFailed)
			{
				Fail("socket creation failed");
			}
			else
			{
				int64_t iElapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
				Fail("connect to 127.0.0.1 failed or timed out after " + std::to_string(iElapsedMilliseconds) + " ms");
			}
			return std::nullopt;
		}
		u_long uiNonBlocking = 1;
		if (::ioctlsocket(rSocket.mSocket, FIONBIO, &uiNonBlocking) != 0)
		{
			Fail("could not configure connected socket for non-blocking I/O");
			return std::nullopt;
		}

		uint32_t uiPayloadLength = static_cast<uint32_t>(request.size());
		unsigned char pLengthPrefix[4] =
		{
			static_cast<unsigned char>(uiPayloadLength & 0xffu),
			static_cast<unsigned char>((uiPayloadLength >> 8) & 0xffu),
			static_cast<unsigned char>((uiPayloadLength >> 16) & 0xffu),
			static_cast<unsigned char>((uiPayloadLength >> 24) & 0xffu),
		};
		SocketOperationResult eSendResult = SendAll(rSocket.mSocket, std::span<const char>(reinterpret_cast<const char*>(pLengthPrefix), sizeof(pLengthPrefix)), rArguments.iTimeoutMilliseconds, rArguments.owner, rNextHeartbeatDue);
		if (eSendResult == SocketOperationResult::kSuccess)
		{
			eSendResult = SendAll(rSocket.mSocket, std::span<const char>(request), rArguments.iTimeoutMilliseconds, rArguments.owner, rNextHeartbeatDue);
		}
		if (eSendResult != SocketOperationResult::kSuccess)
		{
			FailSocketOperation(eSendResult, "send failed");
			return std::nullopt;
		}

		unsigned char pResponseLengthPrefix[4] {};
		SocketOperationResult eReceiveResult = ReceiveAll(rSocket.mSocket, std::span<char>(reinterpret_cast<char*>(pResponseLengthPrefix), sizeof(pResponseLengthPrefix)), rArguments.iTimeoutMilliseconds, rArguments.owner, rNextHeartbeatDue);
		if (eReceiveResult != SocketOperationResult::kSuccess)
		{
			FailSocketOperation(eReceiveResult, "no response (timed out or peer closed)");
			return std::nullopt;
		}
		uint32_t uiResponseLength = static_cast<uint32_t>(pResponseLengthPrefix[0]) |
			(static_cast<uint32_t>(pResponseLengthPrefix[1]) << 8) |
			(static_cast<uint32_t>(pResponseLengthPrefix[2]) << 16) |
			(static_cast<uint32_t>(pResponseLengthPrefix[3]) << 24);
		if (uiResponseLength == 0 || uiResponseLength > kuiMaxResponseBytes)
		{
			Fail("response length out of range");
			return std::nullopt;
		}

		std::string response(uiResponseLength, '\0');
		eReceiveResult = ReceiveAll(rSocket.mSocket, std::span<char>(response.data(), uiResponseLength), rArguments.iTimeoutMilliseconds, rArguments.owner, rNextHeartbeatDue);
		if (eReceiveResult != SocketOperationResult::kSuccess)
		{
			FailSocketOperation(eReceiveResult, "incomplete response (timed out or peer closed)");
			return std::nullopt;
		}
		return response;
	}

	static int PrintResponseAndInterpretExitCode(std::string_view response)
	{
		std::cout << response << '\n';
		try
		{
			nlohmann::json parsed = nlohmann::json::parse(response);
			if (parsed.contains("ok") && parsed["ok"].is_boolean())
			{
				return parsed["ok"].get<bool>() ? kiExitOk : kiExitStateConflict;
			}
			Fail("response missing boolean \"ok\" field");
		}
		catch (const std::exception& rException)
		{
			Fail(std::string("response is not valid JSON: ") + rException.what());
		}
		return kiExitFailure;
	}

	static int RunSocketCommand(int iArgumentCount, wchar_t* const pArgumentValues[])
	{
		try
		{
			SocketCommandArguments arguments {};
			if (!ParseSocketCommandArguments(iArgumentCount, pArgumentValues, arguments))
			{
				return kiExitFailure;
			}
			std::string request = arguments.inlineRequest;
			if (arguments.bReadStandardInput && !ReadAllStandardInput(request))
			{
				return kiExitFailure;
			}
			if (request.empty())
			{
				Fail("no request JSON provided");
				PrintUsage(std::cerr);
				return kiExitFailure;
			}
			if (request.size() > kuiMaxRequestBytes)
			{
				Fail("request exceeds 1 MiB");
				return kiExitFailure;
			}
			if (!RefreshHarnessHeartbeat(arguments.owner))
			{
				Fail("harness heartbeat refresh failed");
				return kiExitFailure;
			}
			std::chrono::steady_clock::time_point nextHeartbeatDue = std::chrono::steady_clock::now() + std::chrono::milliseconds(kiHeartbeatIntervalMilliseconds);

			ScopedWindowsSockets windowsSockets;
			if (!windowsSockets.Initialize())
			{
				Fail("WSAStartup failed");
				return kiExitFailure;
			}
			ScopedSocket socket;
			std::optional<std::string> response = ExchangeFramedSocketCommand(socket, arguments, request, nextHeartbeatDue);
			if (!response)
			{
				return kiExitFailure;
			}
			if (!RefreshHarnessHeartbeat(arguments.owner))
			{
				Fail("harness heartbeat refresh failed");
				return kiExitFailure;
			}
			return PrintResponseAndInterpretExitCode(*response);
		}
		catch (const std::exception&)
		{
			Fail("socket command failed");
			return kiExitFailure;
		}
		catch (...)
		{
			Fail("socket command failed with an unknown exception");
			return kiExitFailure;
		}
	}
} // namespace toolcli

int wmain(int iArgumentCount, wchar_t* pArgumentValues[])
{
	toolcli::SetToolName("AgentHarness");
	if (iArgumentCount >= 2)
	{
		std::wstring_view mode = pArgumentValues[1];
		if (mode == L"--help")
		{
			if (iArgumentCount != 2)
			{
				toolcli::Fail("--help accepts no arguments");
				return toolcli::kiExitFailure;
			}
			toolcli::PrintUsage(std::cout);
			return toolcli::kiExitOk;
		}
		if (mode == L"lock")
		{
			return toolcli::RunHarnessLockCommand(std::span<const wchar_t* const>(pArgumentValues, static_cast<size_t>(iArgumentCount)));
		}
	}
	return toolcli::RunSocketCommand(iArgumentCount, pArgumentValues);
}
