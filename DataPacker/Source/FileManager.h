#pragma once

#include "InputFingerprint.h"

class FileManager : public common::Singleton<FileManager>
{
public:
	enum class OutputRoot
	{
		kData,
		kAttribution,
	};

	enum class EnsureLocalResult
	{
		kAlreadyLocal,
		kMaterialized,
		kCancelled,
		kFailed,
	};

	enum class InitializationMode
	{
		kFull,
		kDataOnly,
	};

	FileManager(std::span<char*> argvSpan, EnsureLocalResult& reInitializationResult, InitializationMode eMode = InitializationMode::kFull);

	std::filesystem::path mpInputDirectories[2];
	std::filesystem::path mCacheDirectory;
	std::filesystem::path mGaeaCacheDirectory;
	std::filesystem::path mOutputDirectory;
	std::filesystem::path mThirdPartyDirectory;
	std::string mProjectName;
	std::unique_ptr<InputFingerprintCache> mpInputFingerprintCache;

	bool mbForbidExpensiveExport = false;
	bool mbForbidGaeaExport = false;

	EnsureLocalResult EnsureLocal(OutputRoot eRoot);

private:
	enum class OutputRootState
	{
		kLocal,
		kRecognizedPrimaryLink,
		kAbsent,
		kUnvalidatedReparse,
	};

	struct OutputRootInfo
	{
		std::filesystem::path source;
		std::filesystem::path destination;
		OutputRootState eState = OutputRootState::kLocal;
	};

	EnsureLocalResult InitializeWorktreeOutputs(InitializationMode eMode);
	EnsureLocalResult ReconcileWorktreeOutput(OutputRootInfo& rRoot);
	EnsureLocalResult MaterializeOutput(OutputRootInfo& rRoot);

	OutputRootInfo mDataOutput;
	OutputRootInfo mAttributionOutput;

};

void WriteEntireFile(const std::filesystem::path& rPath, std::string_view contents);

inline FileManager* gpFileManager = nullptr;
