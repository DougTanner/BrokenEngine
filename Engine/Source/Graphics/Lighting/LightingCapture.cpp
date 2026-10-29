#if defined(BT_CLIENT)

#include "Graphics/Managers/TextureCache.h"
#include "Graphics/Screenshot.h"
#include "LightingState.h"

namespace engine
{

static void LightingWriteFile(const std::filesystem::path& rPath, std::span<const std::byte> bytes)
{
	std::ofstream stream(rPath, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	stream.close();
	if (!stream)
	{
		throw std::runtime_error("Lighting capture file write failed");
	}
}

void Lighting::RecordCapture(VkCommandBuffer vkCommandBuffer)
{
	LightingDiagnostics& rDiagnostics = diagnostics;
	if (rDiagnostics.status != "receivers" && rDiagnostics.status != "tracing")
	{
		return;
	}
	State::Comparison& rComparison = *pState->comparison;
	if (rDiagnostics.status == "receivers")
	{
		rComparison.reference.TransitionImageLayout(vkCommandBuffer, rComparison.eReferenceLayout, TextureLayout::kComputeReadWrite);
		rComparison.eReferenceLayout = TextureLayout::kComputeReadWrite;
		RecordReceivers(vkCommandBuffer);
		rDiagnostics.status = "tracing";
		return;
	}
	int64_t iEnd = std::min(rDiagnostics.iCompletedTiles + State::Comparison::kiTilesPerFrame, rDiagnostics.iTotalTiles);
	for (int64_t i = rDiagnostics.iCompletedTiles; i < iEnd; ++i)
	{
		rComparison.compute.RecordCompute(pState->iFramebuffer, vkCommandBuffer, 1, 1, 1, {static_cast<float>((i % State::Comparison::kiTileColumns) * 4), static_cast<float>((i / State::Comparison::kiTileColumns) * 4), static_cast<float>(pState->settings.iAreaSamples), 0.0f});
	}
	rDiagnostics.iCompletedTiles = iEnd;
	if (iEnd < rDiagnostics.iTotalTiles)
	{
		return;
	}
	// Every earlier frame's tiles precede this barrier on the graphics queue, so Main composes the whole reference.
	rComparison.reference.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
	rComparison.eReferenceLayout = TextureLayout::kShaderReadOnly;
	gpTextureManager->mTextureDescriptors.SetLightingReference(pState->iFramebuffer, &rComparison.reference, true);
	rDiagnostics.status = "composing";
}

void Lighting::FinishCapture(int64_t iFramebuffer)
{
	LightingDiagnostics& rDiagnostics = diagnostics;
	if (rDiagnostics.status != "composing")
	{
		return;
	}
	// Heap: the developer capture's readbacks, files and status text are written once per capture.
	ScopedSuppressAllocationTracking suppress;
	// The composing frame's UI submit signals this fence; the reference descriptor changes only after Main has read it.
	VkFence vkFence = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebuffer).mVkFence;
	CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &vkFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));
	const State::Comparison& rComparison = *pState->comparison;
	const LightingComparisonSettings& rSettings = pState->settings;
	try
	{
		rDiagnostics.directory = gpFileManager->mTempDirectory / "Lighting" / std::format("capture-{}-{}", pState->uiServicedCapture, pState->captureStart.time_since_epoch().count());
		std::filesystem::create_directories(rDiagnostics.directory);
		SaveFinishedPng(iFramebuffer, rDiagnostics.directory / "finished.png");
		static constexpr std::array<std::string_view, 5> kNames {"position.rgba32f", "shading-normal.rgba32f", "geometric-normal.rgba32f", "albedo.rgba32f", "direct.rgba32f"};
		for (int64_t i = 0; i < std::ssize(kNames); ++i)
		{
			const Texture& rTexture = i < std::ssize(rComparison.receivers) ? rComparison.receivers[i] : rComparison.reference;
			std::vector<std::byte> bytes;
			TextureCache::CopyImageToHostMemory(rTexture.mVkImage, State::Comparison::kVkExtent, VK_FORMAT_R32G32B32A32_SFLOAT, 1, 1, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, bytes);
			LightingWriteFile(rDiagnostics.directory / kNames[i], bytes);
		}
		nlohmann::json capture
		{
			{"request", pState->uiServicedCapture},
			{"sourceCount", pState->capturePublication.uiSourceCount},
			{"samples", rSettings.iAreaSamples},
			{"event", rSettings.iEvent},
			{"camera", static_cast<int>(rSettings.eCamera)},
			{"scenario", static_cast<int>(rSettings.eScenario)},
			{"width", State::Comparison::kVkExtent.width},
			{"height", State::Comparison::kVkExtent.height},
			{"wallMs", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pState->captureStart).count()},
			{"device", std::string_view(gpInstanceManager->mVkPhysicalDeviceProperties.deviceName)},
		};
		std::string text = capture.dump(2);
		LightingWriteFile(rDiagnostics.directory / "capture.json", std::as_bytes(std::span(text)));
		rDiagnostics.status = "complete";
	}
	catch (const std::system_error& rException)
	{
		if (rException.code() == VkErrorCode(VK_ERROR_DEVICE_LOST))
		{
			throw;
		}
		rDiagnostics.status = "failed";
		rDiagnostics.error = rException.what();
	}
	catch (const std::exception& rException)
	{
		rDiagnostics.status = "failed";
		rDiagnostics.error = rException.what();
	}
	gpTextureManager->mTextureDescriptors.SetLightingReference(iFramebuffer, nullptr, false);
}

void Lighting::WriteProfile(nlohmann::json& rResult) const
{
	const LightingDiagnostics& rDiagnostics = diagnostics;
	rResult = {{"sources", rDiagnostics.iSources}, {"instances", rDiagnostics.iInstances}, {"traceOnlyInstances", rDiagnostics.iTraceOnlyInstances}, {"triangles", rDiagnostics.iTriangles}};
	rResult["capture"] = {{"status", rDiagnostics.status}, {"completedTiles", rDiagnostics.iCompletedTiles}, {"totalTiles", rDiagnostics.iTotalTiles}, {"error", rDiagnostics.error}, {"directory", common::ToString(rDiagnostics.directory.native())}};
}

} // namespace engine

#endif // defined(BT_CLIENT)
