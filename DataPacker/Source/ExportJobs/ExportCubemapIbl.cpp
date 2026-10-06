#include "ExportCubemapIbl.h"

#include "Texture/Texture.h"
#include "DiagnosticReporter.h"
#include "FileManager.h"


constexpr int64_t kiCubemapIblFingerprintVersion = 1;

struct KtxCubemapData
{
	std::vector<float> floatData;
	int64_t iFaceSize = 0;
};

static std::filesystem::path GetFingerprintMetadataPath(const std::filesystem::path& rOutputPath, int64_t iInputRoot)
{
	std::filesystem::path metadataPath = gpFileManager->mCacheDirectory / "CubemapIbl" / std::to_string(iInputRoot);
	metadataPath /= std::filesystem::relative(rOutputPath, gpFileManager->mpInputDirectories[iInputRoot]);
	metadataPath += ".meta";
	std::filesystem::create_directories(metadataPath.parent_path());
	return metadataPath;
}

static std::filesystem::path GetDirtyMarkerPath(const std::filesystem::path& rMetadataPath)
{
	std::filesystem::path dirtyMarkerPath = rMetadataPath;
	dirtyMarkerPath += ".dirty";
	return dirtyMarkerPath;
}

using ExpectedIblOutputs = std::unordered_set<std::string>;

// Expected outputs and swept outputs are both built from the same input roots, so a lowered normalized
// generic string is a sufficient identity for membership.
static std::string GetExpectedOutputKey(const std::filesystem::path& rOutputPath)
{
	return common::ToLower(rOutputPath.lexically_normal().generic_string());
}

static std::string GetCubemapFingerprint(std::string_view operation, const std::filesystem::path& rSourcePath)
{
	nlohmann::json metadata;
	metadata["version"] = kiCubemapIblFingerprintVersion;
	metadata["operation"] = operation;
	metadata["source"] = gpFileManager->mpInputFingerprintCache->Get(rSourcePath);
	return metadata.dump();
}

static std::string GetFaceCubemapFingerprint(std::string_view operation, const std::filesystem::path& rSourceDirectory, const char* const* ppFaceNames)
{
	nlohmann::json metadata;
	metadata["version"] = kiCubemapIblFingerprintVersion;
	metadata["operation"] = operation;
	for (int64_t i = 0; i < 6; ++i)
	{
		metadata["faces"][ppFaceNames[i]] = gpFileManager->mpInputFingerprintCache->Get(rSourceDirectory / ppFaceNames[i]);
	}
	return metadata.dump();
}

static void WriteFingerprintMetadata(const std::filesystem::path& rMetadataPath, std::string_view fingerprint)
{
	std::filesystem::path temporaryPath = rMetadataPath;
	temporaryPath += ".tmp";
	std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
	stream.write(fingerprint.data(), static_cast<std::streamsize>(fingerprint.size()));
	stream.close();
	VERIFY_SUCCESS(stream.good());
	VERIFY_SUCCESS(MoveFileExW(temporaryPath.native().c_str(), rMetadataPath.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
}

static bool IsOutputCurrent(const std::filesystem::path& rOutputPath, const std::filesystem::path& rMetadataPath, std::string_view fingerprint, std::span<const std::filesystem::path> legacyInputs)
{
	if (std::filesystem::exists(GetDirtyMarkerPath(rMetadataPath)))
	{
		return false;
	}

	if (!std::filesystem::exists(rOutputPath))
	{
		return false;
	}

	if (std::filesystem::exists(rMetadataPath))
	{
		std::ifstream stream(rMetadataPath, std::ios::binary);
		std::string cachedFingerprint = std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
		return !stream.bad() && cachedFingerprint == fingerprint;
	}

	std::filesystem::file_time_type outputTime = std::filesystem::last_write_time(rOutputPath);
	if (std::ranges::any_of(legacyInputs, [outputTime](const std::filesystem::path& rLegacyInput)
	{
		return std::filesystem::last_write_time(rLegacyInput) > outputTime;
	}))
	{
		return false;
	}
	WriteFingerprintMetadata(rMetadataPath, fingerprint);
	return true;
}

static void BeginOutputUpdate(const std::filesystem::path& rMetadataPath)
{
	if (gpFileManager->mbForbidExpensiveExport)
	{
		throw std::runtime_error("IBL cubemap convolution blocked by BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1");
	}

	std::ofstream stream(GetDirtyMarkerPath(rMetadataPath), std::ios::binary | std::ios::trunc);
	stream.close();
	VERIFY_SUCCESS(stream.good());
	std::filesystem::remove(rMetadataPath);
}

static void CompleteOutputUpdate(const std::filesystem::path& rMetadataPath, std::string_view fingerprint)
{
	WriteFingerprintMetadata(rMetadataPath, fingerprint);
	VERIFY_SUCCESS(std::filesystem::remove(GetDirtyMarkerPath(rMetadataPath)));
}

constexpr std::string_view kIrradianceOutputStem = "_Irradiance";
constexpr std::string_view kPrefilteredOutputStem = "_Prefiltered";

static void RemoveOrphanedOutput(const std::filesystem::path& rOutputPath, const std::filesystem::path& rSidecarPath)
{
	std::filesystem::file_status outputStatus = std::filesystem::symlink_status(rOutputPath);
	if (outputStatus.type() != std::filesystem::file_type::not_found)
	{
		if (outputStatus.type() != std::filesystem::file_type::regular)
		{
			throw std::runtime_error(std::format("Orphaned IBL cubemap intermediate \"{}\" is not a regular file.", rOutputPath.string()));
		}
		std::filesystem::remove(rOutputPath);
		LOG(kDefault, kDebug, "Removed orphaned IBL cubemap intermediate: \"{}\"", rOutputPath.native());
	}
	std::filesystem::remove(rSidecarPath);
}

// Removes every producer-owned output that current inputs no longer expect, so a deleted or renamed source
// cannot keep publishing a texture chunk. Only a mirrored cache sidecar (.meta from a completed write,
// .meta.dirty from an interrupted one) proves this pre-pass wrote the output; a half-float file without one
// may belong to another producer, so it is never removed here. A sidecar whose output is already gone is
// still cleaned up.
static void ReconcileIblOutputs(const ExpectedIblOutputs& rExpectedOutputs, std::string_view outputStem)
{
	std::string outputTail = common::ToLower(std::string(outputStem) + TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT));

	for (int64_t i = 0; i < static_cast<int64_t>(std::size(gpFileManager->mpInputDirectories)); ++i)
	{
		std::filesystem::path cacheRoot = gpFileManager->mCacheDirectory / "CubemapIbl" / std::to_string(i);
		if (!std::filesystem::exists(cacheRoot))
		{
			continue;
		}

		// Removal is deferred until the sweep finishes because erasing entries under an active
		// recursive_directory_iterator is unspecified.
		std::vector<std::pair<std::filesystem::path, std::filesystem::path>> orphans;
		for (const std::filesystem::directory_entry& rSidecar : std::filesystem::recursive_directory_iterator(cacheRoot))
		{
			// Only a real file is ownership proof: a directory or symlink named like a sidecar would
			// otherwise derive and delete a sibling output this pre-pass never wrote.
			if (std::filesystem::symlink_status(rSidecar.path()).type() != std::filesystem::file_type::regular)
			{
				continue;
			}

			std::string sidecarName = rSidecar.path().filename().string();
			std::string_view outputName = sidecarName;
			if (outputName.ends_with(".meta.dirty"))
			{
				outputName.remove_suffix(std::string_view(".meta.dirty").size());
			}
			else if (outputName.ends_with(".meta"))
			{
				outputName.remove_suffix(std::string_view(".meta").size());
			}
			else
			{
				continue;
			}

			if (!common::ToLower(outputName).ends_with(outputTail))
			{
				continue;
			}

			std::filesystem::path relativePath = rSidecar.path().lexically_relative(cacheRoot);
			if (relativePath.empty())
			{
				throw std::runtime_error(std::format("IBL cubemap cache entry \"{}\" is not inside its cache root.", rSidecar.path().string()));
			}

			if (relativePath.is_absolute())
			{
				throw std::runtime_error(std::format("IBL cubemap cache entry \"{}\" is not inside its cache root.", rSidecar.path().string()));
			}

			if (relativePath.begin()->string() == "..")
			{
				throw std::runtime_error(std::format("IBL cubemap cache entry \"{}\" is not inside its cache root.", rSidecar.path().string()));
			}

			relativePath.replace_filename(outputName);

			std::filesystem::path outputPath = gpFileManager->mpInputDirectories[i] / relativePath;
			if (!rExpectedOutputs.contains(GetExpectedOutputKey(outputPath)))
			{
				orphans.emplace_back(std::move(outputPath), rSidecar.path());
			}
		}

		for (const std::pair<std::filesystem::path, std::filesystem::path>& rOrphan : orphans)
		{
			RemoveOrphanedOutput(rOrphan.first, rOrphan.second);
		}
	}
}

// Reports one pass's collected failures as a single aggregate record, matching the shape RunExportJobs uses
// for an ordinary asset type, and answers whether the pass succeeded. These entries stay out of the run
// summary's job counts, which cover export jobs only.
static bool ReportIblFailures(std::string_view passName, std::vector<diagnostic::ExportFailure>& rFailures)
{
	if (rFailures.empty())
	{
		return true;
	}

	diagnostic::Record record
	{
		.eSeverity = diagnostic::Severity::kError,
		.title = "Data Packer - Export Failed",
		.message = std::format("One or more {} cubemaps failed", passName),
		.eButtons = diagnostic::ButtonContract::kOk,
		.eIcon = diagnostic::ModalIcon::kNone,
		.exportFailures = std::move(rFailures),
	};
	diagnostic::Report(record);
	return false;
}


static KtxCubemapData LoadKtxCubemapAsFloat(const std::filesystem::path& rPath)
{
	KtxCubemapData result;
	gli::texture texture = LoadGliFromPath(rPath);
	ASSERT(!texture.empty() && texture.target() == gli::TARGET_CUBE);
	gli::texture_cube textureCube(texture);
	ASSERT(textureCube.format() == gli::FORMAT_RGBA16_SFLOAT_PACK16);
	result.iFaceSize = textureCube[0].extent().x;
	int64_t iPixelsPerFace = result.iFaceSize * result.iFaceSize;
	result.floatData.resize(iPixelsPerFace * 6 * 4);
	for (int64_t i = 0; i < 6; ++i)
	{
		const uint16_t* pSourceHalf = static_cast<const uint16_t*>(textureCube[i].data());
		float* pDestinationFloat = result.floatData.data() + i * iPixelsPerFace * 4;
		DirectX::PackedVector::XMConvertHalfToFloatStream(pDestinationFloat, sizeof(float), pSourceHalf, sizeof(uint16_t), iPixelsPerFace * 4);
	}
	return result;
}

bool GenerateIrradianceCubemaps()
{
	ExpectedIblOutputs expectedOutputs;
	std::vector<diagnostic::ExportFailure> failures;
	bool bDiscoveryComplete = true;
	try
	{
		for (int64_t i = 0; i < static_cast<int64_t>(std::size(gpFileManager->mpInputDirectories)); ++i)
		{
			const std::filesystem::path& rBaseDirectory = gpFileManager->mpInputDirectories[i];
			for (const std::filesystem::directory_entry& rDirectoryEntry : std::filesystem::recursive_directory_iterator(rBaseDirectory))
			{
				if (rDirectoryEntry.path().extension() != ".ktx")
				{
					continue;
				}

				if (!HasCubemapTag(rDirectoryEntry.path()))
				{
					continue;
				}

				// Build output path: [C]<name>_Irradiance<R16G16B16A16_SFLOAT suffix>
				std::filesystem::path outputPath = rDirectoryEntry.path().parent_path() / rDirectoryEntry.path().stem();
				outputPath += kIrradianceOutputStem;
				outputPath += TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT);
				// Recorded before any work that can fail, so a contained per-input failure still leaves this
				// output expected and the reconcile below cannot delete its previous complete file.
				expectedOutputs.insert(GetExpectedOutputKey(outputPath));

				try
				{
					std::string fingerprint = GetCubemapFingerprint("irradiance", rDirectoryEntry.path());
					std::filesystem::path metadataPath = GetFingerprintMetadataPath(outputPath, i);
					std::filesystem::path legacyInput = rDirectoryEntry.path();
					if (IsOutputCurrent(outputPath, metadataPath, fingerprint, std::span(&legacyInput, 1)))
					{
						continue;
					}
					BeginOutputUpdate(metadataPath);

					LOG(kDefault, kDebug, "Generating irradiance cubemap for \"{}\"", rDirectoryEntry.path().filename().string());

					KtxCubemapData cubemapData = LoadKtxCubemapAsFloat(rDirectoryEntry.path());
					int64_t iFaceSize = cubemapData.iFaceSize;
					int64_t iPixelsPerFace = iFaceSize * iFaceSize;
					int64_t iTotalPixels = iPixelsPerFace * 6;

					cmft::Image sourceImage;
					cmft::Image destinationImage;
					cmft::imageCreate(sourceImage, static_cast<uint32_t>(iFaceSize), static_cast<uint32_t>(iFaceSize), 0x000000ff, 1, 6, cmft::TextureFormat::RGBA32F);
					common::ScopedLambda imageCleanup([&]()
					{
						cmft::imageUnload(sourceImage);
						cmft::imageUnload(destinationImage);
					});
					std::memcpy(sourceImage.m_data, cubemapData.floatData.data(), iTotalPixels * 4 * sizeof(float));

					// Generate 128x128 irradiance through serial CPU double-precision spherical harmonics, without
					// OpenCL or a thread-count input. DataPacker is not /fp:strict, so FMA contraction and the FP
					// environment can affect output across hosts.
					static constexpr int64_t kiIrradianceFaceSize = 128;
					cmft::imageIrradianceFilterSh(destinationImage, static_cast<uint32_t>(kiIrradianceFaceSize), sourceImage);

					int64_t iIrradiancePixelsPerFace = kiIrradianceFaceSize * kiIrradianceFaceSize;
					int64_t iIrradianceTotalPixels = iIrradiancePixelsPerFace * 6;
					std::vector<uint16_t> halfData(iIrradianceTotalPixels * 4);

					const float* pSourceFloat = static_cast<const float*>(destinationImage.m_data);
					DirectX::PackedVector::XMConvertFloatToHalfStream(halfData.data(), sizeof(uint16_t), pSourceFloat, sizeof(float), iIrradianceTotalPixels * 4);

					// Write intermediate file: [width][height][mipcount][pixel data for 6 faces]
					int64_t iWidth = kiIrradianceFaceSize;
					int64_t iHeight = kiIrradianceFaceSize;
					int64_t iMipCount = 1;

					WriteStagedIntermediate(outputPath, [&](std::ostream& rStream)
					{
						rStream.write(reinterpret_cast<const char*>(&iWidth), sizeof(iWidth));
						rStream.write(reinterpret_cast<const char*>(&iHeight), sizeof(iHeight));
						rStream.write(reinterpret_cast<const char*>(&iMipCount), sizeof(iMipCount));
						rStream.write(reinterpret_cast<const char*>(halfData.data()), static_cast<std::streamsize>(std::ssize(halfData) * static_cast<int64_t>(sizeof(uint16_t))));
					});
					CompleteOutputUpdate(metadataPath, fingerprint);
				}
				catch (const std::exception& rException)
				{
					failures.push_back({.assetPath = rDirectoryEntry.path(), .message = rException.what()});
				}
			}
		}
	}
	catch (const std::exception& rException)
	{
		// A partial walk leaves the expected-output set incomplete, and reconciling against it would delete
		// outputs the unvisited inputs still expect, so the sweep is skipped for this run.
		bDiscoveryComplete = false;
		failures.push_back({.message = std::format("Irradiance cubemap discovery failed, skipping orphaned-output removal: {}", rException.what())});
	}

	if (bDiscoveryComplete)
	{
		try
		{
			ReconcileIblOutputs(expectedOutputs, kIrradianceOutputStem);
		}
		catch (const std::exception& rException)
		{
			failures.push_back({.message = std::format("Irradiance cubemap orphaned-output removal failed: {}", rException.what())});
		}
	}

	return ReportIblFailures("irradiance", failures);
}

static constexpr int64_t kiPreFilteredFaceSize = 1'024;
static constexpr int64_t kiPreFilteredMipCount = 11; // log2(1024) + 1

// Packs a CMFT radiance-filtered cubemap into face-major / mip-minor half-floats (matching the engine's
// TextureUploadManager iteration order) and writes the [width][height][mipcount][pixels] intermediate.
static void WriteFilteredCubemap(cmft::Image& rDestinationImage, const std::filesystem::path& rOutputPath)
{
	uint32_t offsets[CUBE_FACE_NUM][MAX_MIP_NUM] {};
	cmft::imageGetMipOffsets(offsets, rDestinationImage);

	int64_t iTotalHalfFloats = 0;
	int64_t iMipSize = kiPreFilteredFaceSize;
	for (int64_t i = 0; i < kiPreFilteredMipCount; ++i, iMipSize /= 2)
	{
		iTotalHalfFloats += iMipSize * iMipSize * 4;
	}
	iTotalHalfFloats *= 6; // 6 faces

	std::vector<uint16_t> halfData(iTotalHalfFloats);
	int64_t iHalfOffset = 0;

	for (int64_t i = 0; i < 6; ++i)
	{
		iMipSize = kiPreFilteredFaceSize;
		for (int64_t j = 0; j < kiPreFilteredMipCount; ++j, iMipSize /= 2)
		{
			int64_t iMipPixels = iMipSize * iMipSize;
			const float* pSourceFloat = reinterpret_cast<const float*>(static_cast<uint8_t*>(rDestinationImage.m_data) + offsets[i][j]);
			DirectX::PackedVector::XMConvertFloatToHalfStream(halfData.data() + iHalfOffset, sizeof(uint16_t), pSourceFloat, sizeof(float), iMipPixels * 4);
			iHalfOffset += iMipPixels * 4;
		}
	}

	// Write intermediate file: [width][height][mipcount][pixel data]
	int64_t iWidth = kiPreFilteredFaceSize;
	int64_t iHeight = kiPreFilteredFaceSize;
	int64_t iMipCount = kiPreFilteredMipCount;

	WriteStagedIntermediate(rOutputPath, [&](std::ostream& rStream)
	{
		rStream.write(reinterpret_cast<const char*>(&iWidth), sizeof(iWidth));
		rStream.write(reinterpret_cast<const char*>(&iHeight), sizeof(iHeight));
		rStream.write(reinterpret_cast<const char*>(&iMipCount), sizeof(iMipCount));
		rStream.write(reinterpret_cast<const char*>(halfData.data()), static_cast<std::streamsize>(std::ssize(halfData) * static_cast<int64_t>(sizeof(uint16_t))));
	});
}

// Radiance-filters every [C]-tagged .ktx cubemap that is out of date and writes the pre-filtered
// intermediate beside the source. Returns false when input discovery itself failed, leaving the shared
// expected-output set incomplete.
static bool ProcessKtxCubemaps(int64_t iCpuThreads, cmft::ClContext* pClContext, ExpectedIblOutputs& rExpectedOutputs, std::vector<diagnostic::ExportFailure>& rFailures)
{
	try
	{
		for (int64_t i = 0; i < static_cast<int64_t>(std::size(gpFileManager->mpInputDirectories)); ++i)
		{
			const std::filesystem::path& rBaseDirectory = gpFileManager->mpInputDirectories[i];
			for (const std::filesystem::directory_entry& rDirectoryEntry : std::filesystem::recursive_directory_iterator(rBaseDirectory))
			{
				if (rDirectoryEntry.path().extension() != ".ktx")
				{
					continue;
				}

				if (!HasCubemapTag(rDirectoryEntry.path()))
				{
					continue;
				}

				std::filesystem::path outputPath = rDirectoryEntry.path().parent_path() / rDirectoryEntry.path().stem();
				outputPath += kPrefilteredOutputStem;
				outputPath += TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT);
				rExpectedOutputs.insert(GetExpectedOutputKey(outputPath));

				try
				{
					std::string fingerprint = GetCubemapFingerprint("prefiltered", rDirectoryEntry.path());
					std::filesystem::path metadataPath = GetFingerprintMetadataPath(outputPath, i);
					std::filesystem::path legacyInput = rDirectoryEntry.path();
					if (IsOutputCurrent(outputPath, metadataPath, fingerprint, std::span(&legacyInput, 1)))
					{
						continue;
					}
					BeginOutputUpdate(metadataPath);

					LOG(kDefault, kDebug, "Generating pre-filtered cubemap for \"{}\"", rDirectoryEntry.path().filename().string());

					KtxCubemapData cubemapData = LoadKtxCubemapAsFloat(rDirectoryEntry.path());
					int64_t iFaceSize = cubemapData.iFaceSize;
					int64_t iPixelsPerFace = iFaceSize * iFaceSize;
					int64_t iTotalPixels = iPixelsPerFace * 6;

					cmft::Image sourceImage;
					cmft::imageCreate(sourceImage, static_cast<uint32_t>(iFaceSize), static_cast<uint32_t>(iFaceSize), 0x000000ff, 1, 6, cmft::TextureFormat::RGBA32F);
					cmft::Image destinationImage;
					common::ScopedLambda imageCleanup([&]()
					{
						cmft::imageUnload(sourceImage);
						cmft::imageUnload(destinationImage);
					});
					std::memcpy(sourceImage.m_data, cubemapData.floatData.data(), iTotalPixels * 4 * sizeof(float));

					cmft::imageRadianceFilter(destinationImage, static_cast<uint32_t>(kiPreFilteredFaceSize), cmft::LightingModel::BlinnBrdf, false, static_cast<uint8_t>(kiPreFilteredMipCount), 14, 4, sourceImage, cmft::EdgeFixup::None, static_cast<uint8_t>(iCpuThreads), pClContext);

					WriteFilteredCubemap(destinationImage, outputPath);
					CompleteOutputUpdate(metadataPath, fingerprint);
				}
				catch (const std::exception& rException)
				{
					rFailures.push_back({.assetPath = rDirectoryEntry.path(), .message = rException.what()});
				}
			}
		}
	}
	catch (const std::exception& rException)
	{
		rFailures.push_back({.message = std::format("Pre-filtered cubemap .ktx discovery failed, skipping orphaned-output removal: {}", rException.what())});
		return false;
	}

	return true;
}

// Radiance-filters every [C]-tagged directory of six cube-face images (posx/negx/... .jpg or px/nx/... .png)
// that is out of date and writes the pre-filtered intermediate beside the directory. Returns false when
// input discovery itself failed, leaving the shared expected-output set incomplete.
static bool ProcessFaceImageCubemaps(int64_t iCpuThreads, cmft::ClContext* pClContext, ExpectedIblOutputs& rExpectedOutputs, std::vector<diagnostic::ExportFailure>& rFailures)
{
	try
	{
		for (int64_t i = 0; i < static_cast<int64_t>(std::size(gpFileManager->mpInputDirectories)); ++i)
		{
			const std::filesystem::path& rBaseDirectory = gpFileManager->mpInputDirectories[i];
			for (const std::filesystem::directory_entry& rDirectoryEntry : std::filesystem::recursive_directory_iterator(rBaseDirectory))
			{
				if (!rDirectoryEntry.is_directory())
				{
					continue;
				}

				if (!HasCubemapTag(rDirectoryEntry.path()))
				{
					continue;
				}

				bool bHasJpgFaces = std::filesystem::exists(rDirectoryEntry.path() / "posx.jpg");
				bool bHasPngFaces = std::filesystem::exists(rDirectoryEntry.path() / "px.png");
				if (!bHasJpgFaces && !bHasPngFaces)
				{
					continue;
				}

				std::filesystem::path outputPath = rDirectoryEntry.path().parent_path() / rDirectoryEntry.path().filename();
				outputPath += kPrefilteredOutputStem;
				outputPath += TextureIntermediateSuffix(VK_FORMAT_R16G16B16A16_SFLOAT);
				rExpectedOutputs.insert(GetExpectedOutputKey(outputPath));

				try
				{
					static constexpr const char* kpcJpgFaceNames[6] =
					{
						"posx.jpg",
						"negx.jpg",
						"posy.jpg",
						"negy.jpg",
						"posz.jpg",
						"negz.jpg",
					};
					static constexpr const char* kpcPngFaceNames[6] =
					{
						"px.png",
						"nx.png",
						"py.png",
						"ny.png",
						"pz.png",
						"nz.png",
					};
					const char* const* pFaceNames = bHasJpgFaces ? kpcJpgFaceNames : kpcPngFaceNames;
					std::filesystem::path legacyInputs[6];
					for (int64_t j = 0; j < 6; ++j)
					{
						legacyInputs[j] = rDirectoryEntry.path() / pFaceNames[j];
					}
					std::string fingerprint = GetFaceCubemapFingerprint("prefiltered", rDirectoryEntry.path(), pFaceNames);
					std::filesystem::path metadataPath = GetFingerprintMetadataPath(outputPath, i);
					if (IsOutputCurrent(outputPath, metadataPath, fingerprint, legacyInputs))
					{
						continue;
					}
					BeginOutputUpdate(metadataPath);

					LOG(kDefault, kDebug, "Generating pre-filtered cubemap for \"{}\"", rDirectoryEntry.path().filename().string());

					cmft::Image faceImages[6];
					cmft::Image sourceImage;
					cmft::Image destinationImage;
					common::ScopedLambda imageCleanup([&]()
					{
						for (cmft::Image& rFaceImage : faceImages)
						{
							cmft::imageUnload(rFaceImage);
						}
						cmft::imageUnload(sourceImage);
						cmft::imageUnload(destinationImage);
					});
					for (int64_t j = 0; j < 6; ++j)
					{
						// stb link-resolves to the shared first-party STBI_WINDOWS_UTF8 build (cmft's vendored stb is not compiled), so imageLoadStb decodes this UTF-8 path correctly.
						std::u8string facePath = (rDirectoryEntry.path() / pFaceNames[j]).u8string();
						cmft::imageLoadStb(faceImages[j], reinterpret_cast<const char*>(facePath.c_str()), cmft::TextureFormat::RGBA32F);
					}

					cmft::imageCubemapFromFaceList(sourceImage, faceImages);

					cmft::imageRadianceFilter(destinationImage, static_cast<uint32_t>(kiPreFilteredFaceSize), cmft::LightingModel::BlinnBrdf, false, static_cast<uint8_t>(kiPreFilteredMipCount), 14, 4, sourceImage, cmft::EdgeFixup::None, static_cast<uint8_t>(iCpuThreads), pClContext);

					WriteFilteredCubemap(destinationImage, outputPath);
					CompleteOutputUpdate(metadataPath, fingerprint);
				}
				catch (const std::exception& rException)
				{
					rFailures.push_back({.assetPath = rDirectoryEntry.path(), .message = rException.what()});
				}
			}
		}
	}
	catch (const std::exception& rException)
	{
		rFailures.push_back({.message = std::format("Pre-filtered cubemap face-image discovery failed, skipping orphaned-output removal: {}", rException.what())});
		return false;
	}

	return true;
}

bool GeneratePreFilteredCubemaps()
{
	// cmft takes the CPU thread count as uint8_t
	static const int64_t siCpuThreads = std::min(common::LogicalCoreCount(), static_cast<int64_t>(std::numeric_limits<uint8_t>::max()));

	// OpenCL radiance convolution produces GPU/driver-dependent half-float output; the CPU fallback
	// depends on siCpuThreads. R16G16B16A16_SFLOAT intermediates rely on a single canonical bake host
	// for reproducibility.
	ExpectedIblOutputs expectedOutputs;
	std::vector<diagnostic::ExportFailure> failures;
	bool bDiscoveryComplete = true;
	cmft::ClContext* pClContext = nullptr;
	bool bClLoaded = cmft::clLoad() != 0;
	{
		common::ScopedLambda openClCleanup([&]()
		{
			if (bClLoaded)
			{
				cmft::clDestroy(pClContext);
				cmft::clUnload();
			}
		});

		if (bClLoaded)
		{
			pClContext = cmft::clInit(CMFT_CL_VENDOR_ANY_GPU, CMFT_CL_DEVICE_TYPE_GPU);
		}

		// Both sub-passes fill one expected-output set, so both run and either one's incomplete walk blocks
		// the single sweep below.
		bDiscoveryComplete &= ProcessKtxCubemaps(siCpuThreads, pClContext, expectedOutputs, failures);
		bDiscoveryComplete &= ProcessFaceImageCubemaps(siCpuThreads, pClContext, expectedOutputs, failures);
	}

	if (bDiscoveryComplete)
	{
		try
		{
			ReconcileIblOutputs(expectedOutputs, kPrefilteredOutputStem);
		}
		catch (const std::exception& rException)
		{
			failures.push_back({.message = std::format("Pre-filtered cubemap orphaned-output removal failed: {}", rException.what())});
		}
	}

	return ReportIblFailures("pre-filtered", failures);
}
