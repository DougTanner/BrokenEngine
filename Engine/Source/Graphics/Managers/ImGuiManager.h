#pragma once

#if defined(BT_CLIENT)

namespace engine
{

enum class UiTheme : uint8_t;

enum TextAreas
{
	kTextDebug,
	kTextGraphics,
	kTextProfileFps,
	kTextProfileCpuTimers,
	kTextProfileGpuTimers,
	kTextProfileCpuCounters,
	kTextProfileMemory,
	kTextAreasCount,
};

struct TextArea
{
	static constexpr int64_t kiMaxCharacters = 4'096;

	float fX = 0.0f;
	float fY = 0.0f;
	float fSize = 0.1f;
	int64_t iCharacterCount = 0;
	char pcText[kiMaxCharacters] {};
};

class ImGuiManager : public common::Singleton<ImGuiManager>
{
public:

	ImGuiManager(HWND hwnd);
	~ImGuiManager();

	// Idempotent: a deferred swapchain retry or device-loss teardown repeats it on already-destroyed state.
	void DestroyScreenDependentResources();
	void CreateScreenDependentResources();

	void Prepare(int64_t iFramebuffer);
	void Submit(int64_t iFramebuffer);
	void RegisterOpaqueRectangle(const ImVec2& rPosition, const ImVec2& rSize);
	void UpdateTextArea(TextAreas eTextArea, std::string_view characters);
	std::string_view GetTextArea(TextAreas eTextArea) const;
	void RenderTextAreas();

	static constexpr int64_t kiMaxUiRectangles = 32;

	float mfUiScale = 1.0f;

	VkBuffer mUiPrepassIndirectVkBuffer = VK_NULL_HANDLE;

private:

	struct TextureResource
	{
		~TextureResource();

		Texture texture;
		VkDescriptorSet vkDescriptorSet = VK_NULL_HANDLE;
	};

	struct TextureRetirement
	{
		std::vector<std::unique_ptr<TextureResource>> resources;
		std::vector<std::unique_ptr<StagingBuffer>> stagingBuffers;
	};

	void CreateRenderPass();
	void CreateFramebuffers();
	void CreateUiPrepassIndirectBuffer();
	void UpdateUiRectangleBuffers(int64_t iFramebuffer);
	void SetupThemeGeometry(float fUiScale);
	void ApplyThemeColors(UiTheme eTheme);
	void ProcessTextures(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer, const ImVector<ImTextureData*>* pTextures);

	std::unordered_map<ImTextureData*, std::unique_ptr<TextureResource>> mTextureResources;
	std::vector<TextureRetirement> mTextureRetirements;

	VkRenderPass mImGuiVkRenderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> mImGuiFramebuffers;
	ImDrawData* mpDrawData = nullptr;

	VmaAllocation mUiPrepassIndirectVmaAllocation = VK_NULL_HANDLE;
	VkDrawIndirectCommand* mpUiPrepassIndirectMappedVkDrawIndirectCommand = nullptr;

	int64_t miOpaqueRectangleCount = 0;
	XMFLOAT4 mpf4OpaqueRectangles[kiMaxUiRectangles] {};

	static constexpr float kfTextEdge = 0.025f;
	TextArea mpTextAreas[kTextAreasCount]
	{
		{ .fX = 0.45f, .fY = 1.0f - 2.0f * kfTextEdge },
		{ .fX = 0.875f, .fY = kfTextEdge },
		{ .fX = 0.5f * kfTextEdge, .fY = kfTextEdge },
		{ .fX = 0.5f * kfTextEdge, .fY = kfTextEdge },
		{ .fX = 0.5f * kfTextEdge, .fY = kfTextEdge },
		{ .fX = 0.875f, .fY = 0.15f },
		{ .fX = 0.725f, .fY = 0.15f },
	};
	static_assert(kTextAreasCount == 7);
};

inline ImGuiManager* gpImGuiManager = nullptr;

// Defined by the host program. ImGuiManager calls CreateUiScreens at the end of its constructor, RenderUiScreens between
// ImGui::NewFrame and ImGui::Render, and DestroyUiScreens first in its destructor.
void CreateUiScreens();
void RenderUiScreens();
void DestroyUiScreens();

inline float UiScale()
{
	return gpImGuiManager != nullptr ? gpImGuiManager->mfUiScale : 1.0f;
}

} // namespace engine

#endif // defined(BT_CLIENT)
