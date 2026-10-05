#if defined(BT_CLIENT)

#include "ImGuiManager.h"

#include "Data/Raw.h"
#include "File/PackChunks.h"
#include "Ui/Screens/GameSettingsScreen.h"
#include "Ui/Screens/GraphicsMenuScreen.h"
#include "Ui/Screens/MainMenuScreen.h"
#include "Ui/Screens/ModalScreen.h"
#include "Ui/Screens/PauseMenuScreen.h"
#include "Ui/Screens/SoundMenuScreen.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileManager.h"
#include "Ui/Screens/TweaksScreen/TweaksScreen.h"
#include "Ui/Screens/HudScreen.h"
#include "Game.h"

namespace engine
{

// Keep integer-truncated ImGui geometry fields at least one pixel; WindowBorderHoverPadding is 4px by default.
constexpr float kfMinimumStyleGeometryFactor = 0.25f;

// Base hues per UiTheme; ApplyThemeColors derives the full ImGuiStyle::Colors[] set from these
struct ThemePalette
{
	ImVec4 f4Text;
	ImVec4 f4TextDisabled;
	ImVec4 f4Background;
	ImVec4 f4BackgroundElevated;
	ImVec4 f4Accent;
	ImVec4 f4AccentHover;
	ImVec4 f4AccentActive;
	ImVec4 f4Border;
};

constexpr ThemePalette kThemePalettes[]
{
	// kNavalSteel: near-black blue-grey, steel borders, cyan/teal accent
	{
		.f4Text = ImVec4(0.86f, 0.91f, 0.94f, 1.0f),
		.f4TextDisabled = ImVec4(0.45f, 0.52f, 0.58f, 1.0f),
		.f4Background = ImVec4(0.07f, 0.09f, 0.11f, 1.0f),
		.f4BackgroundElevated = ImVec4(0.12f, 0.16f, 0.20f, 1.0f),
		.f4Accent = ImVec4(0.15f, 0.75f, 0.85f, 1.0f),
		.f4AccentHover = ImVec4(0.25f, 0.85f, 0.95f, 1.0f),
		.f4AccentActive = ImVec4(0.10f, 0.60f, 0.70f, 1.0f),
		.f4Border = ImVec4(0.25f, 0.33f, 0.40f, 1.0f),
	},
	// kDarkAmber: charcoal, warm amber accent
	{
		.f4Text = ImVec4(0.92f, 0.89f, 0.84f, 1.0f),
		.f4TextDisabled = ImVec4(0.55f, 0.51f, 0.45f, 1.0f),
		.f4Background = ImVec4(0.09f, 0.09f, 0.09f, 1.0f),
		.f4BackgroundElevated = ImVec4(0.15f, 0.14f, 0.13f, 1.0f),
		.f4Accent = ImVec4(0.95f, 0.65f, 0.15f, 1.0f),
		.f4AccentHover = ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
		.f4AccentActive = ImVec4(0.80f, 0.52f, 0.10f, 1.0f),
		.f4Border = ImVec4(0.38f, 0.33f, 0.26f, 1.0f),
	},
	// kMidnightMauve: near-black indigo base, lavender/mauve accent (Catppuccin Mocha)
	{
		.f4Text = ImVec4(0.80f, 0.84f, 0.96f, 1.0f),
		.f4TextDisabled = ImVec4(0.50f, 0.52f, 0.61f, 1.0f),
		.f4Background = ImVec4(0.12f, 0.12f, 0.18f, 1.0f),
		.f4BackgroundElevated = ImVec4(0.19f, 0.20f, 0.27f, 1.0f),
		.f4Accent = ImVec4(0.80f, 0.65f, 0.97f, 1.0f),
		.f4AccentHover = ImVec4(0.85f, 0.73f, 1.0f, 1.0f),
		.f4AccentActive = ImVec4(0.68f, 0.52f, 0.86f, 1.0f),
		.f4Border = ImVec4(0.27f, 0.28f, 0.35f, 1.0f),
	},
};
static_assert(std::size(kThemePalettes) == static_cast<size_t>(UiTheme::kCount));

static ImVec4 WithAlpha(const ImVec4& rf4Color, float fAlpha)
{
	return ImVec4(rf4Color.x, rf4Color.y, rf4Color.z, fAlpha);
}

ImGuiManager::ImGuiManager(HWND hwnd)
{
	ASSERT(gpImGuiManager == nullptr);

	gpImGuiManager = this;

	CreateRenderPass();
	CreateFramebuffers();
	CreateUiPrepassIndirectBuffer();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	// Enable the vendored imgui's test-engine ItemAdd/ItemInfo hooks so engine::AgentUiRegistry captures widget
	// rects/labels each frame. Only worth the per-item hook cost when the agent layer is live; defaults false otherwise.
	if (gpAgentCommandServer != nullptr)
	{
		ImGui::GetCurrentContext()->TestEngineHookItems = true;
	}
	ImGuiIO& rInputOutput = ImGui::GetIO();
	rInputOutput.IniFilename = nullptr;
	rInputOutput.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	rInputOutput.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// Load one default font for Latin and CJK text, with oversampling for crisp rendering
	const EagerChunk& rFontChunk = gpFileManager->mpPackChunks->GetEagerChunkMap().at(data::kRawNotoSansSCLightotfCrc);
	ImFontConfig fontConfiguration;
	fontConfiguration.OversampleH = 2;
	fontConfiguration.OversampleV = 1;
	fontConfiguration.FontDataOwnedByAtlas = false;
	// Trust boundary: on-disk ChunkHeader::iSize drives the TTF byte length ImGui reads from pData; bound it to the eager chunk's true extent before the copy (reject non-positive too — a negative int64 passes the upper bound and reaches stb_truetype as a negative int).
	if (rFontChunk.pHeader->iSize <= 0 || rFontChunk.pHeader->iSize > rFontChunk.iDataSize)
	{
		throw std::ios_base::failure("ImGuiManager font");
	}
	ImGui::GetIO().Fonts->AddFontFromMemoryTTF(rFontChunk.pData, static_cast<int>(rFontChunk.pHeader->iSize), 26.0f, &fontConfiguration);

	bool bWin32Init = ImGui_ImplWin32_Init(hwnd);
	ASSERT(bWin32Init);

	ImGui_ImplVulkan_InitInfo initializationInfo
	{
		.Instance = gpInstanceManager->mVkInstance,
		.PhysicalDevice = gpInstanceManager->mVkPhysicalDevice,
		.Device = gpDeviceManager->mVkDevice,
		.QueueFamily = static_cast<uint32_t>(gpInstanceManager->miGraphicsQueueFamilyIndex),
		.Queue = gpDeviceManager->mGraphicsVkQueue,
		.DescriptorPool = gpDeviceManager->mVkDescriptorPool,
		.MinImageCount = static_cast<uint32_t>(std::ssize(gpSwapchainManager->mFramebuffers)),
		.ImageCount = static_cast<uint32_t>(std::ssize(gpSwapchainManager->mFramebuffers)),
		.PipelineInfoMain
		{
			.RenderPass = mImGuiVkRenderPass,
			.Subpass = 0,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		},
		.MinAllocationSize = 1'024 * 1'024,
	};
	// ImGui_ImplVulkan_Init reports failures through internal IM_ASSERTs and always returns true.
	ImGui_ImplVulkan_Init(&initializationInfo);

	// Minimized/zero-height: keep the 1.0f default scale rather than collapsing style to 0
	float fHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	if (fHeight > 0.0f)
	{
		mfUiScale = fHeight / kfUiReferenceHeight;
	}
	SetupThemeGeometry(mfUiScale);
	ImGui::GetStyle().FontScaleDpi = mfUiScale;
	ApplyThemeColors(GetUiTheme());

	// NewFrame creates the font atlas; upload its textures before EndFrame validates them.
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	for (ImTextureData* pTexture : ImGui::GetPlatformIO().Textures)
	{
		if (pTexture->Status != ImTextureStatus_OK)
		{
			ImGui_ImplVulkan_UpdateTexture(pTexture);
		}
	}

	ImGui::EndFrame();

	mpTweaksScreen = std::make_unique<game::TweaksScreen>();
	mpMainMenuScreen = std::make_unique<MainMenuScreen>();
	mpModalScreen = std::make_unique<ModalScreen>();
	mpPauseMenuScreen = std::make_unique<PauseMenuScreen>();
	mpGraphicsMenuScreen = std::make_unique<GraphicsMenuScreen>();
	mpSoundMenuScreen = std::make_unique<SoundMenuScreen>();
	mpGameSettingsScreen = std::make_unique<GameSettingsScreen>();
	mpHudScreen = std::make_unique<game::HudScreen>();
}

// Reset all geometry before scaling to prevent cumulative drift across resolution changes, including fields without explicit overrides.
void ImGuiManager::SetupThemeGeometry(float fUiScale)
{
	ImGuiStyle& rStyle = ImGui::GetStyle();

	// Preserve Colors, which ApplyThemeColors and opacity updates own. Default ImGuiStyle construction is heap-free for the allocation-tracked main loop. The reset restores FontScaleMain/FontScaleDpi to 1.0f; both callers restore FontScaleDpi, while only Prepare restores FontScaleMain from gUiFontScale. FontSizeBase resets to 0.0f and is derived from the default font's LegacySize on the next font update.
	ImGuiStyle defaultStyle;
	std::copy(std::begin(rStyle.Colors), std::end(rStyle.Colors), std::span(defaultStyle.Colors).begin());
	rStyle = defaultStyle;

	// WindowRounding stays small: RegisterOpaqueRectangle occlusion rects are rectangular, so with gOpaqueUi on, large rounding
	// would occlude the 3D scene behind the rounded-off corners (4.0f base -> 8px at 4K after the 2x scale below)
	rStyle.WindowRounding = 4.0f;
	rStyle.ChildRounding = 3.0f;
	rStyle.FrameRounding = 3.0f;
	rStyle.PopupRounding = 3.0f;
	rStyle.GrabRounding = 3.0f;
	rStyle.TabRounding = 3.0f;
	rStyle.WindowBorderSize = 1.0f;
	rStyle.FrameBorderSize = 1.0f;
	rStyle.WindowPadding = ImVec2(10.0f, 10.0f);
	rStyle.FramePadding = ImVec2(6.0f, 4.0f);
	rStyle.ItemSpacing = ImVec2(8.0f, 6.0f);
	rStyle.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	rStyle.ScrollbarSize = 14.0f;
	rStyle.GrabMinSize = 12.0f;

	// Scale UI element sizes to 2x at the 4K reference, times the resolution scale
	rStyle.ScaleAllSizes(std::max(kfMinimumStyleGeometryFactor, 2.0f * fUiScale));
}

void ImGuiManager::ApplyThemeColors(UiTheme eTheme)
{
	const ThemePalette& rPalette = kThemePalettes[static_cast<size_t>(eTheme)];
	ImVec4* pColors = ImGui::GetStyle().Colors;

	// Window backgrounds carry the user-controlled opacity; Prepare() rewrites only their .w on opacity changes
	float fAlpha = gOpaqueUi.Get<bool>() ? 1.0f : gUiOpacity.mfCurrent;
	pColors[ImGuiCol_WindowBg] = WithAlpha(rPalette.f4Background, fAlpha);
	pColors[ImGuiCol_ChildBg] = WithAlpha(rPalette.f4Background, fAlpha);
	pColors[ImGuiCol_PopupBg] = WithAlpha(rPalette.f4Background, fAlpha);

	pColors[ImGuiCol_Text] = rPalette.f4Text;
	pColors[ImGuiCol_TextDisabled] = rPalette.f4TextDisabled;
	pColors[ImGuiCol_Border] = WithAlpha(rPalette.f4Border, 0.6f);
	pColors[ImGuiCol_FrameBg] = WithAlpha(rPalette.f4BackgroundElevated, 0.8f);
	pColors[ImGuiCol_FrameBgHovered] = WithAlpha(rPalette.f4Accent, 0.25f);
	pColors[ImGuiCol_FrameBgActive] = WithAlpha(rPalette.f4Accent, 0.4f);
	pColors[ImGuiCol_TitleBg] = rPalette.f4Background;
	pColors[ImGuiCol_TitleBgActive] = rPalette.f4BackgroundElevated;
	pColors[ImGuiCol_TitleBgCollapsed] = WithAlpha(rPalette.f4Background, 0.6f);
	pColors[ImGuiCol_MenuBarBg] = rPalette.f4BackgroundElevated;
	pColors[ImGuiCol_ScrollbarBg] = WithAlpha(rPalette.f4Background, 0.6f);
	pColors[ImGuiCol_ScrollbarGrab] = rPalette.f4BackgroundElevated;
	pColors[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(rPalette.f4Accent, 0.6f);
	pColors[ImGuiCol_ScrollbarGrabActive] = rPalette.f4AccentActive;
	pColors[ImGuiCol_CheckMark] = rPalette.f4Accent;
	pColors[ImGuiCol_SliderGrab] = WithAlpha(rPalette.f4Accent, 0.8f);
	pColors[ImGuiCol_SliderGrabActive] = rPalette.f4AccentActive;
	pColors[ImGuiCol_Button] = WithAlpha(rPalette.f4BackgroundElevated, 0.9f);
	pColors[ImGuiCol_ButtonHovered] = WithAlpha(rPalette.f4AccentHover, 0.4f);
	pColors[ImGuiCol_ButtonActive] = WithAlpha(rPalette.f4AccentActive, 0.6f);
	pColors[ImGuiCol_Header] = WithAlpha(rPalette.f4Accent, 0.25f);
	pColors[ImGuiCol_HeaderHovered] = WithAlpha(rPalette.f4Accent, 0.35f);
	pColors[ImGuiCol_HeaderActive] = WithAlpha(rPalette.f4Accent, 0.45f);
	pColors[ImGuiCol_Separator] = WithAlpha(rPalette.f4Border, 0.6f);
	pColors[ImGuiCol_SeparatorHovered] = WithAlpha(rPalette.f4Accent, 0.6f);
	pColors[ImGuiCol_SeparatorActive] = rPalette.f4Accent;
	pColors[ImGuiCol_ResizeGrip] = WithAlpha(rPalette.f4Accent, 0.2f);
	pColors[ImGuiCol_ResizeGripHovered] = WithAlpha(rPalette.f4Accent, 0.5f);
	pColors[ImGuiCol_ResizeGripActive] = rPalette.f4Accent;
	pColors[ImGuiCol_Tab] = rPalette.f4Background;
	pColors[ImGuiCol_TabHovered] = WithAlpha(rPalette.f4Accent, 0.4f);
	pColors[ImGuiCol_TabSelected] = rPalette.f4BackgroundElevated;
	pColors[ImGuiCol_TabSelectedOverline] = rPalette.f4Accent;
	pColors[ImGuiCol_TabDimmed] = WithAlpha(rPalette.f4Background, 0.8f);
	pColors[ImGuiCol_TabDimmedSelected] = WithAlpha(rPalette.f4BackgroundElevated, 0.8f);
	pColors[ImGuiCol_PlotLines] = rPalette.f4Accent;
	pColors[ImGuiCol_PlotLinesHovered] = rPalette.f4AccentHover;
	pColors[ImGuiCol_PlotHistogram] = rPalette.f4Accent;
	pColors[ImGuiCol_PlotHistogramHovered] = rPalette.f4AccentHover;
	pColors[ImGuiCol_TextSelectedBg] = WithAlpha(rPalette.f4Accent, 0.35f);
	pColors[ImGuiCol_NavCursor] = rPalette.f4Accent;

	pColors[ImGuiCol_TextLink] = rPalette.f4Accent;
	pColors[ImGuiCol_InputTextCursor] = rPalette.f4Text;
	pColors[ImGuiCol_TreeLines] = WithAlpha(rPalette.f4Border, 0.6f);
	pColors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.3f);
	pColors[ImGuiCol_UnsavedMarker] = rPalette.f4Accent;
	pColors[ImGuiCol_DragDropTarget] = rPalette.f4Accent;
	pColors[ImGuiCol_DragDropTargetBg] = WithAlpha(rPalette.f4Accent, 0.25f);
	pColors[ImGuiCol_TableHeaderBg] = rPalette.f4BackgroundElevated;
	pColors[ImGuiCol_TableBorderStrong] = WithAlpha(rPalette.f4Border, 0.6f);
	pColors[ImGuiCol_TableBorderLight] = WithAlpha(rPalette.f4Border, 0.35f);
	pColors[ImGuiCol_TableRowBg] = WithAlpha(rPalette.f4Background, 0.0f);
	pColors[ImGuiCol_TableRowBgAlt] = WithAlpha(rPalette.f4BackgroundElevated, 0.35f);
	pColors[ImGuiCol_NavWindowingHighlight] = WithAlpha(rPalette.f4Accent, 0.7f);
	pColors[ImGuiCol_NavWindowingDimBg] = WithAlpha(rPalette.f4Background, 0.2f);
	pColors[ImGuiCol_ModalWindowDimBg] = WithAlpha(rPalette.f4Background, 0.35f);
	pColors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(rPalette.f4Accent, 0.5f);
}

ImGuiManager::~ImGuiManager()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	vmaDestroyBuffer(gpDeviceManager->mpAllocator, mUiPrepassIndirectVkBuffer, mUiPrepassIndirectVmaAllocation);

	for (VkFramebuffer vkFramebuffer : mImGuiFramebuffers)
	{
		vkDestroyFramebuffer(gpDeviceManager->mVkDevice, vkFramebuffer, nullptr);
	}

	vkDestroyRenderPass(gpDeviceManager->mVkDevice, mImGuiVkRenderPass, nullptr);

	if (gpImGuiManager == this)
	{
		gpImGuiManager = nullptr;
	}
}

void ImGuiManager::CreateUiPrepassIndirectBuffer()
{
	int64_t iFramebufferCount = std::ssize(gpSwapchainManager->mFramebuffers);
	VkBufferCreateInfo vkBufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = static_cast<VkDeviceSize>(iFramebufferCount * sizeof(VkDrawIndirectCommand)),
		.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	VmaAllocationCreateInfo vmaAllocationCreateInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	};
	VmaAllocationInfo vmaAllocationInfo {};
	CHECK_VK(vmaCreateBuffer(gpDeviceManager->mpAllocator, &vkBufferCreateInfo, &vmaAllocationCreateInfo, &mUiPrepassIndirectVkBuffer, &mUiPrepassIndirectVmaAllocation, &vmaAllocationInfo));
	VkName(VK_OBJECT_TYPE_BUFFER, mUiPrepassIndirectVkBuffer, "UiPrepassIndirect");
	mpUiPrepassIndirectMappedVkDrawIndirectCommand = static_cast<VkDrawIndirectCommand*>(vmaAllocationInfo.pMappedData);
	ASSERT(mpUiPrepassIndirectMappedVkDrawIndirectCommand != nullptr);
	for (int64_t i = 0; i < iFramebufferCount; ++i)
	{
		mpUiPrepassIndirectMappedVkDrawIndirectCommand[i] = {.vertexCount = 6, .instanceCount = 0, .firstVertex = 0, .firstInstance = 0};
	}
}

void ImGuiManager::CreateRenderPass()
{
	VkAttachmentDescription vkAttachmentDescription
	{
		.flags = 0,
		.format = gpInstanceManager->mFramebufferVkFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference vkAttachmentReference
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription vkSubpassDescription
	{
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &vkAttachmentReference,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	VkSubpassDependency vkSubpassDependency
	{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = 0,
	};

	VkRenderPassCreateInfo vkRenderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = 1,
		.pAttachments = &vkAttachmentDescription,
		.subpassCount = 1,
		.pSubpasses = &vkSubpassDescription,
		.dependencyCount = 1,
		.pDependencies = &vkSubpassDependency,
	};
	CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkRenderPassCreateInfo, nullptr, &mImGuiVkRenderPass));
	VkName(VK_OBJECT_TYPE_RENDER_PASS, mImGuiVkRenderPass, "ImGui");
}

void ImGuiManager::CreateFramebuffers()
{
	mImGuiFramebuffers.resize(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (int64_t i = 0; i < std::ssize(mImGuiFramebuffers); ++i)
	{
		VkImageView vkImageView = gpSwapchainManager->mFramebuffers.at(i).vkPresentImageView;
		VkFramebufferCreateInfo vkFramebufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = mImGuiVkRenderPass,
			.attachmentCount = 1,
			.pAttachments = &vkImageView,
			.width = gpGraphics->mFramebufferVkExtent2D.width,
			.height = gpGraphics->mFramebufferVkExtent2D.height,
			.layers = 1,
		};
		CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkFramebufferCreateInfo, nullptr, &mImGuiFramebuffers.at(i)));
		VkName(VK_OBJECT_TYPE_FRAMEBUFFER, mImGuiFramebuffers.at(i), std::format("ImGui_{}", i).c_str());
	}
}

void ImGuiManager::Prepare(int64_t iFramebuffer)
{
	// Changed() advances the single-consumer change tracking; apply re-reads via GetUiTheme() for the trust-boundary clamp
	auto [eUiTheme, ePreviousUiTheme, bUiThemeChanged] = gUiTheme.Changed<UiTheme>();
	if (bUiThemeChanged)
	{
		ApplyThemeColors(GetUiTheme());
	}

	auto [bOpaqueUi, bPreviousOpaqueUi, bOpaqueUiChanged] = gOpaqueUi.Changed<bool>();
	auto [fUiOpacity, fPreviousUiOpacity, bUiOpacityChanged] = gUiOpacity.Changed<float>();
	if (bOpaqueUiChanged || bUiOpacityChanged)
	{
		ImGuiStyle& rStyle = ImGui::GetStyle();
		float fAlpha = bOpaqueUi ? 1.0f : fUiOpacity;
		rStyle.Colors[ImGuiCol_WindowBg].w = fAlpha;
		rStyle.Colors[ImGuiCol_ChildBg].w = fAlpha;
		rStyle.Colors[ImGuiCol_PopupBg].w = fAlpha;
	}

	// Update geometry only when its scale changes; ImGui_ImplWin32_NewFrame refreshes DisplaySize later. Extent changes recreate this manager and its ImGui context at the kSwapchain destroy tier.
	float fPreviousUiScale = mfUiScale;
	// Minimized/zero-height: keep previous scale rather than collapsing style to 0
	float fHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	if (fHeight > 0.0f)
	{
		mfUiScale = fHeight / kfUiReferenceHeight;
	}
	if (mfUiScale != fPreviousUiScale)
	{
		SetupThemeGeometry(mfUiScale);
	}
	// Both font scale factors set unconditionally after the geometry block: SetupThemeGeometry's whole-style reset
	// (rStyle = defaultStyle) clears FontScaleMain to 1.0f, so re-applying here restores the user's Font Size on a
	// scale-change frame rather than dropping it for that frame.
	ImGui::GetStyle().FontScaleMain = gUiFontScale.mfCurrent;
	ImGui::GetStyle().FontScaleDpi = mfUiScale;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	// The Win32 backend re-queues the physical cursor pos in NewFrame above; re-issue the harness's synthetic pin after
	// it so the injected pos wins last-writer-wins in ImGui::NewFrame (agent harness only; pin-valid gate lives inside).
	if (gpAgentInput != nullptr)
	{
		gpAgentInput->ReissueImGuiMousePosition();
	}

	// Suppressed harness client: neutralize the two NewFrame physical polls the Win32 backend just ran. This runs after
	// the synthetic re-pin above so an active pin still wins last-writer-wins.
	if (PhysicalInputSuppressed())
	{
		ImGuiIO& rInputOutput = ImGui::GetIO();

		// (d) Physical cursor poll: unless a synthetic pin owns io.MousePos, park it at ImGui's no-mouse sentinel.
		// Missing agent input must also suppress the physical cursor rather than dereference a nullable startup global.
		if (gpAgentInput == nullptr || !gpAgentInput->mbImGuiMousePositionPinned)
		{
			rInputOutput.AddMousePosEvent(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		}

		// XInputGetState polls ImGui gamepad navigation; clear the ImGuiKey_Gamepad* range because the harness injects no gamepad events.
		for (int64_t i = ImGuiKey_GamepadStart; i <= ImGuiKey_GamepadRStickDown; ++i)
		{
			rInputOutput.AddKeyEvent(static_cast<ImGuiKey>(i), false);
		}
	}

	ImGui::NewFrame();

	// Menu screens required to progress past the pre-game / rejection flows must render even
	// with Tweaks open — otherwise a persisted-active Tweaks menu strands a fresh client with
	// no way to reach Connect/Spawn. Each self-gates internally on the game's UI state.
	mpMainMenuScreen->Render();
	mpModalScreen->Render();

	// Hide in-game UI when Tweaks menu is active
	if (!game::gpGame->mbShowImGui)
	{
		mpHudScreen->Render();
		mpPauseMenuScreen->Render();
		mpGraphicsMenuScreen->Render();
		mpSoundMenuScreen->Render();
		mpGameSettingsScreen->Render();
	}

	mpTweaksScreen->Render();

	RenderImPlotGraphs();
	RenderTextAreas();

	ImGui::Render();
	mpDrawData = ImGui::GetDrawData();

	// Publish this completed frame's widget/window snapshot as the agent registry's read table (double-buffered).
	if (gpAgentUiRegistry != nullptr)
	{
		gpAgentUiRegistry->Swap();
	}

	UpdateUiRectangleBuffers(iFramebuffer);
}

void ImGuiManager::UpdateTextArea(TextAreas eTextArea, std::string_view characters)
{
	ASSERT(common::gpMultithreading->IsMainThread());
	ASSERT(characters.size() < TextArea::kiMaxCharacters);

	TextArea& rTextArea = mTextAreas[eTextArea];
	rTextArea.iCharacterCount = static_cast<int64_t>(characters.size());
	std::memcpy(rTextArea.pcText, characters.data(), rTextArea.iCharacterCount);
}

void ImGuiManager::RenderTextAreas()
{
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
	ImFont* pFont = ImGui::GetFont();

	for (const TextArea& rTextArea : mTextAreas)
	{
		if (rTextArea.iCharacterCount == 0)
		{
			continue;
		}

		const char* pcTextEnd = rTextArea.pcText + rTextArea.iCharacterCount;
		ImVec2 position(rTextArea.fX * displaySize.x, rTextArea.fY * displaySize.y);
		float fFontSize = 0.25f * rTextArea.fSize * displaySize.y;
		ImVec2 shadowPosition(position.x + 0.00075f * displaySize.x, position.y + 0.00175f * displaySize.y);

		// Heap: ImGui may grow internal draw-list vertex/index buffers for first-use or worst-case profile text.
		ScopedSuppressAllocationTracking suppress;
		pDrawList->AddText(pFont, fFontSize, shadowPosition, IM_COL32_BLACK, rTextArea.pcText, pcTextEnd);
		pDrawList->AddText(pFont, fFontSize, position, IM_COL32_WHITE, rTextArea.pcText, pcTextEnd);
	}
}

void ImGuiManager::RegisterOpaqueRectangle(const ImVec2& rPosition, const ImVec2& rSize)
{
	if (!gOpaqueUi.Get<bool>())
	{
		return;
	}

	if (miOpaqueRectangleCount >= kiMaxUiRectangles)
	{
		return;
	}

	float fWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	float fHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);

	// Convert pixel coords to NDC [-1, 1] (Y inverted for negative viewport height)
	float fMinX = 2.0f * rPosition.x / fWidth - 1.0f;
	float fMaxX = 2.0f * (rPosition.x + rSize.x) / fWidth - 1.0f;
	float fMinY = 1.0f - 2.0f * (rPosition.y + rSize.y) / fHeight;
	float fMaxY = 1.0f - 2.0f * rPosition.y / fHeight;

	mf4OpaqueRectangles[miOpaqueRectangleCount] = {fMinX, fMinY, fMaxX, fMaxY};
	++miOpaqueRectangleCount;
}

void ImGuiManager::UpdateUiRectangleBuffers(int64_t iFramebuffer)
{
	if (miOpaqueRectangleCount > 0)
	{
		Buffer& rStorageBuffer = gpBufferManager->mUiRectangleStorageBuffers.at(iFramebuffer);
		XMFLOAT4* pf4Rectangles = reinterpret_cast<XMFLOAT4*>(rStorageBuffer.mpMappedMemory);
		std::memcpy(pf4Rectangles, mf4OpaqueRectangles, static_cast<size_t>(miOpaqueRectangleCount) * sizeof(XMFLOAT4));
	}

	mpUiPrepassIndirectMappedVkDrawIndirectCommand[iFramebuffer] = {.vertexCount = 6, .instanceCount = static_cast<uint32_t>(miOpaqueRectangleCount), .firstVertex = 0, .firstInstance = 0};
	miOpaqueRectangleCount = 0;
}

void ImGuiManager::Submit(int64_t iFramebuffer)
{
	CommandBuffers& rCommandBuffers = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebuffer);

	CHECK_VK(vkResetCommandBuffer(rCommandBuffers.mImGuiVkCommandBuffer, 0));

	VkCommandBufferBeginInfo vkCommandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	CHECK_VK(vkBeginCommandBuffer(rCommandBuffers.mImGuiVkCommandBuffer, &vkCommandBufferBeginInfo));

	gpProfileManager->ResetQueryPools(iFramebuffer, rCommandBuffers.mImGuiVkCommandBuffer, kGpuTimerUiRender, kGpuTimerCount);
	gpProfileManager->GpuStart(iFramebuffer, rCommandBuffers.mImGuiVkCommandBuffer, kGpuTimerUiRender);

	VkRenderPassBeginInfo vkRenderPassBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = mImGuiVkRenderPass,
		.framebuffer = mImGuiFramebuffers.at(iFramebuffer),
		.renderArea = VkRect2D
		{
			.offset = {.x = 0, .y = 0},
			.extent = gpGraphics->mFramebufferVkExtent2D,
		},
		.clearValueCount = 0,
		.pClearValues = nullptr,
	};
	vkCmdBeginRenderPass(rCommandBuffers.mImGuiVkCommandBuffer, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(mpDrawData, rCommandBuffers.mImGuiVkCommandBuffer);

	vkCmdEndRenderPass(rCommandBuffers.mImGuiVkCommandBuffer);

	gpProfileManager->GpuStop(iFramebuffer, rCommandBuffers.mImGuiVkCommandBuffer, kGpuTimerUiRender);

	CHECK_VK(vkEndCommandBuffer(rCommandBuffers.mImGuiVkCommandBuffer));

	VkPipelineStageFlags vkWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo vkSubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &rCommandBuffers.mMainFinishedVkSemaphore,
		.pWaitDstStageMask = &vkWaitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &rCommandBuffers.mImGuiVkCommandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &rCommandBuffers.mImGuiFinishedVkSemaphore,
	};
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkSubmitInfo, rCommandBuffers.mVkFence));
}

} // namespace engine

#endif // defined(BT_CLIENT)
