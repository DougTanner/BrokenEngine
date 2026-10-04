#pragma once

#if defined(BT_CLIENT)

namespace game
{
class HudScreen;
class TweaksScreen;
} // namespace game

namespace engine
{

class GameSettingsScreen;
class GraphicsMenuScreen;
class MainMenuScreen;
class ModalScreen;
class PauseMenuScreen;
class SoundMenuScreen;

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

class ImGuiManager
{
public:

	ImGuiManager(HWND hwnd);
	~ImGuiManager();

	void Prepare(int64_t iFramebuffer);
	void Submit(int64_t iFramebuffer);
	void RegisterOpaqueRectangle(const ImVec2& rPosition, const ImVec2& rSize);
	void UpdateTextArea(TextAreas eTextArea, std::string_view characters);

	static constexpr int64_t kiMaxUiRectangles = 32;

	float mfUiScale = 1.0f;

	std::unique_ptr<game::TweaksScreen> mpTweaksScreen;

	VkBuffer mUiPrepassIndirectVkBuffer = VK_NULL_HANDLE;

private:

	void CreateRenderPass();
	void CreateFramebuffers();
	void CreateUiPrepassIndirectBuffer();
	void UpdateUiRectangleBuffers(int64_t iFramebuffer);
	void SetupThemeGeometry(float fUiScale);
	void ApplyThemeColors(UiTheme eTheme);
	void RenderTextAreas();

	VkRenderPass mImGuiVkRenderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> mImGuiFramebuffers;
	ImDrawData* mpDrawData = nullptr;

public:

	std::unique_ptr<MainMenuScreen> mpMainMenuScreen;
	std::unique_ptr<ModalScreen> mpModalScreen;
	std::unique_ptr<PauseMenuScreen> mpPauseMenuScreen;
	std::unique_ptr<GraphicsMenuScreen> mpGraphicsMenuScreen;
	std::unique_ptr<SoundMenuScreen> mpSoundMenuScreen;
	std::unique_ptr<GameSettingsScreen> mpGameSettingsScreen;
	std::unique_ptr<game::HudScreen> mpHudScreen;

private:

	VmaAllocation mUiPrepassIndirectVmaAllocation = VK_NULL_HANDLE;
	VkDrawIndirectCommand* mpUiPrepassIndirectMappedVkDrawIndirectCommand = nullptr;

	int64_t miOpaqueRectangleCount = 0;
	XMFLOAT4 mf4OpaqueRectangles[kiMaxUiRectangles] {};

	static constexpr float kfTextEdge = 0.025f;
	TextArea mTextAreas[kTextAreasCount]
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

inline constexpr float kfUiReferenceHeight = 2'160.0f;

// Resolution scale relative to the 2160-high reference monitor; multiply raw pixel constants by this
inline float UiScale()
{
	return gpImGuiManager != nullptr ? gpImGuiManager->mfUiScale : 1.0f;
}

} // namespace engine

#endif // defined(BT_CLIENT)
