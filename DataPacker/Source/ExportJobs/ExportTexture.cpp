#include "ExportTexture.h"

#include "Texture/Texture.h"
#include "FileManager.h"

std::optional<common::ChunkFlags_t> ExportTexture::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	if (HasCubemapTag(rDirectoryEntry.path()))
	{
		return common::ChunkFlags_t({common::ChunkFlags::kTexture, common::ChunkFlags::kCubemap});
	}

	static constexpr std::string_view kpExtensions[] = {".png", ".tga", ".jpg", ".ktx", TextureIntermediateSuffix(VK_FORMAT_BC4_UNORM_BLOCK), TextureIntermediateSuffix(VK_FORMAT_BC5_UNORM_BLOCK), TextureIntermediateSuffix(VK_FORMAT_BC7_UNORM_BLOCK), TextureIntermediateSuffix(VK_FORMAT_R16_UNORM), TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT)};
	std::string extension = rDirectoryEntry.path().extension().string();
	return std::ranges::contains(kpExtensions, extension) ? std::optional<common::ChunkFlags_t>(common::ChunkFlags::kTexture) : std::nullopt;
}

static int64_t ComputeUncompressedTextureSize(VkFormat vkFormat, int64_t iWidth, int64_t iHeight, int64_t iMipLevels)
{
	return common::ComputeImageByteSize(vkFormat, iWidth, iHeight, iMipLevels, 1, 1);
}

void ExportTexture::Export()
{
	std::string inputExtension = mInputPath.extension().string();
	bool bCompressedPassthrough = inputExtension == TextureIntermediateSuffix(VK_FORMAT_BC4_UNORM_BLOCK)
	 || inputExtension == TextureIntermediateSuffix(VK_FORMAT_BC5_UNORM_BLOCK)
	 || inputExtension == TextureIntermediateSuffix(VK_FORMAT_BC7_UNORM_BLOCK);
	if (gpFileManager->mbForbidExpensiveExport && !bCompressedPassthrough)
	{
		throw std::runtime_error("Texture export blocked by BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1");
	}

	// Format tokens live in either the file extension (explicit raw-intermediate formats) or the
	// filename (the [BC4]/[BC5]/[BC7] encode tags). Matching against extension()/filename() — not
	// the full path — keeps a parent directory whose name happens to contain a token (e.g. a folder
	// named "Foo.ktx") from misrouting every file beneath it.
	std::wstring extension = mInputPath.extension().native();
	std::wstring filename = mInputPath.filename().native();

	// Narrow copy of the extension for matching the canonical texture-intermediate suffixes
	// (TextureIntermediateSuffix returns narrow const char*) so this consumer can't drift from the
	// producers; the [BC4]/[BC5]/[BC7] encode-tag checks and the .ktx test below stay on the wide
	// strings. The suffixes are ASCII, so the narrow comparison is exact.
	std::string narrowExtension = mInputPath.extension().string();

	// bRawTexture covers only the explicit-extension formats (the already-encoded intermediates that
	// pass straight through). A [BC4]-tagged source resolves the BC4 format below but must still take
	// the encode path, so it is deliberately excluded here.
	bool bRawTexture = narrowExtension == TextureIntermediateSuffix(VK_FORMAT_R16_UNORM) || narrowExtension == TextureIntermediateSuffix(VK_FORMAT_BC4_UNORM_BLOCK) || narrowExtension == TextureIntermediateSuffix(VK_FORMAT_BC5_UNORM_BLOCK) || narrowExtension == TextureIntermediateSuffix(VK_FORMAT_BC7_UNORM_BLOCK) || narrowExtension == TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT);

	VkFormat vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
	if (narrowExtension == TextureIntermediateSuffix(VK_FORMAT_R16_UNORM))
	{
		vkFormat = VK_FORMAT_R16_UNORM;
	}
	else if (narrowExtension == TextureIntermediateSuffix(VK_FORMAT_BC4_UNORM_BLOCK) || filename.contains(L"[BC4]"))
	{
		vkFormat = VK_FORMAT_BC4_UNORM_BLOCK;
	}
	else if (narrowExtension == TextureIntermediateSuffix(VK_FORMAT_BC5_UNORM_BLOCK) || filename.contains(L"[BC5]"))
	{
		vkFormat = VK_FORMAT_BC5_UNORM_BLOCK;
	}
	else if (narrowExtension == TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT))
	{
		vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	}
	else if (mChunkFlags & common::ChunkFlags::kCubemap || narrowExtension == TextureIntermediateSuffix(VK_FORMAT_BC7_UNORM_BLOCK) || filename.contains(L"[BC7]"))
	{
		vkFormat = VK_FORMAT_BC7_UNORM_BLOCK;
	}

	// Dispatch order is load-bearing: .ktx and raw passthrough win over the cubemap flag, so a
	// [C]-tagged raw intermediate (e.g. the IBL .R16G16B16A16_SFLOAT outputs) stays on the raw path.
	if (extension == L".ktx")
	{
		ProcessKtxCubemap();
	}
	else if (bRawTexture)
	{
		ProcessRawTexture(vkFormat);
	}
	else if (mChunkFlags & common::ChunkFlags::kCubemap)
	{
		ProcessLiveCubemap(vkFormat);
	}
	else
	{
		ProcessRegularTexture(vkFormat);
	}
}

void ExportTexture::ProcessKtxCubemap()
{
	gli::texture texture = LoadGliFromPath(mInputPath);
	ASSERT(!texture.empty() && texture.target() == gli::TARGET_CUBE);
	ASSERT(mChunkFlags & common::ChunkFlags::kCubemap);

	gli::texture_cube textureCube(texture);
	ASSERT(textureCube.format() == gli::FORMAT_RGBA16_SFLOAT_PACK16);

	int64_t iUncompressedSize = static_cast<int64_t>(textureCube.size());
	std::vector<std::byte> compressed = Lz4Compress(std::span<const std::byte>(static_cast<const std::byte*>(textureCube.data()), static_cast<size_t>(iUncompressedSize)));
	mChunkFlags.Set(common::ChunkFlags::kLz4Compressed);

	auto [pHeader, dataSpan] = AllocateHeaderAndData(std::ssize(compressed));
	pHeader->iUncompressedSize = iUncompressedSize;
	pHeader->textureHeader.iTextureWidth = textureCube[0].extent().x;
	pHeader->textureHeader.iTextureHeight = textureCube[0].extent().y;
	pHeader->textureHeader.iMipLevels = textureCube.levels();
	pHeader->textureHeader.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	std::memcpy(dataSpan.data(), compressed.data(), static_cast<size_t>(std::ssize(compressed)));
}

void ExportTexture::ProcessRawTexture(VkFormat vkFormat)
{
	std::vector<std::byte> fileBytes = common::ReadEntireFile(mInputPath);
	int64_t iFileSize = std::ssize(fileBytes);
	TextureIntermediateHeader header = ReadTextureIntermediateHeader(fileBytes);
	int64_t iWidth = header.iWidth;
	int64_t iHeight = header.iHeight;
	int64_t iMipMaps = header.iMipCount;

	// Trust boundary: the intermediate is an opaque file and the shared parser locates the payload
	// without judging it, so bound the header and the declared extents here — before the payload
	// iterators below, ComputeImageByteSize's per-mip loop, and the inflate buffer allocation.
	if (iFileSize <= header.iPayloadOffset)
	{
		throw std::runtime_error(std::format("Texture intermediate \"{}\" is {} bytes; expected more than its {}-byte header.", mInputPath.string(), iFileSize, header.iPayloadOffset));
	}

	// 16384 / 15 match TextureUploadManager::ValidateTextureDimensions' fixed pack ceilings. Runtime also
	// validates the actual mip chain and selected device capabilities; these bounds keep every zlib-path mip
	// chain inside uLongf.
	if (iWidth <= 0 || iHeight <= 0 || iMipMaps <= 0 || iWidth > 16'384 || iHeight > 16'384 || iMipMaps > 15)
	{
		throw std::runtime_error(std::format("Texture intermediate \"{}\" declares invalid dimensions {}x{} with {} mips.", mInputPath.string(), iWidth, iHeight, iMipMaps));
	}

	std::vector<std::byte> data(fileBytes.begin() + header.iPayloadOffset, fileBytes.end());

	// Texture .pack chunks are LZ4-compressed (the runtime FileManager LZ4-decompresses them). Neither
	// raw-passthrough intermediate is LZ4 on disk, so both transcode into an LZ4 chunk here:
	//   * BCn / R16 intermediates from Texture::Save are zlib streams on disk —
	//     zlib-inflate to raw bytes, then LZ4-compress. Uncompressed size is the 2D
	//     mip-chain byte count derived from the intermediate dims.
	//   * .R16G16B16A16_SFLOAT cubemap intermediates from Generate{Irradiance,PreFiltered}Cubemaps are
	//     raw half-float pixels with 6 cube faces packed in — LZ4-compress directly, using the on-disk
	//     payload size (already 6 faces) instead of the 2D-only ComputeUncompressedTextureSize math.
	bool bRawHalfFloat = vkFormat == VK_FORMAT_R16G16B16A16_SFLOAT;
	int64_t iUncompressedSize = bRawHalfFloat
		? std::ssize(data)
		: ComputeUncompressedTextureSize(vkFormat, iWidth, iHeight, iMipMaps);

	// Lz4Compress narrows the source size to int, so bound the derived size here: the inflate buffer
	// and the LZ4 call never see a size outside LZ4's input domain.
	if (iUncompressedSize > LZ4_MAX_INPUT_SIZE)
	{
		throw std::runtime_error(std::format("Texture intermediate \"{}\" derives a {}-byte payload; LZ4 accepts at most {} bytes.", mInputPath.string(), iUncompressedSize, LZ4_MAX_INPUT_SIZE));
	}

	if (bRawHalfFloat)
	{
		// Half-float intermediates are only ExportCubemapIbl's raw six-face output, so the payload must be
		// exactly the declared six-face mip chain.
		int64_t iExpectedSize = common::ComputeImageByteSize(vkFormat, iWidth, iHeight, iMipMaps, 6, 1);
		if (iUncompressedSize != iExpectedSize)
		{
			throw std::runtime_error(std::format("Texture intermediate \"{}\" payload is {} bytes; expected {} bytes for {}x{} with {} mips across 6 faces.", mInputPath.string(), iUncompressedSize, iExpectedSize, iWidth, iHeight, iMipMaps));
		}
	}

	std::vector<std::byte> inflated;
	if (!bRawHalfFloat)
	{
		// zlib intermediate payload is opaque input; inflate into a fixed-size buffer and assert an exact
		// fill (DataPacker has no soft-fail path — a mismatch is a producer bug, not corrupt shipped data).
		inflated.resize(static_cast<size_t>(iUncompressedSize));
		uLongf uiInflatedSize = static_cast<uLongf>(iUncompressedSize);
		int64_t iZlibResult = uncompress(reinterpret_cast<Bytef*>(inflated.data()), &uiInflatedSize, reinterpret_cast<const Bytef*>(data.data()), static_cast<uLong>(std::ssize(data)));
		ASSERT(iZlibResult == Z_OK && static_cast<int64_t>(uiInflatedSize) == iUncompressedSize);
	}
	const std::vector<std::byte>& rRawBytes = bRawHalfFloat ? data : inflated;

	std::vector<std::byte> compressed = Lz4Compress(rRawBytes);
	mChunkFlags.Set(common::ChunkFlags::kLz4Compressed);

	auto [pHeader, dataSpan] = AllocateHeaderAndData(std::ssize(compressed));
	pHeader->iUncompressedSize = iUncompressedSize;
	pHeader->textureHeader.iTextureWidth = iWidth;
	pHeader->textureHeader.iTextureHeight = iHeight;
	pHeader->textureHeader.iMipLevels = iMipMaps;
	pHeader->textureHeader.vkFormat = vkFormat;

	std::memcpy(dataSpan.data(), compressed.data(), static_cast<size_t>(std::ssize(compressed)));
}

void ExportTexture::ProcessLiveCubemap(VkFormat vkFormat)
{
	int64_t iWidth = 0;
	int64_t iHeight = 0;
	std::vector<std::byte> data;

	static constexpr const char* kpcPngFaceNames[6] = {"px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"};
	static constexpr const char* kpcJpgFaceNames[6] = {"posx.jpg", "negx.jpg", "posy.jpg", "negy.jpg", "posz.jpg", "negz.jpg"};
	const char* const* pFaceNames = std::filesystem::exists(mInputPath / kpcPngFaceNames[0]) ? kpcPngFaceNames : kpcJpgFaceNames;
	{
		std::lock_guard<std::mutex> lock(Texture::sEncodeMutex);
		for (int64_t i = 0; i < 6; ++i)
		{
			Texture texture(mInputPath / pFaceNames[i], FileType::kImage);
			if (i == 0)
			{
				iWidth = texture.miWidth;
				iHeight = texture.miHeight;
				if (iWidth != iHeight)
				{
					throw std::runtime_error(std::format("Live cubemap \"{}\" face \"{}\" has dimensions {}x{}; required a square face.", mInputPath.string(), pFaceNames[i], iWidth, iHeight));
				}
			}
			else if (texture.miWidth != iWidth || texture.miHeight != iHeight)
			{
				throw std::runtime_error(std::format("Live cubemap \"{}\" face \"{}\" has dimensions {}x{}; required {}x{} to match face \"{}\".", mInputPath.string(), pFaceNames[i], texture.miWidth, texture.miHeight, iWidth, iHeight, pFaceNames[0]));
			}
			texture.Export(data, vkFormat, TextureOptions::kVerifyNoAlpha);
		}
	}

	int64_t iUncompressedSize = std::ssize(data);
	std::vector<std::byte> compressed = Lz4Compress(std::span<const std::byte>(data.data(), static_cast<size_t>(iUncompressedSize)));
	mChunkFlags.Set(common::ChunkFlags::kLz4Compressed);

	auto [pHeader, dataSpan] = AllocateHeaderAndData(std::ssize(compressed));
	pHeader->iUncompressedSize = iUncompressedSize;
	pHeader->textureHeader.iTextureWidth = iWidth;
	pHeader->textureHeader.iTextureHeight = iHeight;
	pHeader->textureHeader.iMipLevels = 1;
	pHeader->textureHeader.vkFormat = vkFormat;
	std::memcpy(dataSpan.data(), compressed.data(), static_cast<size_t>(std::ssize(compressed)));
}

void ExportTexture::ProcessRegularTexture(VkFormat vkFormat)
{
	std::lock_guard<std::mutex> lock(Texture::sEncodeMutex);
	Texture texture(mInputPath, FileType::kImage);
	texture.MakeMipmaps(vkFormat);

	std::vector<std::byte> data = texture.Export(vkFormat, {});

	int64_t iUncompressedSize = std::ssize(data);
	std::vector<std::byte> compressed = Lz4Compress(std::span<const std::byte>(data.data(), static_cast<size_t>(iUncompressedSize)));
	mChunkFlags.Set(common::ChunkFlags::kLz4Compressed);

	auto [pHeader, dataSpan] = AllocateHeaderAndData(std::ssize(compressed));
	pHeader->iUncompressedSize = iUncompressedSize;
	pHeader->textureHeader.iTextureWidth = texture.miWidth;
	pHeader->textureHeader.iTextureHeight = texture.miHeight;
	pHeader->textureHeader.iMipLevels = std::ssize(texture.mData);
	pHeader->textureHeader.vkFormat = vkFormat;
	std::memcpy(dataSpan.data(), compressed.data(), static_cast<size_t>(std::ssize(compressed)));
}
