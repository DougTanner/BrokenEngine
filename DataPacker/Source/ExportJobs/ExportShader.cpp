#include "ExportShader.h"

#include "FileManager.h"


static const std::filesystem::path& GetVulkanSdkBinariesDirectory()
{
	static const std::filesystem::path sPath = []()
	{
		wchar_t pcDirectory[MAX_PATH] {};
		int64_t iResult = GetEnvironmentVariableW(L"VK_SDK_PATH", pcDirectory, static_cast<DWORD>(std::size(pcDirectory) - 1));
		if (iResult == 0)
		{
			throw std::runtime_error("VK_SDK_PATH environment variable not found");
		}
		std::filesystem::path path(pcDirectory);
		VERIFY_SUCCESS(std::filesystem::exists(path));
		path.append("Bin");
		LOG(kDefault, kDebug, "Vulkan binaries directory: \"{}\"", path.string());
		return path;
	}();
	return sPath;
}

std::optional<common::ChunkFlags_t> ExportShader::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	std::filesystem::path extension = rDirectoryEntry.path().extension();
	return extension == ".comp" || extension == ".frag" || extension == ".vert" ? std::optional<common::ChunkFlags_t>(common::ChunkFlags::kShader) : std::nullopt;
}


// In-progress binding-table state shared by every CollectBindings call for one shader.
// The buffers and riBindingCount refer to caller-owned storage; CollectBindings updates riBindingCount as bindings land.
struct BindingTable
{
	VkDescriptorSetLayoutBinding* pVkDescriptorSetLayoutBindings = nullptr;
	uint32_t* puiSetIndices = nullptr;
	int64_t& riBindingCount;
	common::ChunkFlags_t chunkFlags;
};

static void WriteBinding(BindingTable& rTable, int64_t iBinding, int64_t iSet, VkDescriptorType vkDescriptorType, int64_t iDescriptorCount)
{
	ASSERT(iBinding < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
	// Table is indexed by binding number alone — a second write means two sets reuse one binding number, which would silently overwrite the first entry
	ASSERT(rTable.pVkDescriptorSetLayoutBindings[iBinding].descriptorCount == 0);
	VkDescriptorSetLayoutBinding& rVkDescriptorSetLayoutBinding = rTable.pVkDescriptorSetLayoutBindings[iBinding];
	rVkDescriptorSetLayoutBinding.binding = static_cast<uint32_t>(iBinding);
	rVkDescriptorSetLayoutBinding.descriptorType = vkDescriptorType;
	rVkDescriptorSetLayoutBinding.descriptorCount = static_cast<uint32_t>(iDescriptorCount);
	rVkDescriptorSetLayoutBinding.stageFlags = rTable.chunkFlags & common::ChunkFlags::kCompute ? VK_SHADER_STAGE_COMPUTE_BIT : (rTable.chunkFlags & common::ChunkFlags::kFragment ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT);
	rVkDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
	rTable.puiSetIndices[iBinding] = static_cast<uint32_t>(iSet);
}

// Enumerates one SPIRV-Cross resource category, writing a descriptor binding per resource.
// countFn maps the reflected type to a descriptor count (constant for buffers/samplers, array-derived for images).
template <typename COUNT_FN>
static void CollectBindings(const spirv_cross::SmallVector<spirv_cross::Resource>& rResources, std::string_view label, VkDescriptorType vkDescriptorType, const spirv_cross::Compiler& rCompiler, BindingTable& rTable, COUNT_FN&& countFn)
{
	if (rResources.empty())
	{
		return;
	}

	LOG(kDefault, kVerbose, "{}", label);
	for (const spirv_cross::Resource& rResource : rResources)
	{
		int64_t iBinding = rCompiler.get_decoration(rResource.id, spv::DecorationBinding);
		int64_t iSet = rCompiler.get_decoration(rResource.id, spv::DecorationDescriptorSet);
		LOG(kDefault, kVerbose,"   {} {} {} set {} bound at {}", static_cast<uint32_t>(rResource.type_id), static_cast<uint32_t>(rResource.base_type_id), rResource.name, iSet, iBinding);

		const spirv_cross::SPIRType& rSpirvType = rCompiler.get_type(rResource.type_id);
		if (!rSpirvType.array.empty())
		{
			LOG(kDefault, kVerbose,"   Array size: {}", rSpirvType.array[0]);
		}

		WriteBinding(rTable, iBinding, iSet, vkDescriptorType, countFn(rSpirvType));
		rTable.riBindingCount = std::max(iBinding + 1, rTable.riBindingCount);
	}
}


void ExportShader::Export()
{
	std::filesystem::path preProcessedFile = PreprocessShader();
	std::filesystem::path spirvFile = CompileShader(preProcessedFile);
	spirvFile = OptimizeShader(spirvFile);
	ReflectAndWriteShader(spirvFile);
}

std::filesystem::path ExportShader::RunVulkanTool(const std::filesystem::path& rExecutable, std::wstring& rParameters, const std::filesystem::path& rOutputFile, bool bThrowOnAnyOutput)
{
	LOG(kDefault, kVerbose, "{}: {}{}", common::gpThreadLocal->miThreadId.value_or(0), rExecutable, rParameters);

	std::optional<common::ExecutableResult> result = common::RunExecutable(rExecutable, rParameters);

	std::string toolName = rExecutable.filename().string();
	if (!result)
	{
		throw std::runtime_error(std::format("{} could not be started", toolName));
	}

	// glslc fails silently on compile errors (empty stdout + success exit code), so the preprocess caller treats any
	// stdout as fatal; glslangValidator / spirv-opt key on the exit code and only warn on non-empty stdout.
	if (bThrowOnAnyOutput)
	{
		if (!result->output.empty())
		{
			throw std::runtime_error(std::format("{} error: {}", toolName, result->output));
		}
	}
	else if (result->iExitCode != 0)
	{
		throw std::runtime_error(std::format("{} error: {}", toolName, result->output));
	}

	if (!std::filesystem::exists(rOutputFile))
	{
		throw std::runtime_error(std::format("{} did not produce \"{}\"", toolName, rOutputFile.string()));
	}

	if (!bThrowOnAnyOutput && !result->output.empty())
	{
		LOG(kDefault, kWarning, "{} output: {}", toolName, result->output);
	}

	mIntermediateFiles.push_back(rOutputFile);

	return rOutputFile;
}

std::filesystem::path ExportShader::PreprocessShader()
{
	// glslc preprocesses shaders with #include support; glslangValidator compiles them because glslc can
	// return success with compile errors.
	std::filesystem::path glslcExecutable(GetVulkanSdkBinariesDirectory());
	glslcExecutable.append("glslc.exe");

	std::filesystem::path preProcessedFile(gpFileManager->mCacheDirectory);
	preProcessedFile /= mRelativeDirectory;
	preProcessedFile /= mInputPath.filename();
	std::filesystem::remove(preProcessedFile);

	std::wstring commandLineParameters(L"");
	commandLineParameters += L" -O";      // Enable optimization
	commandLineParameters += L" -E";      // Pre-process only
	commandLineParameters += L" -Werror"; // Treat warnings as errors
	commandLineParameters += L" -MD";
	commandLineParameters += L" -MF \"" + mDependencyFile.native() + L"\"";
	commandLineParameters += L" -I \"" + gpFileManager->mpInputDirectories[0].native() + L"/Shaders\"";
	commandLineParameters += L" -I \"" + gpFileManager->mpInputDirectories[1].native() + L"/Shaders\"";
	commandLineParameters += L" -o \"" + preProcessedFile.native() + L"\"";
	commandLineParameters += L" \"" + mInputPath.native() + L"\"";

	std::filesystem::path result = RunVulkanTool(glslcExecutable, commandLineParameters, preProcessedFile, /*bThrowOnAnyOutput*/ true);
	CaptureDependencies();
	return result;
}

ExportShader::ExportShader(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile)
: ExportJob(rChunkFlags, rFile, kiVersion)
{
	mDependencyFile = gpFileManager->mCacheDirectory / mRelativeDirectory / (mInputPath.filename().native() + L".d");
	mDependencyMetadataFile = gpFileManager->mCacheDirectory / mRelativeDirectory / mInputPath.filename();
	mDependencyMetadataFile += std::format(".v{}.deps.meta", miVersion);

	if (rFile.extension() == ".comp")
	{
		mChunkFlags.Set(common::ChunkFlags::kCompute);
	}
	else if (rFile.extension() == ".frag")
	{
		mChunkFlags.Set(common::ChunkFlags::kFragment);
	}
	else if (rFile.extension() == ".vert")
	{
		mChunkFlags.Set(common::ChunkFlags::kVertex);
	}
}

std::filesystem::path ExportShader::CompileShader(const std::filesystem::path& rPreProcessedFile)
{
	std::filesystem::path glslangValidatorExecutable(GetVulkanSdkBinariesDirectory());
	glslangValidatorExecutable.append("glslangValidator.exe");

	std::filesystem::path spirvFile(rPreProcessedFile);
	spirvFile += ".spv";
	std::filesystem::remove(spirvFile);

	std::wstring commandLineParameters = L"";
	commandLineParameters += L" -g0"; // Strip debug info
	commandLineParameters += L" -V";      // Generate binary
	commandLineParameters += L" --target-env vulkan1.2"; // Also update VK_API_VERSION_1_2 in engine
	commandLineParameters += L" -o \"" + spirvFile.native() + L"\"";
	commandLineParameters += L" \"" + rPreProcessedFile.native() + L"\"";

	return RunVulkanTool(glslangValidatorExecutable, commandLineParameters, spirvFile, /*bThrowOnAnyOutput*/ false);
}

std::filesystem::path ExportShader::OptimizeShader(const std::filesystem::path& rSpirvFile)
{
	std::filesystem::path spirvOptExecutable(GetVulkanSdkBinariesDirectory());
	spirvOptExecutable.append("spirv-opt.exe");

	std::filesystem::path optimizedSpirvFile(rSpirvFile);
	optimizedSpirvFile += ".opt.spv";
	std::filesystem::remove(optimizedSpirvFile);

	std::wstring spirvOptCommandLineParameters = L"";
	spirvOptCommandLineParameters += L" -O";
	spirvOptCommandLineParameters += L" --target-env=vulkan1.2";
	spirvOptCommandLineParameters += L" --scalar-block-layout";
	spirvOptCommandLineParameters += L" -o \"" + optimizedSpirvFile.native() + L"\"";
	spirvOptCommandLineParameters += L" \"" + rSpirvFile.native() + L"\"";

	return RunVulkanTool(spirvOptExecutable, spirvOptCommandLineParameters, optimizedSpirvFile, /*bThrowOnAnyOutput*/ false);
}

void ExportShader::ReflectAndWriteShader(const std::filesystem::path& rSpirvFile)
{
	std::vector<std::byte> spirvData = common::ReadEntireFile(rSpirvFile);
	int64_t iSpirvFileBytes = std::ssize(spirvData);

	VkDescriptorSetLayoutBinding pVkDescriptorSetLayoutBindings[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
	uint32_t puiSetIndices[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
	VkVertexInputAttributeDescription pVkVertexInputAttributeDescriptions[common::ShaderHeader::kiMaxVertexInputAttributeDescriptions] {};
	int64_t iBindingCount = 0;
	int64_t iAttributeCount = 0;
	int64_t iVertexInputStride = 0;

	spirv_cross::Compiler spirvCrossCompiler(reinterpret_cast<uint32_t*>(spirvData.data()), iSpirvFileBytes / sizeof(uint32_t));
	spirv_cross::ShaderResources shaderResources = spirvCrossCompiler.get_shader_resources();

	// stage_inputs can be reported out of order; sort by location once, then process ascending
	std::vector<std::pair<int64_t, const spirv_cross::Resource*>> sortedStageInputs;
	sortedStageInputs.reserve(shaderResources.stage_inputs.size());
	for (const spirv_cross::Resource& rResource : shaderResources.stage_inputs)
	{
		sortedStageInputs.emplace_back(spirvCrossCompiler.get_decoration(rResource.id, spv::DecorationLocation), &rResource);
	}
	std::sort(sortedStageInputs.begin(), sortedStageInputs.end(), [](const std::pair<int64_t, const spirv_cross::Resource*>& rLeftHandSide, const std::pair<int64_t, const spirv_cross::Resource*>& rRightHandSide) { return rLeftHandSide.first < rRightHandSide.first; });

	for (const auto& [iLocation, pResource] : sortedStageInputs)
	{
		const spirv_cross::SPIRType& rSpirvType = spirvCrossCompiler.get_type(pResource->type_id);
		LOG(kDefault, kVerbose,"   {} {} {} size {}", static_cast<uint32_t>(pResource->type_id), static_cast<uint32_t>(pResource->base_type_id), pResource->name, rSpirvType.vecsize);

		ASSERT(iLocation < common::ShaderHeader::kiMaxVertexInputAttributeDescriptions);
		// Format mapping below assumes 32-bit float inputs (SPIRV-Cross Half/Double are distinct basetypes) — an int attribute would be silently mis-typed.
		// Vertex stage only: fragment interpolants are legitimately flat int/uint and never feed VkVertexInputAttributeDescription
		ASSERT(rSpirvType.basetype == spirv_cross::SPIRType::Float || !(mChunkFlags & common::ChunkFlags::kVertex));
		VkVertexInputAttributeDescription& rVkVertexInputAttributeDescription = pVkVertexInputAttributeDescriptions[iLocation];
		rVkVertexInputAttributeDescription.location = static_cast<uint32_t>(iLocation);
		rVkVertexInputAttributeDescription.binding = 0;
		rVkVertexInputAttributeDescription.format = rSpirvType.vecsize == 1 ? VK_FORMAT_R32_SFLOAT : (rSpirvType.vecsize == 2 ? VK_FORMAT_R32G32_SFLOAT : (rSpirvType.vecsize == 3 ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT));
		rVkVertexInputAttributeDescription.offset = static_cast<uint32_t>(iVertexInputStride);
		++iAttributeCount;

		LOG(kDefault, kVerbose,"       location {} binding {} format {} offset {}", rVkVertexInputAttributeDescription.location, rVkVertexInputAttributeDescription.binding, static_cast<int64_t>(rVkVertexInputAttributeDescription.format), rVkVertexInputAttributeDescription.offset);

		iVertexInputStride += rSpirvType.vecsize * sizeof(float);
	}
	LOG(kDefault, kVerbose, "   Descriptions: {} Input stride: {}", iAttributeCount, iVertexInputStride);

	if (std::ssize(shaderResources.stage_outputs) > 0)
	{
		LOG(kDefault, kVerbose,"Stage outputs:");
		for (const spirv_cross::Resource& rResource : shaderResources.stage_outputs)
		{
			int64_t iBinding = spirvCrossCompiler.get_decoration(rResource.id, spv::DecorationBinding);
			LOG(kDefault, kVerbose,"   {} {} {} bound at {}", static_cast<uint32_t>(rResource.type_id), static_cast<uint32_t>(rResource.base_type_id), rResource.name, iBinding);
		}
	}

	auto ConstantOne = [](const spirv_cross::SPIRType&) -> int64_t { return 1; };
	// Unsized arrays report array[0] == 0 in these categories; a 0 descriptorCount disarms the WriteBinding double-write guard, so fail loud (no current shader hits this)
	auto ArrayCount = [](const spirv_cross::SPIRType& rType) -> int64_t { ASSERT(rType.array.empty() || rType.array[0] != 0); return rType.array.empty() ? 1 : rType.array[0]; };
	// Runtime-sized arrays (unsized) report array[0] == 0; use UINT32_MAX sentinel for pipeline to resolve
	auto RuntimeArrayCount = [](const spirv_cross::SPIRType& rType) -> int64_t { return rType.array.empty() ? 1 : (rType.array[0] == 0 ? std::numeric_limits<uint32_t>::max() : rType.array[0]); };

	BindingTable bindingTable {.pVkDescriptorSetLayoutBindings = pVkDescriptorSetLayoutBindings, .puiSetIndices = puiSetIndices, .riBindingCount = iBindingCount, .chunkFlags = mChunkFlags};
	CollectBindings(shaderResources.uniform_buffers, "Uniform buffers:", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, spirvCrossCompiler, bindingTable, ConstantOne);
	CollectBindings(shaderResources.storage_buffers, "Storage buffers:", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, spirvCrossCompiler, bindingTable, ArrayCount);
	CollectBindings(shaderResources.sampled_images, "Sampled images:", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, spirvCrossCompiler, bindingTable, ArrayCount);
	CollectBindings(shaderResources.storage_images, "Storage images:", VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, spirvCrossCompiler, bindingTable, ArrayCount);
	CollectBindings(shaderResources.separate_images, "Separate images:", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, spirvCrossCompiler, bindingTable, RuntimeArrayCount);
	CollectBindings(shaderResources.separate_samplers, "Separate samplers:", VK_DESCRIPTOR_TYPE_SAMPLER, spirvCrossCompiler, bindingTable, ConstantOne);

	// Allocate data span: [bindings ALIGN16] [setIndices ALIGN16] [attrs ALIGN16] [SPIR-V]
	auto [pHeader, dataSpan] = AllocateHeaderAndData(common::ShaderHeader::SpirvOffset(iBindingCount, iAttributeCount) + iSpirvFileBytes);
	pHeader->shaderHeader = common::ShaderHeader {};
	pHeader->shaderHeader.iDescriptorSetLayoutBindings = iBindingCount;
	pHeader->shaderHeader.iVertexInputAttributeDescriptions = iAttributeCount;
	pHeader->shaderHeader.iVertexInputStride = iVertexInputStride;

	std::memcpy(dataSpan.data(), pVkDescriptorSetLayoutBindings, iBindingCount * sizeof(VkDescriptorSetLayoutBinding));
	std::memcpy(dataSpan.data() + common::ShaderHeader::SetIndicesOffset(iBindingCount), puiSetIndices, iBindingCount * sizeof(uint32_t));
	std::memcpy(dataSpan.data() + common::ShaderHeader::AttributesOffset(iBindingCount), pVkVertexInputAttributeDescriptions, iAttributeCount * sizeof(VkVertexInputAttributeDescription));

	std::memcpy(dataSpan.data() + common::ShaderHeader::SpirvOffset(iBindingCount, iAttributeCount), spirvData.data(), iSpirvFileBytes);

	ASSERT(*reinterpret_cast<uint32_t*>(dataSpan.data() + common::ShaderHeader::SpirvOffset(iBindingCount, iAttributeCount)) == common::ShaderHeader::kiSpirvMagic);
}

void ExportShader::CleanupOnFailure()
{
	for (const std::filesystem::path& rPath : mIntermediateFiles)
	{
		std::filesystem::remove(rPath);
	}
	mIntermediateFiles.clear();
}

constexpr int64_t kiDependencyMetadataMagic = 0x53484445504D5431;
constexpr int64_t kiDependencyMetadataVersion = 2;
constexpr int64_t kiFingerprintCharacters = 64;

struct CachedDependencyFingerprint
{
	int64_t iInputRoot = 0;
	std::filesystem::path relativePath;
	std::string fingerprint;
};

static bool IsDependencyInInputRoot(const std::filesystem::path& rDependency)
{
	if (!std::filesystem::exists(rDependency))
	{
		return false;
	}
	std::filesystem::path dependency = std::filesystem::weakly_canonical(rDependency);
	for (const std::filesystem::path& rInputRoot : gpFileManager->mpInputDirectories)
	{
		std::error_code error;
		std::filesystem::path relativePath = std::filesystem::relative(dependency, rInputRoot, error);
		if (!error && !relativePath.empty() && *relativePath.begin() != "..")
		{
			return true;
		}
	}
	return false;
}

static std::optional<std::vector<CachedDependencyFingerprint>> ReadDependencyMetadata(const std::filesystem::path& rPath)
{
	std::fstream stream(rPath, std::ios::in | std::ios::binary);
	int64_t iMagic = 0;
	int64_t iVersion = 0;
	int64_t iCount = 0;
	stream.read(reinterpret_cast<char*>(&iMagic), sizeof(iMagic));
	stream.read(reinterpret_cast<char*>(&iVersion), sizeof(iVersion));
	stream.read(reinterpret_cast<char*>(&iCount), sizeof(iCount));
	if (!stream)
	{
		return std::nullopt;
	}
	if (iMagic != kiDependencyMetadataMagic)
	{
		return std::nullopt;
	}
	if (iVersion != kiDependencyMetadataVersion)
	{
		return std::nullopt;
	}
	if (iCount < 0 || iCount > 10'000)
	{
		return std::nullopt;
	}

	std::vector<CachedDependencyFingerprint> dependencies;
	dependencies.reserve(static_cast<size_t>(iCount));
	for (int64_t iDependency = 0; iDependency < iCount; ++iDependency)
	{
		int64_t iInputRoot = 0;
		int64_t iPathCharacters = 0;
		stream.read(reinterpret_cast<char*>(&iInputRoot), sizeof(iInputRoot));
		stream.read(reinterpret_cast<char*>(&iPathCharacters), sizeof(iPathCharacters));
		if (!stream)
		{
			return std::nullopt;
		}
		if (iInputRoot < 0 || iInputRoot >= static_cast<int64_t>(std::size(gpFileManager->mpInputDirectories)))
		{
			return std::nullopt;
		}
		if (iPathCharacters <= 0 || iPathCharacters > MAX_PATH * 4)
		{
			return std::nullopt;
		}
		std::string relativePath(static_cast<size_t>(iPathCharacters), '\0');
		std::string fingerprint(static_cast<size_t>(kiFingerprintCharacters), '\0');
		stream.read(relativePath.data(), relativePath.size());
		stream.read(fingerprint.data(), fingerprint.size());
		if (!stream)
		{
			return std::nullopt;
		}
		std::filesystem::path relativeDependencyPath = std::filesystem::path(relativePath).lexically_normal();
		if (relativeDependencyPath.is_absolute())
		{
			return std::nullopt;
		}
		if (*relativeDependencyPath.begin() == "..")
		{
			return std::nullopt;
		}
		dependencies.emplace_back(CachedDependencyFingerprint
		{
			.iInputRoot = iInputRoot,
			.relativePath = std::move(relativeDependencyPath),
			.fingerprint = std::move(fingerprint),
		});
	}
	return dependencies;
}


static std::string ReadAndValidateDependencyFile(const std::filesystem::path& rDependencyFilePath)
{
	std::fstream fileStream(rDependencyFilePath, std::ios::in);
	std::string content((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
	if (!fileStream && !fileStream.eof())
	{
		throw std::runtime_error(std::format("Failed to read shader dependency file \"{}\"", rDependencyFilePath.string()));
	}

	// shaderc emits the target separator literally as ": ". A Windows drive colon is followed by a
	// slash, so searching for the full separator avoids cutting the target at "C:".
	int64_t iTargetSeparator = static_cast<int64_t>(content.find(": "));
	if (iTargetSeparator == static_cast<int64_t>(std::string::npos))
	{
		throw std::runtime_error(std::format("Shader dependency file \"{}\" has no ': ' target separator", rDependencyFilePath.string()));
	}
	content.erase(0, static_cast<size_t>(iTargetSeparator + 2));
	while (!content.empty() && (content.back() == '\r' || content.back() == '\n'))
	{
		content.pop_back();
	}
	if (content.empty())
	{
		throw std::runtime_error(std::format("Shader dependency file \"{}\" is empty or contains an unsupported continuation", rDependencyFilePath.string()));
	}
	if (content.find_first_of("\r\n") != std::string::npos)
	{
		throw std::runtime_error(std::format("Shader dependency file \"{}\" is empty or contains an unsupported continuation", rDependencyFilePath.string()));
	}
	return content;
}

static std::vector<std::string> BuildDependencyRootPrefixes()
{
	std::vector<std::string> rootPrefixes;
	for (const std::filesystem::path& rInputRoot : gpFileManager->mpInputDirectories)
	{
		for (std::string rootPrefix : {rInputRoot.string(), rInputRoot.generic_string()})
		{
			std::string lowerPrefix = common::ToLower(rootPrefix);
			if (std::ranges::none_of(rootPrefixes, [&lowerPrefix](std::string_view existing)
			{
				return common::ToLower(existing) == lowerPrefix;
			}))
			{
				rootPrefixes.push_back(std::move(rootPrefix));
			}
		}
	}
	std::ranges::sort(rootPrefixes, [](std::string_view left, std::string_view right)
	{
		return left.size() > right.size();
	});
	return rootPrefixes;
}

static const std::string* FindMatchingDependencyRoot(const std::vector<std::string>& rRootPrefixes, std::string_view lowerContent, int64_t iOffset)
{
	for (const std::string& rRootPrefix : rRootPrefixes)
	{
		std::string lowerPrefix = common::ToLower(rRootPrefix);
		if (iOffset + std::ssize(lowerPrefix) <= std::ssize(lowerContent) && lowerContent.compare(static_cast<size_t>(iOffset), lowerPrefix.size(), lowerPrefix) == 0
		 && (iOffset + std::ssize(lowerPrefix) == std::ssize(lowerContent) || lowerContent[static_cast<size_t>(iOffset + std::ssize(lowerPrefix))] == '\\' || lowerContent[static_cast<size_t>(iOffset + std::ssize(lowerPrefix))] == '/'))
		{
			return &rRootPrefix;
		}
	}
	return nullptr;
}

static std::vector<std::filesystem::path> ParseRootDelimitedDependencies(std::string_view content, const std::vector<std::string>& rRootPrefixes, std::string_view lowerContent)
{
	std::vector<std::filesystem::path> dependencies;
	int64_t iDependencyStart = 0;
	while (iDependencyStart < std::ssize(content))
	{
		ASSERT(FindMatchingDependencyRoot(rRootPrefixes, lowerContent, iDependencyStart) != nullptr);

		int64_t iDependencyEnd = std::ssize(content);
		for (int64_t iSpace = static_cast<int64_t>(content.find(' ', static_cast<size_t>(iDependencyStart))); iSpace != static_cast<int64_t>(std::string_view::npos); iSpace = static_cast<int64_t>(content.find(' ', static_cast<size_t>(iSpace + 1))))
		{
			if (FindMatchingDependencyRoot(rRootPrefixes, lowerContent, iSpace + 1) != nullptr)
			{
				iDependencyEnd = iSpace;
				break;
			}
		}

		std::filesystem::path dependency(content.substr(static_cast<size_t>(iDependencyStart), static_cast<size_t>(iDependencyEnd - iDependencyStart)));
		if (!IsDependencyInInputRoot(dependency))
		{
			throw std::runtime_error(std::format("Shader dependency \"{}\" is ambiguous, missing, or outside DataPacker input roots", dependency.string()));
		}
		dependencies.push_back(std::move(dependency));
		iDependencyStart = iDependencyEnd == std::ssize(content) ? std::ssize(content) : iDependencyEnd + 1;
	}
	return dependencies;
}

static std::vector<std::filesystem::path> ParseWhitespaceDependencies(const std::filesystem::path& rDependencyFilePath, std::string_view content)
{
	// Each whitespace token must independently resolve to an existing dependency under an input root; invalid tokens throw.
	std::vector<std::filesystem::path> dependencies;
	std::ispanstream stream(content);
	std::string token;
	while (stream >> token)
	{
		std::filesystem::path dependency(token);
		if (!IsDependencyInInputRoot(dependency))
		{
			throw std::runtime_error(std::format("Shader dependency file \"{}\" cannot unambiguously delimit \"{}\" using DataPacker input roots", rDependencyFilePath.string(), content));
		}
		dependencies.push_back(std::move(dependency));
	}
	return dependencies;
}

static std::vector<std::filesystem::path> ParseDependencyFile(const std::filesystem::path& rDependencyFilePath)
{
	std::string content = ReadAndValidateDependencyFile(rDependencyFilePath);

	// shaderc writes dependency names verbatim with spaces between entries; it does not escape spaces
	// inside filenames. Known canonical input-root prefixes are therefore the only lossless delimiters.
	std::vector<std::string> rootPrefixes = BuildDependencyRootPrefixes();
	std::string lowerContent = common::ToLower(content);
	if (FindMatchingDependencyRoot(rootPrefixes, lowerContent, 0) != nullptr)
	{
		return ParseRootDelimitedDependencies(content, rootPrefixes, lowerContent);
	}
	return ParseWhitespaceDependencies(rDependencyFilePath, content);
}

bool ExportShader::CheckDirty(const std::filesystem::path& rPackFile)
{
	if (ExportJob::CheckDirty(rPackFile))
	{
		return true;
	}

	std::optional<std::vector<CachedDependencyFingerprint>> dependencies = ReadDependencyMetadata(mDependencyMetadataFile);
	if (!dependencies.has_value())
	{
		mbDirty = true;
		return true;
	}

	for (const CachedDependencyFingerprint& rDependency : dependencies.value())
	{
		std::filesystem::path dependencyPath = gpFileManager->mpInputDirectories[rDependency.iInputRoot] / rDependency.relativePath;
		if (!std::filesystem::exists(dependencyPath))
		{
			mbDirty = true;
			return true;
		}
		if (gpFileManager->mpInputFingerprintCache->Get(dependencyPath) != rDependency.fingerprint)
		{
			LOG(kDefault, kDebug, "Shader dependency changed: \"{}\"", dependencyPath.string());
			mbDirty = true;
			return mbDirty;
		}
	}

	return mbDirty;
}

void ExportShader::CaptureDependencies()
{
	mDependencyFingerprints.clear();
	std::vector<std::filesystem::path> dependencies = ParseDependencyFile(mDependencyFile);
	if (dependencies.empty())
	{
		throw std::runtime_error(std::format("Shader dependency file \"{}\" is empty or invalid", mDependencyFile.string()));
	}
	for (const std::filesystem::path& rDependency : dependencies)
	{
		std::filesystem::path dependency = std::filesystem::weakly_canonical(rDependency);
		bool bFoundRoot = false;
		for (int64_t iRoot = 0; iRoot < static_cast<int64_t>(std::size(gpFileManager->mpInputDirectories)); ++iRoot)
		{
			std::error_code error;
			std::filesystem::path relativePath = std::filesystem::relative(dependency, gpFileManager->mpInputDirectories[iRoot], error);
			if (error)
			{
				continue;
			}
			if (relativePath.empty())
			{
				continue;
			}
			if (*relativePath.begin() == "..")
			{
				continue;
			}
			mDependencyFingerprints.emplace_back(DependencyFingerprint
			{
				.iInputRoot = iRoot,
				.relativePath = std::move(relativePath),
				.fingerprint = gpFileManager->mpInputFingerprintCache->Get(dependency),
			});
			bFoundRoot = true;
			break;
		}
		ASSERT(bFoundRoot);
	}
}

void ExportShader::UpdateCacheMetadata()
{
	std::fstream stream(mDependencyMetadataFile, std::ios::out | std::ios::binary);
	int64_t iCount = std::ssize(mDependencyFingerprints);
	stream.write(reinterpret_cast<const char*>(&kiDependencyMetadataMagic), sizeof(kiDependencyMetadataMagic));
	stream.write(reinterpret_cast<const char*>(&kiDependencyMetadataVersion), sizeof(kiDependencyMetadataVersion));
	stream.write(reinterpret_cast<const char*>(&iCount), sizeof(iCount));
	for (const DependencyFingerprint& rDependency : mDependencyFingerprints)
	{
		std::string relativePath = rDependency.relativePath.generic_string();
		int64_t iPathCharacters = static_cast<int64_t>(relativePath.size());
		stream.write(reinterpret_cast<const char*>(&rDependency.iInputRoot), sizeof(rDependency.iInputRoot));
		stream.write(reinterpret_cast<const char*>(&iPathCharacters), sizeof(iPathCharacters));
		stream.write(relativePath.data(), relativePath.size());
		stream.write(rDependency.fingerprint.data(), rDependency.fingerprint.size());
	}
	stream.close();
	VERIFY_SUCCESS(stream.good());
}
