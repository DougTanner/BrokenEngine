#pragma once

class ExportJob
{
public:

	// Version magic number for export format validation
	static constexpr int64_t kiMagic = 0xDA7ACCCC;

	// Folds in the chunk-header size so a header-layout change auto-invalidates every cache.
	static constexpr int64_t Version(int64_t iRaw) { return iRaw + sizeof(common::ChunkHeader); }

	ExportJob(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile, int64_t iVersion);
	ExportJob(ExportJob&& rToMove) noexcept = default;
	ExportJob& operator=(ExportJob&& rToMove) noexcept = default;
	virtual ~ExportJob() = default;

	ExportJob() = delete;
	ExportJob(const ExportJob& rToCopy) = delete;
	ExportJob& operator=(const ExportJob& rToCopy) = delete;

	virtual bool CheckDirty(const std::filesystem::path& rPackFile);
	std::vector<std::byte>& RunExport();
	bool ReadCachedChunk(std::vector<std::byte>& rHeaderAndData) const;

	int64_t miId = 0;
	int64_t miVersion = 0;
	bool mbDirty = false;
	common::ChunkFlags_t mChunkFlags;

	std::filesystem::path mInputPath;
	std::filesystem::path mRelativeDirectory;
	std::string mRelativeFile;
	std::filesystem::path mChunkFile;
	std::filesystem::path mCacheMetadataFile;
	// Set by SetContentKey; empty until CheckDirty on a job whose constructor sets no key, and after a failed fingerprint.
	std::string mContentKey;

	std::future<std::vector<std::byte>&> mFuture;

	common::crc_t mCrc = 0;
	// The chunk body CRC CheckDirty read from the .meta; meaningful on a clean job.
	common::crc_t muiCachedChunkCrc = 0;

protected:

	virtual void Export() = 0;
	virtual void CleanupOnFailure() {}
	virtual std::string GetInputFingerprint() const;
	virtual void UpdateCacheMetadata() {}

	// Called from a derived constructor or from CheckDirty, before any cache entry is touched. The key holds content
	// fingerprints only, so trees with equal inputs resolve to one entry and trees with different inputs keep separate
	// entries.
	void SetContentKey(common::crc_t uiContentKey);

	// Output-freshness marker sidecar, shared by the jobs whose generated outputs are tracked in the checkout.
	// Plain text (no magic / version prefix): the stored bytes are the whole comparison, and the caller puts its
	// own version inside the fingerprint string, so a stale marker can never compare equal.
	static std::optional<std::string> ReadMarkerFile(const std::filesystem::path& rPath);
	static void WriteMarkerFile(const std::filesystem::path& rPath, std::string_view fingerprint);

	std::tuple<common::ChunkHeader*, std::span<std::byte>> AllocateHeaderAndData(int64_t iDataSize);

	std::vector<std::byte> mHeaderAndData;
};

template <typename T>
concept IsExportJob =
	std::derived_from<T, ExportJob> &&
	requires
	{
		{ T::kName } -> std::convertible_to<std::string_view>;
	} &&
	requires (const std::filesystem::directory_entry& rEntry)
	{
		{ T::Handles(rEntry) } -> std::same_as<std::optional<common::ChunkFlags_t>>;
	};
