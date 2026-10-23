#include "BlenderExport.h"

#include "FileManager.h"


static std::string BlenderPathText(const std::filesystem::path& rPath)
{
	std::u8string text = rPath.generic_u8string();
	return std::string(text.begin(), text.end());
}

static std::filesystem::path BlenderPathFromText(std::string_view text)
{
	return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

static bool IsBlender52(const std::filesystem::path& rExecutable)
{
	HMODULE hmodule = LoadLibraryW(L"version.dll");
	if (hmodule == nullptr)
	{
		return false;
	}
	std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&FreeLibrary)> pModule(hmodule, FreeLibrary);
	DWORD (WINAPI* GetSize)(LPCWSTR, LPDWORD) = reinterpret_cast<DWORD(WINAPI*)(LPCWSTR, LPDWORD)>(GetProcAddress(hmodule, "GetFileVersionInfoSizeW"));
	BOOL (WINAPI* GetInfo)(LPCWSTR, DWORD, DWORD, LPVOID) = reinterpret_cast<BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID)>(GetProcAddress(hmodule, "GetFileVersionInfoW"));
	BOOL (WINAPI* Query)(LPCVOID, LPCWSTR, LPVOID*, PUINT) = reinterpret_cast<BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT)>(GetProcAddress(hmodule, "VerQueryValueW"));
	if (GetSize == nullptr || GetInfo == nullptr || Query == nullptr)
	{
		return false;
	}
	DWORD uiIgnored = 0;
	DWORD uiSize = GetSize(rExecutable.c_str(), &uiIgnored);
	if (uiSize == 0)
	{
		return false;
	}
	std::vector<std::byte> bytes(uiSize);
	if (!GetInfo(rExecutable.c_str(), 0, uiSize, bytes.data()))
	{
		return false;
	}
	VS_FIXEDFILEINFO* pInfo = nullptr;
	UINT uiInfoSize = 0;
	if (!Query(bytes.data(), L"\\", reinterpret_cast<void**>(&pInfo), &uiInfoSize) || uiInfoSize < sizeof(VS_FIXEDFILEINFO))
	{
		return false;
	}
	return pInfo->dwSignature == 0xFEEF04BD && HIWORD(pInfo->dwFileVersionMS) == 5 && LOWORD(pInfo->dwFileVersionMS) == 2;
}

static std::filesystem::path FindBlender()
{
	std::vector<std::filesystem::path> candidates;
	for (const wchar_t* pcVariable : {L"ProgramW6432", L"ProgramFiles", L"LOCALAPPDATA"})
	{
		const wchar_t* pcDirectory = _wgetenv(pcVariable);
		if (pcDirectory == nullptr)
		{
			continue;
		}
		std::filesystem::path directory(pcDirectory);
		if (std::wstring_view(pcVariable) == L"LOCALAPPDATA")
		{
			directory /= "Programs";
		}
		directory /= "Blender Foundation";
		if (!std::filesystem::is_directory(directory))
		{
			continue;
		}
		for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(directory))
		{
			candidates.push_back(rEntry.path() / "blender.exe");
		}
	}
	std::ranges::sort(candidates);
	const wchar_t* pcPath = _wgetenv(L"PATH");
	if (pcPath != nullptr)
	{
		std::wistringstream stream(pcPath);
		std::wstring directory;
		while (std::getline(stream, directory, L';'))
		{
			if (directory.starts_with(L'"') && directory.ends_with(L'"'))
			{
				directory = directory.substr(1, directory.size() - 2);
			}
			if (!directory.empty())
			{
				candidates.push_back(std::filesystem::path(directory) / "blender.exe");
			}
		}
	}
	for (const std::filesystem::path& rCandidate : candidates)
	{
		if (std::filesystem::is_regular_file(rCandidate) && IsBlender52(rCandidate))
		{
			return std::filesystem::canonical(rCandidate);
		}
	}
	throw std::runtime_error("Blender 5.2 LTS was not found in installed programs or PATH");
}

static std::filesystem::path BlenderScript()
{
	return gpFileManager->mpInputDirectories[0].parent_path().parent_path() / "DataPacker" / "Scripts" / "ExportBlenderScene.py";
}

static nlohmann::json BlenderRecipe(const std::filesystem::path& rSource, const std::filesystem::path& rExecutable)
{
	return nlohmann::json
	{
		{"version", kiBlenderExportVersion},
		{"blenderVersion", "5.2"},
		{"tool", BlenderPathText(rExecutable)},
		{"toolHash", gpFileManager->mpInputFingerprintCache->Get(rExecutable)},
		{"scriptHash", gpFileManager->mpInputFingerprintCache->Get(BlenderScript())},
		{"sourceHash", gpFileManager->mpInputFingerprintCache->Get(rSource)},
	};
}

static nlohmann::json BlenderDependency(const std::filesystem::path& rSource, const std::filesystem::path& rDependency)
{
	std::filesystem::path dependency = std::filesystem::canonical(rDependency);
	for (int64_t i = 0; i < std::ssize(gpFileManager->mpInputDirectories); ++i)
	{
		std::filesystem::path relative = dependency.lexically_relative(gpFileManager->mpInputDirectories[i]);
		if (!relative.empty() && *relative.begin() != "..")
		{
			return nlohmann::json {{"root", i}, {"path", BlenderPathText(relative)}, {"hash", gpFileManager->mpInputFingerprintCache->Get(dependency)}};
		}
	}
	std::filesystem::path relative = dependency.lexically_relative(std::filesystem::canonical(rSource).parent_path());
	return nlohmann::json {{"root", -1}, {"path", BlenderPathText(relative.empty() ? dependency : relative)}, {"hash", gpFileManager->mpInputFingerprintCache->Get(dependency)}};
}

bool BlenderExportDirty(const std::filesystem::path& rSource, const std::filesystem::path& rOutput, const std::optional<std::string>& rOptionalMarker) noexcept
{
	try
	{
		if (!rOptionalMarker.has_value())
		{
			return true;
		}
		nlohmann::json marker = nlohmann::json::parse(*rOptionalMarker);
		if (marker.at("recipe") != BlenderRecipe(rSource, FindBlender()))
		{
			return true;
		}
		if (marker.at("outputHash") != gpFileManager->mpInputFingerprintCache->Get(rOutput))
		{
			return true;
		}
		if (!marker.at("dependencies").is_array())
		{
			return true;
		}
		for (const nlohmann::json& rDependency : marker.at("dependencies"))
		{
			int64_t iRoot = rDependency.at("root").get<int64_t>();
			std::filesystem::path relative = BlenderPathFromText(rDependency.at("path").get<std::string>());
			if (relative.empty())
			{
				return true;
			}
			if (iRoot < -1 || iRoot >= std::ssize(gpFileManager->mpInputDirectories))
			{
				return true;
			}
			if (iRoot >= 0 && (relative.is_absolute() || *relative.begin() == ".."))
			{
				return true;
			}
			std::filesystem::path dependency = iRoot == -1 ? rSource.parent_path() / relative : gpFileManager->mpInputDirectories[iRoot] / relative;
			if (rDependency.at("hash") != gpFileManager->mpInputFingerprintCache->Get(dependency))
			{
				return true;
			}
		}
		return false;
	}
	catch (const std::exception&)
	{
		return true;
	}
}

std::string ExportBlenderScene(const std::filesystem::path& rSource, const std::filesystem::path& rOutput, const std::function<void(const std::filesystem::path&)>& rValidate, const std::function<void()>& rInvalidate)
{
	std::filesystem::path executable = FindBlender();
	nlohmann::json recipe = BlenderRecipe(rSource, executable);
	std::filesystem::path stage = rOutput;
	stage += ".blender-stage";
	std::filesystem::path dependencies = rOutput;
	dependencies += ".blender-dependencies";
	try
	{
		std::filesystem::remove(stage);
		std::filesystem::remove(dependencies);
		std::wstring commandLine = L"\"" + executable.native() + L"\" --background --factory-startup --disable-autoexec --python-exit-code 1 --python \"" + BlenderScript().native() + L"\" -- \"" + rSource.native() + L"\" \"" + stage.native() + L"\" \"" + dependencies.native() + L"\"";
		LOG(kDefault, kInfo, "Exporting Blender scene: \"{}\"", rSource.string());
		std::optional<common::ExecutableResult> optionalResult = common::RunExecutable(executable, commandLine);
		if (!optionalResult.has_value())
		{
			throw std::runtime_error(std::format("Unable to launch Blender for \"{}\"", rSource.string()));
		}
		if (optionalResult->iExitCode != 0)
		{
			throw std::runtime_error(std::format("Blender failed for \"{}\" (exit {}): {}", rSource.string(), optionalResult->iExitCode, optionalResult->output));
		}
		std::ifstream stream(dependencies, std::ios::binary);
		nlohmann::json report = nlohmann::json::parse(stream);
		stream.close();
		if (!report.is_array())
		{
			throw std::runtime_error("Blender dependency report is not an array");
		}
		nlohmann::json marker {{"recipe", recipe}, {"dependencies", nlohmann::json::array()}};
		for (const nlohmann::json& rDependency : report)
		{
			marker.at("dependencies").push_back(BlenderDependency(rSource, BlenderPathFromText(rDependency.get<std::string>())));
		}
		if (recipe != BlenderRecipe(rSource, executable))
		{
			throw std::runtime_error("Blender conversion inputs changed during export");
		}
		rValidate(stage);
		marker["outputHash"] = gpFileManager->mpInputFingerprintCache->Get(stage);
		rInvalidate();
		if (!MoveFileExW(stage.c_str(), rOutput.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			throw std::runtime_error(std::format("Unable to publish Blender scene \"{}\": {}", rOutput.string(), common::LastErrorString()));
		}
		std::filesystem::remove(dependencies);
		return marker.dump();
	}
	catch (...)
	{
		std::error_code error;
		std::filesystem::remove(stage, error);
		std::filesystem::remove(dependencies, error);
		throw;
	}
}
