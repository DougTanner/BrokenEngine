#include "Attribution.h"

#include "DiagnosticReporter.h"
#include "FileManager.h"

namespace attribution
{

namespace
{
struct PendingCopy
{
	std::filesystem::path source;
	std::filesystem::path destination;
	std::string libraryName;
};

bool PathLess(const std::filesystem::path& rLeft, const std::filesystem::path& rRight)
{
	int iResult = CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, TRUE);
	return iResult == CSTR_LESS_THAN || (iResult == CSTR_EQUAL && CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, FALSE) == CSTR_LESS_THAN);
}

bool IsReparsePoint(const std::filesystem::path& rPath)
{
	DWORD uiAttributes = GetFileAttributesW(rPath.native().c_str());
	return uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

std::vector<std::filesystem::directory_entry> DiscoverLibraries(const std::filesystem::path& rThirdPartyDirectory)
{
	std::vector<std::filesystem::directory_entry> libraries;
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(rThirdPartyDirectory))
	{
		if (IsReparsePoint(rEntry.path()))
		{
			throw std::runtime_error(std::format("Unsupported ThirdParty reparse point: {}", rEntry.path().string()));
		}
		if (rEntry.symlink_status().type() == std::filesystem::file_type::symlink)
		{
			throw std::runtime_error(std::format("Unsupported ThirdParty reparse point: {}", rEntry.path().string()));
		}
		libraries.push_back(rEntry);
	}
	std::sort(libraries.begin(), libraries.end(), [](const std::filesystem::directory_entry& rLeft, const std::filesystem::directory_entry& rRight)
	{
		return PathLess(rLeft.path(), rRight.path());
	});
	return libraries;
}

std::vector<std::filesystem::directory_entry> EnumerateLibraryFiles(const std::filesystem::path& rLibraryDirectory)
{
	std::vector<std::filesystem::directory_entry> files;
	for (const std::filesystem::directory_entry& rFile : std::filesystem::directory_iterator(rLibraryDirectory))
	{
		std::filesystem::file_status fileStatus = rFile.symlink_status();
		if (IsReparsePoint(rFile.path()))
		{
			throw std::runtime_error(std::format("Unsupported attribution reparse point: {}", rFile.path().string()));
		}
		if (fileStatus.type() == std::filesystem::file_type::symlink)
		{
			throw std::runtime_error(std::format("Unsupported attribution reparse point: {}", rFile.path().string()));
		}
		if (std::filesystem::is_directory(fileStatus))
		{
			continue;
		}
		if (!std::filesystem::is_regular_file(fileStatus))
		{
			throw std::runtime_error(std::format("Unsupported attribution entry: {}", rFile.path().string()));
		}
		files.push_back(rFile);
	}
	std::sort(files.begin(), files.end(), [](const std::filesystem::directory_entry& rLeft, const std::filesystem::directory_entry& rRight)
	{
		return PathLess(rLeft.path().filename(), rRight.path().filename());
	});
	return files;
}

std::vector<std::filesystem::path> SelectLicenseFiles(const std::vector<std::filesystem::directory_entry>& rFiles)
{
	// Priority 1: Look for primary license files (LICENSE, LICENSE.md, LICENSE.txt)
	bool bFoundLicense = false;
	std::filesystem::path primaryLicenseFile;
	for (std::string_view priority : { "license", "license.md", "license.txt" })
	{
		for (const std::filesystem::directory_entry& rFileEntry : rFiles)
		{
			if (common::ToLower(rFileEntry.path().filename().string()) == priority)
			{
				primaryLicenseFile = rFileEntry.path();
				bFoundLicense = true;
				break;
			}
		}
		if (bFoundLicense)
		{
			break;
		}
	}

	// If primary license found, copy it and skip fallback search
	if (bFoundLicense)
	{
		return { primaryLicenseFile };
	}

	// Fallback: Search for alternative license/attribution files (copying, readme, manual.md)
	std::vector<std::filesystem::path> licenseFiles;
	for (const std::filesystem::directory_entry& rFileEntry : rFiles)
	{
		std::string filenameLower = common::ToLower(rFileEntry.path().filename().string());
		if (filenameLower.find("copying") != std::string::npos || filenameLower == "manual.md" || filenameLower.find("readme") != std::string::npos)
		{
			bFoundLicense = true;
			licenseFiles.push_back(rFileEntry.path());
		}
	}

	ASSERT(bFoundLicense);
	return licenseFiles;
}

void AppendPendingCopy(std::vector<PendingCopy>& rPendingCopies, const std::filesystem::path& rSourceFile, const std::filesystem::path& rDestination, std::string_view libraryName)
{
	if (!std::filesystem::exists(rDestination) || std::filesystem::last_write_time(rSourceFile) > std::filesystem::last_write_time(rDestination))
	{
		rPendingCopies.push_back({.source = rSourceFile, .destination = rDestination, .libraryName = std::string(libraryName)});
	}
}

std::vector<PendingCopy> BuildPendingCopies(const std::filesystem::path& rThirdPartyDirectory, const std::filesystem::path& rAttributionDirectory)
{
	std::vector<PendingCopy> pendingCopies;
	std::vector<std::filesystem::directory_entry> libraries = DiscoverLibraries(rThirdPartyDirectory);
	for (const std::filesystem::directory_entry& rDirectoryEntry : libraries)
	{
		std::filesystem::file_status directoryStatus = rDirectoryEntry.symlink_status();
		if (!std::filesystem::is_directory(directoryStatus))
		{
			if (!std::filesystem::is_regular_file(directoryStatus))
			{
				throw std::runtime_error(std::format("Unsupported ThirdParty entry: {}", rDirectoryEntry.path().string()));
			}
			continue;
		}

		std::string libraryName = rDirectoryEntry.path().filename().string();
		if (libraryName == "Prebuilts")
		{
			continue;
		}
		std::filesystem::path libraryAttributionDirectory = rAttributionDirectory / libraryName;
		std::vector<std::filesystem::directory_entry> files = EnumerateLibraryFiles(rDirectoryEntry.path());
		std::vector<std::filesystem::path> licenseFiles = SelectLicenseFiles(files);
		for (const std::filesystem::path& rLicenseFile : licenseFiles)
		{
			AppendPendingCopy(pendingCopies, rLicenseFile, libraryAttributionDirectory / rLicenseFile.filename(), libraryName);
		}
	}
	return pendingCopies;
}

void PublishPendingCopies(const std::vector<PendingCopy>& rPendingCopies)
{
	LOG(kDefault, kDebug, "\nCopying ThirdParty attribution files");
	ScopedLogIndent scopedLogIndent;
	for (const PendingCopy& rPending : rPendingCopies)
	{
		std::filesystem::create_directories(rPending.destination.parent_path());
		std::filesystem::copy_file(rPending.source, rPending.destination, std::filesystem::copy_options::overwrite_existing);
		LOG(kDefault, kDebug, "Copied: {}/{}", rPending.libraryName, rPending.source.filename().string());
	}
}
}

void CopyThirdPartyLicenses()
{
	std::filesystem::path thirdPartyDirectory = gpFileManager->mThirdPartyDirectory;
	std::filesystem::path attributionDirectory = gpFileManager->GetAttributionDirectory();
	std::vector<PendingCopy> pendingCopies = BuildPendingCopies(thirdPartyDirectory, attributionDirectory);

	if (pendingCopies.empty())
	{
		return;
	}
	if (gpFileManager->EnsureLocal(FileManager::OutputRoot::kAttribution) == FileManager::EnsureLocalResult::kCancelled)
	{
		throw diagnostic::AlreadyReportedError("Attribution materialization cancelled");
	}
	PublishPendingCopies(pendingCopies);
}

}
