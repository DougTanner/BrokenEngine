#pragma once

#if defined(BT_CLIENT)

#include "Buffer.h"

namespace engine
{

class Shader;
class Texture;

enum class DescriptorFlags : uint64_t
{
	kEmpty                          = 0x0001,

	kTextures                       = 0x0002,
	kCombinedSamplers               = 0x0008,
		kSamplerClamp               = 0x0010,
		kSamplerBorder              = 0x0020,
		kSamplerRepeat              = 0x0040,
		kSamplerMirroredRepeat      = 0x0080,
		kSamplerSmoke               = 0x0100,
		kSamplerBorderWhite         = 0x0200, // CLAMP_TO_BORDER + opaque white (1.0) — shadow's "no shadow" beyond coverage
		kSamplerWindClamp           = 0x10000,
		kSamplerElevation           = 0x20000,
		kSamplerMirroredRepeatWater  = 0x80000, // MirroredRepeat with the gWaterNormalMipmapBias slider instead of the global -gMipmapLevelOfDetailBias sharpen (water normal maps)
		kSamplerClampLinear          = 0x400000,
		kSamplerRepeatLinear         = 0x800000,
		kSamplerMirroredRepeatLinear = 0x1000000,
		// PipelineDescriptorWriter::Write uses this any-bit mask for standalone samplers and excludes kCombinedSamplers.
		// kSamplerBorderWhite is used only with kCombinedSamplers.
		kSamplerAny                 = 0x00010 | 0x00020 | 0x00040 | 0x00080 | 0x00100 | 0x00200 | 0x10000 | 0x20000 | 0x80000 | 0x400000 | 0x800000 | 0x1000000,
	kStorageImages                  = 0x0400,

	// One plain buffer shared by every frame in flight — correct only for data the CPU does not rewrite per frame.
	// Per-frame data must use one of the framebuffer-indexed routings below instead, or the pipeline reads a buffer
	// the CPU is concurrently writing for another frame: stale or torn values, with no validation error.
	kUniformBuffer                  = 0x0800,
	kStorageBuffer                  = 0x1000,
	// Caller-supplied array of per-framebuffer buffers; the writer picks the entry matching the frame being recorded.
	kPerCommandBufferUniformBuffers = 0x2000,
	kPerCommandBufferStorageBuffers = 0x4000,
	kGlobalLayoutUniformBuffers     = 0x100000, // The renderer's own per-framebuffer global layout buffers
	kMainLayoutUniformBuffers       = 0x200000, // The renderer's own per-framebuffer main layout buffers

	kModel                          = 0x8000,

	// Marks a bindless texture-array descriptor whose per-slot binding key is supplied lazily by the
	// data subsystem (e.g., IslandTerrainResidency) rather than derivable from ppTextures[k]->mInfo.uiCrc.
	// PipelineDescriptorWriter routes flagged entries into TextureDescriptors::mBindlessArrayConsumers;
	// At first-mint, IslandTerrainResidency::AcquireTextureSlot calls TextureDescriptors::MintIslandSlot, whose
	// RegisterIslandSlotBindings looks up that registry and registers under the correct islandCrc / chunk-CRC key.
	kBindlessArrayConsumer          = 0x40000,
};
using DescriptorFlags_t = common::Flags<DescriptorFlags>;

struct DescriptorInfo
{
	DescriptorFlags_t flags {DescriptorFlags::kEmpty};

	int64_t iCount = 1;
	int64_t iExplicitBinding = -1; // If >= 0, use this binding number instead of sequential assignment
	common::crc_t textureCrc = 0;
	Texture* pTexture = nullptr;
	Texture** ppTextures = nullptr;
	Buffer* pBuffers = nullptr;
	VkBuffer* pVkBuffers = nullptr;
	common::crc_t crc = 0;
};

enum class PipelineFlags : uint64_t
{
	kIndirectHostVisible       = 0x0001,
	kIndirectDeviceLocal       = 0x0002,
	kCompute                   = 0x0004,
	kAlphaBlend                = 0x0008,
	kAdd                       = 0x0010,
	kCullFront                 = 0x0020,
	kCullBack                  = 0x0040,
	kDepthTest                 = 0x0080,
	kDepthWrite                = 0x0100,
	kSampleShading             = 0x0200,
	kPushConstants             = 0x0400,
	kNoWireframe               = 0x0800,
	kRenderTarget              = 0x1000,
	kDepthBias                 = 0x2000,
	kMax                       = 0x4000,
	kUpdateAfterBind           = 0x8000,
	kAddAlpha                  = 0x10000,
	kMultiSet                  = 0x20000,
	kLineList                  = 0x40000,
	kNoColorWrite              = 0x80000,
};
using PipelineFlags_t = common::Flags<PipelineFlags>;

struct PipelineInfo
{
	std::string_view name;
	PipelineFlags_t flags;
	Shader* ppShaders[2] {};
	Buffer* pVertexBuffer = nullptr;
	// External Set 0 layout (global descriptor set from TextureManager, not owned)
	VkDescriptorSetLayout vkExternalDescriptorSetLayout = VK_NULL_HANDLE;
	// Multi-set: external Set 1 layout provided by first ModelPipeline material (not owned)
	VkDescriptorSetLayout vkExternalSet1DescriptorSetLayout = VK_NULL_HANDLE;

	// Render target
	VkRenderPass vkTargetRenderPass = VK_NULL_HANDLE;
	VkExtent3D vkExtent3D {};
	int32_t iColorAttachmentCount = 1;

	// Push-constant range size in bytes; 0 => default sizeof(shaders::PushConstantsLayout). Override only
	// for pipelines whose shader declares a smaller push-constant block (e.g. LightCombine).
	int64_t iPushConstantBytes = 0;

	// Entry count is capped at common::ShaderHeader::kiMaxDescriptorSetLayoutBindings by Pipeline::Create.
	std::vector<DescriptorInfo> descriptorInfos;
};

class Pipeline
{
public:

	// Fallback for out-of-bounds binding lookups when vertex/fragment shaders have different binding counts
	static constexpr VkDescriptorSetLayoutBinding kEmptyVkDescriptorSetLayoutBinding {};

	// Resolve which descriptor set a binding belongs to from shader reflection (puiDescriptorSetIndices),
	// defaulting to set 0 when neither shader declares it. Single source for the four set-partition /
	// set-routing sites in PipelineCreator / PipelineDescriptorWriter.
	static uint32_t ResolveBindingSetIndex(const PipelineInfo& rPipelineInfo, uint32_t uiBinding);

	Pipeline() = default;
	Pipeline(const Pipeline&) = delete;
	Pipeline(const PipelineInfo& rInfo);
	~Pipeline();

	void Create(const PipelineInfo& rInfo);
	void Destroy() noexcept;

	void RecordDraw(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, int64_t iInstanceCount, int64_t iFirstInstance, const XMFLOAT4& rf4PushConstants = {});
	void RecordDrawIndirect(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants = {});

	// Bind pipeline + descriptor sets (no vertex buffer, no draw call). Used by per-instance draw
	// loops (e.g., per-island terrain meshes) that bind their own vertex/index buffer and issue
	// vkCmdDrawIndexed themselves.
	void RecordBindPipelineAndDescriptors(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants = {});
	void RecordDrawIndirectSet2(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants);
	void RecordCompute(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, int64_t iGroupCountX, int64_t iGroupCountY = 1, int64_t iGroupCountZ = 1, const XMFLOAT4& rf4PushConstants = {});
	void RecordComputeIndirect(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants = {});
	void RecordComputeIndirectFrom(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, VkBuffer vkIndirectBuffer, VkDeviceSize vkIndirectOffset);

	void WriteIndirectBuffer(int64_t iCommandBuffer, int64_t iInstanceCount, int64_t iIndexCount = -1, int64_t iFirstIndex = 0, int64_t iVertexOffset = 0);
	void WriteIndirectComputeBuffer(int64_t iCommandBuffer, int64_t iGroupCountX, int64_t iGroupCountY, int64_t iGroupCountZ);


	PipelineInfo mInfo;

	bool mbPerCommandBuffer = false;

	VkDescriptorSetLayout mVkDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> mVkDescriptorSets;
	VkPipelineLayout mVkPipelineLayout = VK_NULL_HANDLE;
	VkPipeline mVkPipeline = VK_NULL_HANDLE;

	// Multi-set: Set 2 layout and descriptor sets (per-material bindings)
	VkDescriptorSetLayout mSet2VkDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> mVkDescriptorSetsSet2;
	// External Set 0 layout (global descriptor set from TextureManager, not owned)
	VkDescriptorSetLayout mExternalVkDescriptorSetLayout = VK_NULL_HANDLE;
	// Multi-set: external Set 1 layout provided by first ModelPipeline material (not owned)
	VkDescriptorSetLayout mExternalSet1VkDescriptorSetLayout = VK_NULL_HANDLE;

	// Indirect draw/dispatch buffer; host-visible commands are CPU-written, device-local commands are GPU-written.
	VkBuffer mIndirectVkBuffer = VK_NULL_HANDLE;
	VmaAllocation mIndirectVmaAllocation = VK_NULL_HANDLE;
	VkDrawIndexedIndirectCommand* mpIndirectVkDrawIndexedIndirectCommand = nullptr;
	VkDispatchIndirectCommand* mpIndirectComputeVkDispatchIndirectCommand = nullptr; // Host-visible dispatch map (kIndirectHostVisible | kCompute); mutually exclusive with the draw map above
	int64_t miIndirectSlotCount = 0; // Indirect buffer slot capacity bounding the Record*Indirect command-buffer index. Set only for indirect pipelines: the host-visible / graphics-device-local branches stamp max(framebufferCount, 3), the kIndirectDeviceLocal compute branch stamps 1. Stays 0 on non-indirect pipelines, which never read it (every Record*Indirect ASSERT is gated behind an indirect-flag ASSERT)

	Buffer mModelMaterialsStorageBuffer;

	// Texture CRCs collected during descriptor set creation for demand-driven loading
	std::vector<common::crc_t> mTextureCrcs;
	bool mbTexturesRequested = false;
};

// Global set 0 uses the framebuffer index iCommandBuffer; per-pipeline set 1 uses iDescriptorSetIndex according to mbPerCommandBuffer.
void BindComputeDescriptorSets(VkCommandBuffer vkCommandBuffer, VkPipelineLayout vkPipelineLayout, VkDescriptorSetLayout vkExternalLayout, int64_t iCommandBuffer, int64_t iDescriptorSetIndex, const std::vector<VkDescriptorSet>& rDescriptorSets);

} // namespace engine

#endif // defined(BT_CLIENT)
