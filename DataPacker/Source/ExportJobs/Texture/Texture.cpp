#include "Texture.h"

#include "FileManager.h"

inline constexpr int64_t kiBc45SearchRadius = 5;
inline constexpr int64_t kiBc7MaxPartitions = 64;
inline constexpr int64_t kiBc7UberLevel = 4;

// Tracks how many threads are inside EncodeBlocks at once. The caller-held sEncodeMutex must
// keep this at 0 or 1 — anything higher means a call site forgot to take the lock.
static std::atomic<int64_t> siActiveEncodeCount(0);

void Texture::StaticInitialize()
{
	rgbcx::init();
	bc7enc_compress_block_init();
}

Texture::Texture(const std::filesystem::path& rPath, FileType eFileType, int64_t iWidth, int64_t iHeight)
: miWidth(iWidth)
, miHeight(iHeight)
{
	if (eFileType == FileType::kImage)
	{
		LoadImage(rPath);
	}
	else if (eFileType == FileType::kFloat32)
	{
		LoadFloat32(rPath);
	}
	else if (eFileType == FileType::kUint16Raw)
	{
		LoadUint16Raw(rPath);
	}
	else
	{
		LoadExr(rPath);
	}
}

void Texture::LoadImage(const std::filesystem::path& rPath)
{
	int iStbiWidth = 0;
	int iStbiHeight = 0;
	int iChannelsInFile = 0;
	stbi_uc* puiPixels = stbi_load(reinterpret_cast<const char*>(rPath.u8string().c_str()), &iStbiWidth, &iStbiHeight, &iChannelsInFile, STBI_rgb_alpha);
	common::ScopedLambda freeStbiPixels([=]()
	{
		stbi_image_free(puiPixels);
	});
	ASSERT(iStbiWidth != 0 && iStbiHeight != 0 && puiPixels != nullptr);
	miWidth = iStbiWidth;
	miHeight = iStbiHeight;

	stbi_uc* puiSource = puiPixels;
	std::vector<float>& rPixels = mData.emplace_back(static_cast<size_t>(4 * miWidth * miHeight));
	float* pfDestination = rPixels.data();
	for (int64_t j = 0; j < miHeight; ++j)
	{
		for (int64_t i = 0; i < miWidth; ++i)
		{
			pfDestination[0] = puiSource[0];
			pfDestination[1] = puiSource[1];
			pfDestination[2] = puiSource[2];
			pfDestination[3] = puiSource[3];

			puiSource += 4;
			pfDestination += 4;
		}
	}
}

void Texture::LoadFloat32(const std::filesystem::path& rPath)
{
	ASSERT(miWidth > 0 && miHeight > 0);

	std::vector<std::byte> data = common::ReadEntireFile(rPath);

	float* pfSourceRed = reinterpret_cast<float*>(data.data());
	std::vector<float>& rPixels = mData.emplace_back(4 * miWidth * miHeight);
	float* pfDestination = rPixels.data();
	for (int64_t j = 0; j < miHeight; ++j)
	{
		for (int64_t i = 0; i < miWidth; ++i)
		{
			pfDestination[0] = 255.0f * pfSourceRed[0];
			pfDestination[1] = 0.0f;
			pfDestination[2] = 0.0f;
			pfDestination[3] = 0.0f;

			++pfSourceRed;
			pfDestination += 4;
		}
	}
}

void Texture::LoadUint16Raw(const std::filesystem::path& rPath)
{
	// Headerless linear unorm-16. Gaea's UshortRaw16 format. No gamma applies — source is
	// already linear. Single-channel; R is populated, GBA left at 0 (matches kFloat32).
	ASSERT(miWidth > 0 && miHeight > 0);
	if (miWidth <= 0 || miHeight <= 0)
	{
		throw std::runtime_error("Uint16 raw texture dimensions are invalid");
	}

	uintmax_t uiWidth = static_cast<uintmax_t>(miWidth);
	uintmax_t uiHeight = static_cast<uintmax_t>(miHeight);
	static constexpr int64_t kiBytesPerPixel = sizeof(uint16_t);
	if (uiWidth > std::numeric_limits<uintmax_t>::max() / uiHeight)
	{
		throw std::runtime_error("Uint16 raw texture dimensions overflow");
	}
	uintmax_t uiPixelCount = uiWidth * uiHeight;
	if (uiPixelCount > std::numeric_limits<uintmax_t>::max() / kiBytesPerPixel)
	{
		throw std::runtime_error("Uint16 raw texture byte count overflow");
	}
	uintmax_t uiExpectedBytes = uiPixelCount * kiBytesPerPixel;

	// Headerless: the file length is the only shape check available. Checked before the read because
	// ReadEntireFile sizes its allocation from the on-disk length.
	if (std::filesystem::file_size(rPath) != uiExpectedBytes)
	{
		throw std::runtime_error("Uint16 raw texture byte count does not match dimensions");
	}

	std::vector<std::byte> data = common::ReadEntireFile(rPath);
	if (static_cast<uintmax_t>(std::ssize(data)) != uiExpectedBytes)
	{
		throw std::runtime_error("Uint16 raw texture byte count does not match dimensions");
	}

	const uint16_t* puiSource = reinterpret_cast<const uint16_t*>(data.data());
	std::vector<float>& rPixels = mData.emplace_back(4 * miWidth * miHeight);
	float* pfDestination = rPixels.data();
	for (int64_t j = 0; j < miHeight; ++j)
	{
		for (int64_t i = 0; i < miWidth; ++i)
		{
			pfDestination[0] = 255.0f * common::UnsignedNormalizedIntegerToFloat<uint16_t>(puiSource[0]);
			pfDestination[1] = 0.0f;
			pfDestination[2] = 0.0f;
			pfDestination[3] = 0.0f;

			++puiSource;
			pfDestination += 4;
		}
	}
}

// OpenEXR reports every failure through a result code, so a discarded result publishes an
// incompletely decoded image that looks valid downstream.
static void CheckExrResult(int64_t iExrResult, const std::filesystem::path& rPath, std::string_view call)
{
	if (iExrResult != EXR_ERR_SUCCESS)
	{
		throw std::runtime_error(std::format("{} failed for EXR \"{}\": {}", call, rPath.string(), exr_get_default_error_message(static_cast<exr_result_t>(iExrResult))));
	}
}

void Texture::LoadExr(const std::filesystem::path& rPath)
{
	exr_context_initializer_t exrContextInitializer = EXR_DEFAULT_CONTEXT_INITIALIZER;
	exr_context_t pExrContext {};
	int64_t iExrResult = exr_start_read(&pExrContext, reinterpret_cast<const char*>(rPath.u8string().c_str()), &exrContextInitializer);
	CheckExrResult(iExrResult, rPath, "exr_start_read");
	common::ScopedLambda releaseExrContext([=]()
	{
		exr_context_t pExrContextCopy = pExrContext;
		exr_finish(&pExrContextCopy);
	});

	exr_attr_box2i_t dataWindow {};
	CheckExrResult(exr_get_data_window(pExrContext, 0, &dataWindow), rPath, "exr_get_data_window");
	int32_t iScansPerChunk = 0;
	CheckExrResult(exr_get_scanlines_per_chunk(pExrContext, 0, &iScansPerChunk), rPath, "exr_get_scanlines_per_chunk");
	int64_t iScanlinesPerChunk = iScansPerChunk;
	ASSERT(iScanlinesPerChunk == 1);

	miWidth = dataWindow.max.x + 1;
	miHeight = dataWindow.max.y + 1;
	std::vector<float> pixelsR(miWidth * miHeight);
	std::vector<float> pixelsG(miWidth * miHeight);
	std::vector<float> pixelsB(miWidth * miHeight);

	for (int64_t i = dataWindow.min.y; i <= dataWindow.max.y; i += iScanlinesPerChunk)
	{
		exr_chunk_info_t exrChunkInfo {};
		CheckExrResult(exr_read_scanline_chunk_info(pExrContext, 0, static_cast<int>(i), &exrChunkInfo), rPath, "exr_read_scanline_chunk_info");

		exr_decode_pipeline_t decoder {};
		CheckExrResult(exr_decoding_initialize(pExrContext, 0, &exrChunkInfo, &decoder), rPath, "exr_decoding_initialize");

		if (decoder.channel_count != 3)
		{
			exr_decoding_destroy(pExrContext, &decoder);
			throw std::runtime_error(std::format("EXR \"{}\" expected 3 channels, found {}.", rPath.string(), decoder.channel_count));
		}

		decoder.channels[0].user_data_type = EXR_PIXEL_FLOAT;
		decoder.channels[0].decode_to_ptr = reinterpret_cast<uint8_t*>(pixelsB.data() + i * miWidth);
		decoder.channels[0].user_pixel_stride = 4;
		decoder.channels[0].user_line_stride = static_cast<int32_t>(4 * miWidth);
		decoder.channels[0].user_bytes_per_element = 4;

		decoder.channels[1].user_data_type = EXR_PIXEL_FLOAT;
		decoder.channels[1].decode_to_ptr = reinterpret_cast<uint8_t*>(pixelsG.data() + i * miWidth);
		decoder.channels[1].user_pixel_stride = 4;
		decoder.channels[1].user_line_stride = static_cast<int32_t>(4 * miWidth);
		decoder.channels[1].user_bytes_per_element = 4;

		decoder.channels[2].user_data_type = EXR_PIXEL_FLOAT;
		decoder.channels[2].decode_to_ptr = reinterpret_cast<uint8_t*>(pixelsR.data() + i * miWidth);
		decoder.channels[2].user_pixel_stride = 4;
		decoder.channels[2].user_line_stride = static_cast<int32_t>(4 * miWidth);
		decoder.channels[2].user_bytes_per_element = 4;

		// The initialized decoder holds intermediate buffers, so it must be destroyed before either failure throws.
		const char* pcLastCall = "exr_decoding_choose_default_routines";
		iExrResult = exr_decoding_choose_default_routines(pExrContext, 0, &decoder);
		if (iExrResult == EXR_ERR_SUCCESS)
		{
			pcLastCall = "exr_decoding_run";
			iExrResult = exr_decoding_run(pExrContext, 0, &decoder);
		}
		exr_decoding_destroy(pExrContext, &decoder);
		CheckExrResult(iExrResult, rPath, pcLastCall);
	}

	float* pfSourceRed = pixelsR.data();
	float* pfSourceGreen = pixelsG.data();
	float* pfSourceBlue = pixelsB.data();
	std::vector<float>& rPixels = mData.emplace_back(4 * miWidth * miHeight);
	float* pfDestination = rPixels.data();
	for (int64_t j = 0; j < miHeight; ++j)
	{
		for (int64_t i = 0; i < miWidth; ++i)
		{
			pfDestination[0] = 255.0f * pfSourceRed[0];
			pfDestination[1] = 255.0f * pfSourceGreen[0];
			pfDestination[2] = 255.0f * pfSourceBlue[0];
			pfDestination[3] = 255.0f;

			++pfSourceRed;
			++pfSourceGreen;
			++pfSourceBlue;
			pfDestination += 4;
		}
	}
}

Texture::Texture(const std::byte* puiPixels, int64_t iWidth, int64_t iHeight, int64_t iStride)
: miWidth(iWidth)
, miHeight(iHeight)
{
	std::vector<float>& rPixels = mData.emplace_back(4 * miWidth * miHeight);
	float* pfDestination = rPixels.data();
	for (int64_t j = 0; j < miHeight; ++j)
	{
		for (int64_t i = 0; i < miWidth; ++i)
		{
			pfDestination[0] = static_cast<float>(std::to_integer<int64_t>(puiPixels[0]));
			pfDestination[1] = static_cast<float>(std::to_integer<int64_t>(puiPixels[1]));
			pfDestination[2] = static_cast<float>(std::to_integer<int64_t>(puiPixels[2]));
			pfDestination[3] = static_cast<float>(std::to_integer<int64_t>(iStride == 4 ? puiPixels[3] : std::byte {255}));

			puiPixels += iStride;
			pfDestination += 4;
		}
	}
}

void Texture::MakeMipmaps(VkFormat vkFormat, int64_t iMaxLevel, int64_t iPreviousLevel, int64_t iPreviousWidth, int64_t iPreviousHeight)
{
	int64_t iLevel = iPreviousLevel;
	int64_t iSourceWidth = iPreviousWidth;
	int64_t iSourceHeight = iPreviousHeight;

	while (iSourceWidth > 1 && iSourceHeight > 1 && iLevel + 1 < iMaxLevel)
	{
		int64_t iDestinationWidth = std::max(iSourceWidth / 2, 1i64);
		int64_t iDestinationHeight = std::max(iSourceHeight / 2, 1i64);

		if (vkFormat == VK_FORMAT_BC4_UNORM_BLOCK || vkFormat == VK_FORMAT_BC5_UNORM_BLOCK || vkFormat == VK_FORMAT_BC7_UNORM_BLOCK)
		{
			if (iDestinationWidth < 4 || iDestinationHeight < 4)
			{
				return;
			}

			if ((iDestinationWidth % 4) != 0 || (iDestinationHeight % 4) != 0)
			{
				LOG(kDefault, kVerbose, "BC4/BC5/BC7 early out {} x {}", iDestinationWidth, iDestinationHeight);
				return;
			}
		}

		mData.emplace_back(4 * iDestinationWidth * iDestinationHeight);
		stbir_resize_float_linear(mData.at(iLevel).data(), static_cast<int>(iSourceWidth), static_cast<int>(iSourceHeight), static_cast<int>(4 * iSourceWidth * sizeof(float)), mData.back().data(), static_cast<int>(iDestinationWidth), static_cast<int>(iDestinationHeight), static_cast<int>(4 * iDestinationWidth * sizeof(float)), STBIR_4CHANNEL);

		iSourceWidth = iDestinationWidth;
		iSourceHeight = iDestinationHeight;
		++iLevel;
	}
}

void Texture::Crop(int64_t iX, int64_t iY, int64_t iWidth, int64_t iHeight)
{
	ASSERT(std::ssize(mData) == 1);
	ASSERT(iX >= 0 && iY >= 0 && iWidth > 0 && iHeight > 0);
	ASSERT(iX + iWidth <= miWidth && iY + iHeight <= miHeight);

	const std::vector<float>& rSource = mData.at(0);
	std::vector<float> cropped(static_cast<size_t>(4 * iWidth * iHeight));
	for (int64_t iRow = 0; iRow < iHeight; ++iRow)
	{
		const float* pfSource = &rSource.at(static_cast<size_t>(4 * ((iY + iRow) * miWidth + iX)));
		float* pfDestination = &cropped.at(static_cast<size_t>(4 * iRow * iWidth));
		std::memcpy(pfDestination, pfSource, static_cast<size_t>(4 * iWidth * sizeof(float)));
	}
	mData.at(0) = std::move(cropped);
	miWidth = iWidth;
	miHeight = iHeight;
}

void Texture::MaskByHeightmap(const std::vector<float>& rHeightmap, int64_t iHeightmapWidth, int64_t iHeightmapHeight, int64_t iHeightmapDivisor, float fThresholdMeters, const float pfFlatValue[4])
{
	ASSERT(std::ssize(mData) == 1);
	ASSERT(iHeightmapDivisor > 0);
	ASSERT(miWidth == iHeightmapWidth * iHeightmapDivisor);
	ASSERT(miHeight == iHeightmapHeight * iHeightmapDivisor);
	ASSERT(std::ssize(rHeightmap) == iHeightmapWidth * iHeightmapHeight);

	std::vector<float>& rPixels = mData.at(0);
	for (int64_t iY = 0; iY < miHeight; ++iY)
	{
		const float* pfHeightmapRow = &rHeightmap.at(static_cast<size_t>((iY / iHeightmapDivisor) * iHeightmapWidth));
		float* pfPixel = &rPixels.at(static_cast<size_t>(4 * iY * miWidth));
		for (int64_t iX = 0; iX < miWidth; ++iX)
		{
			if (pfHeightmapRow[iX / iHeightmapDivisor] < fThresholdMeters)
			{
				pfPixel[0] = pfFlatValue[0];
				pfPixel[1] = pfFlatValue[1];
				pfPixel[2] = pfFlatValue[2];
				pfPixel[3] = pfFlatValue[3];
			}
			pfPixel += 4;
		}
	}
}

void Texture::Downsize(int64_t iLevels)
{
	ASSERT(std::ssize(mData) == 1);

	MakeMipmaps(VK_FORMAT_R32_SFLOAT, iLevels + 1);

	for (int64_t i = 0; i < iLevels; ++i)
	{
		ASSERT((miWidth >> i) % 2 == 0);
		ASSERT((miHeight >> i) % 2 == 0);
	}
	mData.erase(mData.begin(), mData.begin() + iLevels);
	miWidth >>= iLevels;
	miHeight >>= iLevels;
}

uint32_t Texture::PixelToUint32(const std::vector<float>& rInput, int64_t iWidth, int64_t iX, int64_t iY)
{
	return static_cast<uint32_t>(rInput.at(4 * (iY * iWidth + iX) + 3)) << 24 |
	       static_cast<uint32_t>(rInput.at(4 * (iY * iWidth + iX) + 2)) << 16 |
	       static_cast<uint32_t>(rInput.at(4 * (iY * iWidth + iX) + 1)) <<  8 |
	       static_cast<uint32_t>(rInput.at(4 * (iY * iWidth + iX) + 0));
}

void Texture::EncodeBlocks(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight, VkFormat vkFormat, TextureOptions_t options)
{
	if (gpFileManager->mbForbidExpensiveExport)
	{
		throw std::runtime_error("Texture encoding blocked by BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1");
	}

	// Callers must hold Texture::sEncodeMutex — this function trusts the caller's lock.
	int64_t iActiveEncodes = siActiveEncodeCount.fetch_add(1, std::memory_order_relaxed) + 1;
	common::ScopedLambda decrementActive([]()
	{
		siActiveEncodeCount.fetch_sub(1, std::memory_order_relaxed);
	});
	ASSERT(iActiveEncodes == 1);
	LOG(kDefault, kVerbose, "BC encode: {}x{} (concurrent={})", iWidth, iHeight, iActiveEncodes);

	bc7enc_compress_block_params bc7Parameters {};
	bc7enc_compress_block_params_init(&bc7Parameters);
	bc7enc_compress_block_params_init_linear_weights(&bc7Parameters);
	bc7Parameters.m_max_partitions = static_cast<uint32_t>(kiBc7MaxPartitions);
	bc7Parameters.m_uber_level = static_cast<uint32_t>(kiBc7UberLevel);

	int64_t iBlockColumns = (iWidth + 3) / 4;
	int64_t iBlockRows = (iHeight + 3) / 4;
	int64_t iBytesPerBlock = vkFormat == VK_FORMAT_BC4_UNORM_BLOCK ? 8 : 16;
	std::atomic<bool> bHasAlpha(false);
	auto EncodeBlockRows = [&](int64_t iStart, int64_t iEnd)
	{
		std::array<uint8_t, 4 * 4 * 4> pixels {};
		for (int64_t iBlockY = iStart; iBlockY < iEnd; ++iBlockY)
		{
			for (int64_t iBlockX = 0; iBlockX < iBlockColumns; ++iBlockX)
			{
				for (int64_t iPixelY = 0; iPixelY < 4; ++iPixelY)
				{
					int64_t iSourceY = std::min(iBlockY * 4 + iPixelY, iHeight - 1);
					for (int64_t iPixelX = 0; iPixelX < 4; ++iPixelX)
					{
						int64_t iSourceX = std::min(iBlockX * 4 + iPixelX, iWidth - 1);
						const float* pfSource = rInput.data() + 4 * (iSourceY * iWidth + iSourceX);
						uint8_t* puiPixel = pixels.data() + 4 * (iPixelY * 4 + iPixelX);
						for (int64_t iChannel = 0; iChannel < 4; ++iChannel)
						{
							puiPixel[iChannel] = static_cast<uint8_t>(std::clamp(pfSource[iChannel], 0.0f, 255.0f));
						}
					}
				}

				std::byte* puiBlock = puiOutput + (iBlockY * iBlockColumns + iBlockX) * iBytesPerBlock;
				switch (vkFormat)
				{
					case VK_FORMAT_BC4_UNORM_BLOCK:
						rgbcx::encode_bc4_hq(puiBlock, pixels.data(), 4, static_cast<uint32_t>(kiBc45SearchRadius), rgbcx::BC4_USE_ALL_MODES);
						break;
					case VK_FORMAT_BC5_UNORM_BLOCK:
						rgbcx::encode_bc5_hq(puiBlock, pixels.data(), 0, 1, 4, static_cast<uint32_t>(kiBc45SearchRadius), rgbcx::BC4_USE_ALL_MODES);
						break;
					case VK_FORMAT_BC7_UNORM_BLOCK:
						if (bc7enc_compress_block(puiBlock, pixels.data(), &bc7Parameters))
						{
							bHasAlpha.store(true, std::memory_order_relaxed);
						}
						break;
					default:
						ASSERT(false);
				}
			}
		}
	};
	common::gpMultithreading->Dispatch(iBlockRows, EncodeBlockRows);

	if ((options & TextureOptions::kVerifyNoAlpha) && vkFormat == VK_FORMAT_BC7_UNORM_BLOCK)
	{
		ASSERT(!bHasAlpha.load(std::memory_order_relaxed));
	}
}

void Texture::ToBc4(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight)
{
	EncodeBlocks(puiOutput, rInput, iWidth, iHeight, VK_FORMAT_BC4_UNORM_BLOCK, {});
}

void Texture::ToBc5(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight)
{
	EncodeBlocks(puiOutput, rInput, iWidth, iHeight, VK_FORMAT_BC5_UNORM_BLOCK, {});
}

void Texture::ToBc7(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight, TextureOptions_t options)
{
	EncodeBlocks(puiOutput, rInput, iWidth, iHeight, VK_FORMAT_BC7_UNORM_BLOCK, options);
}

void Texture::ToR8G8B8A8(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight)
{
	for (int64_t j = 0; j < iHeight; ++j)
	{
		for (int64_t i = 0; i < iWidth; ++i)
		{
			reinterpret_cast<uint32_t*>(puiOutput)[j * iWidth + i] = PixelToUint32(rInput, iWidth, i, j);
		}
	}
}

void Texture::ToR16(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight)
{
	for (int64_t j = 0; j < iHeight; ++j)
	{
		for (int64_t i = 0; i < iWidth; ++i)
		{
			float fPixel = rInput.at(4 * (j * iWidth + i)) / 255.0f;

			if (fPixel > 1.0f) [[unlikely]]
			{
				if (fPixel > 1.01f) [[unlikely]]
				{
					LOG(kDefault, kWarning, "{} > 1.01f", fPixel);
				}
				fPixel = 1.0f;
			}
			if (fPixel < 0.0f) [[unlikely]]
			{
				if (fPixel < -0.01f) [[unlikely]]
				{
					LOG(kDefault, kWarning, "{} < -0.01f", fPixel);
				}
				fPixel = 0.0f;
			}

			reinterpret_cast<uint16_t*>(puiOutput)[j * iWidth + i] = common::FloatToUnsignedNormalizedInteger<uint16_t>(fPixel);
		}
	}
}

// R32_SFLOAT carries raw float meters (elevation), not RGB color. The kFloat32 / kUint16Raw
// constructors store source values scaled by 255 in the R channel; divide back here so the
// emitted bytes are the original meters (e.g., 50.0m source → 12750.0 internal → 50.0 emitted).
void Texture::ToR32Sfloat(std::byte* puiOutput, const std::vector<float>& rInput, int64_t iWidth, int64_t iHeight)
{
	for (int64_t j = 0; j < iHeight; ++j)
	{
		for (int64_t i = 0; i < iWidth; ++i)
		{
			reinterpret_cast<float*>(puiOutput)[j * iWidth + i] = rInput.at(4 * (j * iWidth + i)) / 255.0f;
		}
	}
}

void Texture::Export(std::vector<std::byte>& rData, VkFormat vkFormat, TextureOptions_t options)
{
	int64_t iSize = common::ComputeImageByteSize(vkFormat, miWidth, miHeight, std::ssize(mData), 1, 1);
	std::vector<std::byte> data(iSize);

	int64_t iMipWidth = miWidth;
	int64_t iMipHeight = miHeight;
	std::byte* puiCurrentPosition = data.data();
	for (const std::vector<float>& rMipLevel : mData)
	{
		switch (vkFormat)
		{
			case VK_FORMAT_BC4_UNORM_BLOCK:
				ToBc4(puiCurrentPosition, rMipLevel, iMipWidth, iMipHeight);
				break;

			case VK_FORMAT_BC5_UNORM_BLOCK:
				ToBc5(puiCurrentPosition, rMipLevel, iMipWidth, iMipHeight);
				break;

			case VK_FORMAT_BC7_UNORM_BLOCK:
				ToBc7(puiCurrentPosition, rMipLevel, iMipWidth, iMipHeight, options);
				break;

			case VK_FORMAT_R8G8B8A8_UNORM:
				ToR8G8B8A8(puiCurrentPosition, rMipLevel, iMipWidth, iMipHeight);
				break;

			case VK_FORMAT_R16_UNORM:
				ToR16(puiCurrentPosition, rMipLevel, iMipWidth, iMipHeight);
				break;

			case VK_FORMAT_R32_SFLOAT:
				ToR32Sfloat(puiCurrentPosition, rMipLevel, iMipWidth, iMipHeight);
				break;

			default:
				ASSERT(false);
				break;
		}

		puiCurrentPosition += common::SizeInBytes(vkFormat, iMipWidth, iMipHeight);
		iMipWidth = std::max(iMipWidth / 2, 1i64);
		iMipHeight = std::max(iMipHeight / 2, 1i64);
	}

	rData.insert(rData.end(), data.begin(), data.end());
}

void Texture::SaveJpegSidecar(const std::filesystem::path& rPath, int64_t iQuality, TextureOptions_t options)
{
	bool bGrayscale = options & TextureOptions::kGrayscale;
	bool bAutoNormalize = options & TextureOptions::kAutoNormalize;
	ASSERT(!mData.empty());
	const std::vector<float>& rPixels = mData.at(0);
	int64_t iPixelCount = miWidth * miHeight;

	// First pass (auto-normalize only): scan R for min/max so meters/HDR/etc. fit [0, 255].
	float fMin = 0.0f;
	float fRange = 255.0f;
	if (bAutoNormalize)
	{
		fMin = std::numeric_limits<float>::infinity();
		float fMax = -std::numeric_limits<float>::infinity();
		for (float fRed : rPixels | std::views::stride(4))
		{
			fMin = std::min(fMin, fRed);
			fMax = std::max(fMax, fRed);
		}
		fRange = (fMax > fMin) ? (fMax - fMin) : 1.0f;
	}
	float fScale = 255.0f / fRange;

	std::vector<uint8_t> bytes(static_cast<size_t>(3 * miWidth * miHeight));
	const float* pfSource = rPixels.data();
	uint8_t* puiDestination = bytes.data();
	for (int64_t i = 0; i < iPixelCount; ++i)
	{
		int64_t iR = static_cast<int64_t>(std::clamp((pfSource[0] - fMin) * fScale, 0.0f, 255.0f));
		if (bGrayscale)
		{
			puiDestination[0] = static_cast<uint8_t>(iR);
			puiDestination[1] = static_cast<uint8_t>(iR);
			puiDestination[2] = static_cast<uint8_t>(iR);
		}
		else
		{
			puiDestination[0] = static_cast<uint8_t>(iR);
			puiDestination[1] = static_cast<uint8_t>(std::clamp(pfSource[1], 0.0f, 255.0f));
			puiDestination[2] = static_cast<uint8_t>(std::clamp(pfSource[2], 0.0f, 255.0f));
		}
		pfSource += 4;
		puiDestination += 3;
	}
	int64_t iResult = stbi_write_jpg(reinterpret_cast<const char*>(rPath.u8string().c_str()), static_cast<int>(miWidth), static_cast<int>(miHeight), 3, bytes.data(), static_cast<int>(iQuality));
	ASSERT(iResult != 0);
}

std::vector<std::byte> ZlibCompress(std::span<const std::byte> source)
{
	int64_t iBound = compressBound(static_cast<uLong>(source.size()));
	std::vector<std::byte> compressed(static_cast<size_t>(iBound));
	uLongf uiCompressedSize = static_cast<uLongf>(iBound);
	int64_t iZlibResult = compress2(reinterpret_cast<Bytef*>(compressed.data()), &uiCompressedSize, reinterpret_cast<const Bytef*>(source.data()), static_cast<uLong>(source.size()), Z_BEST_COMPRESSION);
	ASSERT(iZlibResult == Z_OK);
	int64_t iCompressedSize = uiCompressedSize;
	compressed.resize(static_cast<size_t>(iCompressedSize));
	return compressed;
}

std::vector<std::byte> Lz4Compress(std::span<const std::byte> source)
{
	int64_t iBound = LZ4_compressBound(static_cast<int>(source.size()));
	std::vector<std::byte> compressed(static_cast<size_t>(iBound));
	int64_t iCompressedSize = LZ4_compress_HC(reinterpret_cast<const char*>(source.data()), reinterpret_cast<char*>(compressed.data()), static_cast<int>(source.size()), static_cast<int>(iBound), LZ4HC_CLEVEL_MAX);
	ASSERT(iCompressedSize > 0);
	compressed.resize(static_cast<size_t>(iCompressedSize));
	return compressed;
}

gli::texture LoadGliFromPath(const std::filesystem::path& rPath)
{
	std::vector<std::byte> data = common::ReadEntireFile(rPath);
	return gli::load(reinterpret_cast<const char*>(data.data()), static_cast<size_t>(std::ssize(data)));
}

TextureIntermediateHeader ReadTextureIntermediateHeader(std::span<const std::byte> data)
{
	static constexpr int64_t kiQwordBytes = static_cast<int64_t>(sizeof(int64_t));
	auto ReadQword = [=](int64_t iOffset) -> int64_t
	{
		int64_t iValue = 0;
		if (iOffset >= 0 && iOffset + kiQwordBytes <= std::ssize(data))
		{
			std::memcpy(&iValue, data.data() + iOffset, sizeof(iValue));
		}
		return iValue;
	};

	TextureIntermediateHeader header;
	int64_t iFirstQword = ReadQword(0);

	// Magic-prefixed files carry a 4-qword header (magic, then width/height/mipCount); legacy files
	// omit the magic, so the first qword is width and the header is 3 qwords. Width sits one qword past
	// the magic in the former, at qword 0 in the latter.
	int64_t iWidthOffset = 0;
	if (iFirstQword == kiTextureIntermediateMagic)
	{
		header.bHadMagic = true;
		iWidthOffset = kiQwordBytes;
		header.iPayloadOffset = 4 * kiQwordBytes;
	}
	else
	{
		header.bHadMagic = false;
		iWidthOffset = 0;
		header.iPayloadOffset = 3 * kiQwordBytes;
	}

	header.iWidth = ReadQword(iWidthOffset);
	header.iHeight = ReadQword(iWidthOffset + kiQwordBytes);
	header.iMipCount = ReadQword(iWidthOffset + 2 * kiQwordBytes);
	return header;
}

void WriteStagedIntermediate(const std::filesystem::path& rPath, const std::function<void(std::ostream&)>& rWriteBody)
{
	static std::atomic<uint64_t> suiSaveSequence {0};
	std::filesystem::path stagingPath = rPath.parent_path();
	// Keep the basename independent of the final path: tagged texture names must not route this stage.
	stagingPath /= L".TextureSaveStage." + std::to_wstring(::GetCurrentProcessId()) + L"." + std::to_wstring(++suiSaveSequence) + L".tmp";

	try
	{
		std::fstream fileStreamOutput(stagingPath, std::ios::out | std::ios::binary | std::ios::trunc);
		VERIFY_SUCCESS(fileStreamOutput.is_open());
		rWriteBody(fileStreamOutput);
		VERIFY_SUCCESS(fileStreamOutput.good());
		fileStreamOutput.flush();
		VERIFY_SUCCESS(fileStreamOutput.good());
		fileStreamOutput.close();
		VERIFY_SUCCESS(fileStreamOutput.good());
		VERIFY_SUCCESS(MoveFileExW(stagingPath.native().c_str(), rPath.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
	}
	catch (...)
	{
		std::error_code errorCode;
		std::filesystem::remove(stagingPath, errorCode);
		throw;
	}
}

void Texture::Save(const std::filesystem::path& rPath, VkFormat vkFormat, TextureOptions_t options)
{
	std::vector<std::byte> data = Export(vkFormat, options);

	std::vector<std::byte> compressed = ZlibCompress(data);

	WriteStagedIntermediate(rPath, [&](std::ostream& rStream)
	{
		int64_t iMagic = kiTextureIntermediateMagic;
		rStream.write(reinterpret_cast<const char*>(&iMagic), sizeof(iMagic));
		rStream.write(reinterpret_cast<const char*>(&miWidth), sizeof(miWidth));
		rStream.write(reinterpret_cast<const char*>(&miHeight), sizeof(miHeight));
		int64_t iMipMaps = std::ssize(mData);
		rStream.write(reinterpret_cast<const char*>(&iMipMaps), sizeof(iMipMaps));
		rStream.write(reinterpret_cast<const char*>(compressed.data()), static_cast<std::streamsize>(std::ssize(compressed)));
	});
}
