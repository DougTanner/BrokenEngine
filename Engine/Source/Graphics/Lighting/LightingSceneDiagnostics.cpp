#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "LightingState.h"

namespace engine
{

void Lighting::CreateSceneDiagnosticResources()
{
	for (const std::unique_ptr<LightingFramebuffer>& rpFramebuffer : mpState->framebuffers)
	{
		LightingFramebuffer& rFramebuffer = *rpFramebuffer;
		rFramebuffer.diagnosticParameters.Create({.name = "LightingSceneDiagnosticParameters", .flags = {BufferFlags::kStorage, BufferFlags::kHostVisible}, .iDataSize = sizeof(shaders::LightingSceneDiagnosticParameters), .eMemoryPlacement = BufferMemoryPlacement::kOrdinaryRam});
		rFramebuffer.diagnosticResult.mInfo = {.name = "LightingSceneDiagnosticResult", .flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal}, .iDataSize = sizeof(shaders::LightingSceneDiagnosticResult), .eMemoryPlacement = BufferMemoryPlacement::kOrdinaryRam};
		Buffer::CreateBuffer("LightingSceneDiagnosticResult", sizeof(shaders::LightingSceneDiagnosticResult), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rFramebuffer.diagnosticResult.mDeviceLocalVkBuffer, rFramebuffer.diagnosticResult.mDeviceLocalVmaAllocation, nullptr, rFramebuffer.diagnosticResult.mInfo.eMemoryPlacement);
		rFramebuffer.diagnosticReadback = std::make_unique<StagingBuffer>("LightingSceneDiagnosticReadback", sizeof(shaders::LightingSceneDiagnosticResult), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	}
}

void Lighting::CreateSceneDiagnosticPipelines()
{
	DestroySceneDiagnosticPipelines();
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.front();
	PipelineInfo info {.name = "LightingSceneDiagnostics", .flags = {PipelineFlags::kCompute, PipelineFlags::kUpdateAfterBind}, .ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersLightingLightingSceneDiagnosticscompCrc)}};
	AddQueryDescriptorInfos(info, 0);
	info.descriptorInfos.push_back({.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = 27, .pBuffers = &rFramebuffer.diagnosticResult});
	info.descriptorInfos.push_back({.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = 28, .pBuffers = &rFramebuffer.diagnosticParameters});
	mpState->diagnosticPipeline.Create(info);
}

void Lighting::DestroySceneDiagnosticPipelines()
{
	mpState->diagnosticPipeline.Destroy();
}

void Lighting::RecordSceneDiagnostics(VkCommandBuffer vkCommandBuffer)
{
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(mpState->iFramebuffer);
	rFramebuffer.diagnosticOperations = {};
	rFramebuffer.uiDiagnosticPublication = 0;
	if (mpState->settings.eMode == LightingComparisonMode::kDisabled)
	{
		return;
	}
	if (mpState->uiValidationRequest == 0 && mpState->settings.uiCaptureRequest == mpState->uiServicedCapture)
	{
		return;
	}
	*reinterpret_cast<shaders::LightingSceneDiagnosticParameters*>(rFramebuffer.diagnosticParameters.mpMappedMemory) = {.uiPublication = rFramebuffer.pendingDiagnostics.uiPublication, .uiGeneration = rFramebuffer.uiGeneration, .uiSampleCount = static_cast<uint32_t>(mpState->settings.iAreaSamples)};
	BindQueryDescriptors(mpState->diagnosticPipeline, mpState->iFramebuffer);
	PipelineDescriptorWriter::UpdateStorageBuffer(mpState->diagnosticPipeline, mpState->iFramebuffer, 27, &rFramebuffer.diagnosticResult);
	PipelineDescriptorWriter::UpdateStorageBuffer(mpState->diagnosticPipeline, mpState->iFramebuffer, 28, &rFramebuffer.diagnosticParameters);
	// TEMP START descriptor owner evidence
	static uint32_t suiOwnerEvidence = 0;
	if (suiOwnerEvidence < 24)
	{
		++suiOwnerEvidence;
		LOG(kTemp, kInfo, "DESCRIPTOR_OWNER diagnostic framebuffer={} publication={} set={} result27={} parameters28={}", mpState->iFramebuffer, rFramebuffer.pendingDiagnostics.uiPublication, reinterpret_cast<uint64_t>(mpState->diagnosticPipeline.mVkDescriptorSets.at(mpState->iFramebuffer)), reinterpret_cast<uint64_t>(rFramebuffer.diagnosticResult.GetBuffer()), reinterpret_cast<uint64_t>(rFramebuffer.diagnosticParameters.GetBuffer()));
	}
	// TEMP END descriptor owner evidence
	vkCmdFillBuffer(vkCommandBuffer, rFramebuffer.diagnosticResult.GetBuffer(), 0, VK_WHOLE_SIZE, 0);
	++rFramebuffer.diagnosticOperations.iFills;
	VkMemoryBarrier barrier {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	++rFramebuffer.diagnosticOperations.iBarriers;
	int64_t iCount = std::max<int64_t>({8, rFramebuffer.pendingDiagnostics.iSources, mpState->settings.iAreaSamples});
	mpState->diagnosticPipeline.RecordCompute(mpState->iFramebuffer, vkCommandBuffer, (iCount + 63) / 64);
	++rFramebuffer.diagnosticOperations.iDispatches;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	++rFramebuffer.diagnosticOperations.iBarriers;
	VkBufferCopy copy {.size = sizeof(shaders::LightingSceneDiagnosticResult)};
	vkCmdCopyBuffer(vkCommandBuffer, rFramebuffer.diagnosticResult.GetBuffer(), rFramebuffer.diagnosticReadback->vkStagingBuffer, 1, &copy);
	++rFramebuffer.diagnosticOperations.iCopies;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	++rFramebuffer.diagnosticOperations.iBarriers;
	rFramebuffer.uiDiagnosticPublication = rFramebuffer.pendingDiagnostics.uiPublication;
}

void Lighting::CompleteSceneDiagnostics(int64_t iFramebuffer)
{
	const LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	if (rFramebuffer.uiDiagnosticPublication == 0 || rFramebuffer.uiDiagnosticPublication != rFramebuffer.pendingDiagnostics.uiPublication || rFramebuffer.uiRecordedPublication != rFramebuffer.uiDiagnosticPublication)
	{
		return;
	}
	std::ignore = std::memcpy(&mpState->completedSceneDiagnostics, rFramebuffer.diagnosticReadback->vmaAllocationInfo.pMappedData, sizeof(mpState->completedSceneDiagnostics));
	const shaders::LightingSceneDiagnosticResult& rResult = mpState->completedSceneDiagnostics;
	if (rResult.uiPublication != rFramebuffer.uiDiagnosticPublication || rResult.uiGeneration != rFramebuffer.uiGeneration)
	{
		throw std::runtime_error("Lighting scene diagnostic generation mismatch");
	}
	mpState->completedDiagnostics.uiSceneDiagnosticPublication = rResult.uiPublication;
	mpState->completedDiagnostics.iBadSources = rResult.uiBadSources;
	mpState->completedDiagnostics.iBadSamples = rResult.uiBadSamples;
}

} // namespace engine

#endif // defined(BT_CLIENT)
