#pragma once

// The PCH-less AgentTools compile this header by path, so it includes no header: its includer supplies
// <windows.h>, then <bcrypt.h>, plus <array>, <cstddef>, <cstdint>, and <span>, through Pch.h or ToolCliCommon.h.

namespace common
{

// SHA-256 over Windows CNG. A construction or Update failure persists, and Finish reports it.
class Sha256Hasher
{
public:

	Sha256Hasher()
	{
		// The SHA-256 pseudo-handle needs no provider open or close, and CNG allocates the hash object itself.
		mbValid = BCRYPT_SUCCESS(::BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &mpHash, nullptr, 0, nullptr, 0, 0));
	}

	~Sha256Hasher()
	{
		if (mpHash != nullptr)
		{
			::BCryptDestroyHash(mpHash);
		}
	}

	Sha256Hasher(const Sha256Hasher&) = delete;
	Sha256Hasher& operator=(const Sha256Hasher&) = delete;

	// CNG takes a ULONG size, so each call passes fewer than 2^32 bytes.
	void Update(std::span<std::byte> bytes)
	{
		if (mbValid && !bytes.empty())
		{
			mbValid = BCRYPT_SUCCESS(::BCryptHashData(mpHash, reinterpret_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0));
		}
	}

	[[nodiscard]] bool Finish(std::array<uint8_t, 32>& rDigest)
	{
		return mbValid && BCRYPT_SUCCESS(::BCryptFinishHash(mpHash, rDigest.data(), static_cast<ULONG>(rDigest.size()), 0));
	}

private:

	BCRYPT_HASH_HANDLE mpHash = nullptr;
	bool mbValid = false;
};

} // namespace common
