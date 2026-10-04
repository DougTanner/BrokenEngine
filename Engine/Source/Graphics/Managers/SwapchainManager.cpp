#if defined(BT_CLIENT)

#include "SwapchainManager.h"

#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

SwapchainManager::SwapchainManager(VkSwapchainKHR vkOldSwapchain)
: mPresent(common::kThreadPresent, common::kiMinWorkbufferSize)
{
	ASSERT(gpSwapchainManager == nullptr);

	gpSwapchainManager = this;

	ScopedBootTimer scopedBootTimer(kBootTimerSwapchainManager);

	CreateRenderPass();
	CreateSwapchain(vkOldSwapchain);
	CreateFramebuffers();
	CreateSynchronizationObjects();
}

void SwapchainManager::CreateRenderPass()
{
	// The scene renders into an F16 HDR intermediate so accumulated PBR / emissive / additive-particle
	// highlights are not clamped by the UNORM swapchain. The fullscreen HDR-resolve pass (kPipelineHdrResolve)
	// then tone-maps + color-grades the whole frame into the swapchain via the simplified mVkRenderPass below.

	// The HDR pass uses F16 color, a depth attachment, and optional multisampling; color ends in SHADER_READ_ONLY for the same-command-buffer resolve pass.
	VkFormat vkHdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkAttachmentDescription pVkHdrAttachmentDescriptions[]
	{
		// HDR color (resolve target when multisampling)
		VkAttachmentDescription
		{
			.flags = 0,
			.format = vkHdrFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = kbFramebufferClearColor
				? (gMultisampling.Get<bool>() ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR)
				: VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
		VkAttachmentDescription
		{
			.flags = 0,
			.format = gpInstanceManager->mDepthVkFormat,
			.samples = gMultisampling.Get<bool>() ? gSampleCount.Get<VkSampleCountFlagBits>() : VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		},
		VkAttachmentDescription
		{
			.flags = 0,
			.format = vkHdrFormat,
			.samples = gSampleCount.Get<VkSampleCountFlagBits>(),
			.loadOp = kbFramebufferClearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
	};

	VkAttachmentReference vkHdrColorAttachmentReference
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	VkAttachmentReference vkDepthAttachmentReference
	{
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};
	VkAttachmentReference vkMultisamplingAttachmentReference
	{
		.attachment = 2,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	VkSubpassDescription vkHdrSubpassDescription
	{
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = gMultisampling.Get<bool>() ? &vkMultisamplingAttachmentReference : &vkHdrColorAttachmentReference,
		.pResolveAttachments = gMultisampling.Get<bool>() ? &vkHdrColorAttachmentReference : nullptr,
		.pDepthStencilAttachment = &vkDepthAttachmentReference,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	// Incoming: WAR against the previous frame's resolve pass sampling this HDR color (FRAGMENT_SHADER read),
	// plus the acquire-time color/depth transition. Outgoing: order the same-command-buffer resolve pass's
	// sampling (COLOR_ATTACHMENT_WRITE -> FRAGMENT_SHADER SHADER_READ).
	// Incoming also covers a depth WAW: the depth attachment is the single mDepthTexture reused every frame with
	// UNDEFINED initial layout + CLEAR loadOp, so the previous frame's DEPTH_STENCIL_ATTACHMENT_WRITEs (at the
	// EARLY/LATE_FRAGMENT_TESTS stages) must be made available before this frame's automatic
	// UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL transition/clear -- otherwise a latent write-after-write the
	// sync-validation layer flags.
	VkSubpassDependency pVkHdrSubpassDependencies[]
	{
		VkSubpassDependency
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0,
		},
		VkSubpassDependency
		{
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = 0,
		},
	};

	VkRenderPassCreateInfo vkHdrRenderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = gMultisampling.Get<bool>() ? 3ui32 : 2ui32,
		.pAttachments = pVkHdrAttachmentDescriptions,
		.subpassCount = 1,
		.pSubpasses = &vkHdrSubpassDescription,
		.dependencyCount = static_cast<uint32_t>(std::size(pVkHdrSubpassDependencies)),
		.pDependencies = pVkHdrSubpassDependencies,
	};
	CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkHdrRenderPassCreateInfo, nullptr, &mHdrVkRenderPass));
	VkName(VK_OBJECT_TYPE_RENDER_PASS, mHdrVkRenderPass, "SwapchainManagerHdr");

	// Present render pass: one single-sample color attachment for the resolve quad; scene pipelines target the HDR pass, so no depth/MSAA. Full
	// overwrite permits DONT_CARE load; final layout is PRESENT_SRC_KHR, with a color-only acquire-semaphore dependency.
	VkAttachmentDescription vkPresentAttachmentDescription
	{
		.flags = 0,
		.format = gpInstanceManager->mFramebufferVkFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};
	VkAttachmentReference vkPresentAttachmentReference
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	VkSubpassDescription vkPresentSubpassDescription
	{
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &vkPresentAttachmentReference,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	// Wait to write colors until the acquired image is available (see the swapchain acquire semaphore chain).
	VkSubpassDependency vkPresentSubpassDependency
	{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = 0,
	};
	VkRenderPassCreateInfo vkRenderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = 1,
		.pAttachments = &vkPresentAttachmentDescription,
		.subpassCount = 1,
		.pSubpasses = &vkPresentSubpassDescription,
		.dependencyCount = 1,
		.pDependencies = &vkPresentSubpassDependency,
	};
	CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkRenderPassCreateInfo, nullptr, &mVkRenderPass));
	VkName(VK_OBJECT_TYPE_RENDER_PASS, mVkRenderPass, "SwapchainManager");
}

void SwapchainManager::CreateSwapchain(VkSwapchainKHR vkOldSwapchain)
{
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR {};
	CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpInstanceManager->mVkPhysicalDevice, gpInstanceManager->mVkSurfaceKHR, &vkSurfaceCapabilitiesKHR));
	ASSERT((vkSurfaceCapabilitiesKHR.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)) != 0);

	ASSERT((vkSurfaceCapabilitiesKHR.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0);
	VkImageUsageFlags vkSwapchainUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if constexpr (kbScreenshots)
	{
		if ((vkSurfaceCapabilitiesKHR.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
		{
			vkSwapchainUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		else
		{
			LOG(kGraphics, kWarning, "WARNING: Screenshot functionality will be disabled - VK_IMAGE_USAGE_TRANSFER_SRC_BIT not supported by surface");
		}
	}

	uint32_t uiPresentModeCount = 0;
	CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpInstanceManager->mVkPhysicalDevice, gpInstanceManager->mVkSurfaceKHR, &uiPresentModeCount, nullptr));
	ASSERT(uiPresentModeCount != 0);
	std::vector<VkPresentModeKHR> physicalDevicePresentModes(uiPresentModeCount);
	CHECK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpInstanceManager->mVkPhysicalDevice, gpInstanceManager->mVkSurfaceKHR, &uiPresentModeCount, physicalDevicePresentModes.data()));

	LOG(kGraphics, kInfo, "Present modes ({}):", std::ssize(physicalDevicePresentModes));
	// FIFO is guaranteed to be available
	VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
	for (const VkPresentModeKHR& rVkPresentModeKHR : physicalDevicePresentModes)
	{
		LOG(kGraphics, kInfo, "  {}", string_VkPresentModeKHR(rVkPresentModeKHR));

		if (rVkPresentModeKHR == VK_PRESENT_MODE_MAILBOX_KHR && gPresentMode.Get<VkPresentModeKHR>() == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
		}
		else if (rVkPresentModeKHR == VK_PRESENT_MODE_IMMEDIATE_KHR && gPresentMode.Get<VkPresentModeKHR>() == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			vkPresentModeKHR = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}
	LOG(kGraphics, kInfo, "Present mode selected: {}", string_VkPresentModeKHR(vkPresentModeKHR));
	gPresentMode.Reset(vkPresentModeKHR);

	// The swap extent is the resolution of the swap chain images and it's almost always exactly equal to the resolution of the window that we're drawing to
	RECT clientRect {};
	GetClientRect(gpGraphics->mWindowHandle, &clientRect);
	LOG(kGraphics, kDebug, "Swapchain extent resolution: currentExtent {} x {}, minImageExtent {} x {}, maxImageExtent {} x {}, client rect {} x {}", vkSurfaceCapabilitiesKHR.currentExtent.width, vkSurfaceCapabilitiesKHR.currentExtent.height, vkSurfaceCapabilitiesKHR.minImageExtent.width, vkSurfaceCapabilitiesKHR.minImageExtent.height, vkSurfaceCapabilitiesKHR.maxImageExtent.width, vkSurfaceCapabilitiesKHR.maxImageExtent.height, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

	if (vkSurfaceCapabilitiesKHR.currentExtent.width == 0xFFFFFFFF)
	{
		// If the surface size is undefined, the size is set to the size of the images requested
		gpGraphics->mFramebufferVkExtent2D.width = std::clamp(gVkWantedFramebufferExtent2D.width, vkSurfaceCapabilitiesKHR.minImageExtent.width, vkSurfaceCapabilitiesKHR.maxImageExtent.width);
		gpGraphics->mFramebufferVkExtent2D.height = std::clamp(gVkWantedFramebufferExtent2D.height, vkSurfaceCapabilitiesKHR.minImageExtent.height, vkSurfaceCapabilitiesKHR.maxImageExtent.height);
	}
	else
	{
		// If the surface size is defined, the swapchain size must match. Clamp to [minImageExtent, maxImageExtent]
		// symmetrically with the undefined branch — a degenerate currentExtent must never reach .imageExtent or any attachment.
		gpGraphics->mFramebufferVkExtent2D.width = std::clamp(vkSurfaceCapabilitiesKHR.currentExtent.width, vkSurfaceCapabilitiesKHR.minImageExtent.width, vkSurfaceCapabilitiesKHR.maxImageExtent.width);
		gpGraphics->mFramebufferVkExtent2D.height = std::clamp(vkSurfaceCapabilitiesKHR.currentExtent.height, vkSurfaceCapabilitiesKHR.minImageExtent.height, vkSurfaceCapabilitiesKHR.maxImageExtent.height);

		gVkWantedFramebufferExtent2D = gpGraphics->mFramebufferVkExtent2D;
	}

	mfAspectRatio = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width) / static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);

	// Prefer at least three swapchain images within surface limits so rendering below the vsync rate can use triple buffering.
	uint32_t uiMinImageCount = std::max(3ui32, vkSurfaceCapabilitiesKHR.minImageCount);
	if (vkSurfaceCapabilitiesKHR.maxImageCount > 0)
	{
		uiMinImageCount = std::min(uiMinImageCount, vkSurfaceCapabilitiesKHR.maxImageCount);
	}
	LOG(kGraphics, kDebug, "uiMinImageCount: {}", uiMinImageCount);

	uint32_t pQueueFamilyIndices[]
	{
		static_cast<uint32_t>(gpInstanceManager->miGraphicsQueueFamilyIndex),
		static_cast<uint32_t>(gpInstanceManager->miPresentQueueFamilyIndex),
	};

	// If the graphics and present queues are from different queue families, we either have to explicitly transfer ownership of images between the
	// queues, or we have to create the swapchain with imageSharingMode as VK_SHARING_MODE_CONCURRENT
	bool bDifferentQueueFamilies = gpInstanceManager->miGraphicsQueueFamilyIndex != gpInstanceManager->miPresentQueueFamilyIndex;

	VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = gpInstanceManager->mVkSurfaceKHR,
		.minImageCount = uiMinImageCount,
		.imageFormat = gpInstanceManager->mFramebufferVkFormat,
		.imageColorSpace = gpInstanceManager->mFramebufferVkColorSpaceKHR,
		.imageExtent = gpGraphics->mFramebufferVkExtent2D,
		.imageArrayLayers = 1,
		.imageUsage = vkSwapchainUsageFlags,
		.imageSharingMode = bDifferentQueueFamilies ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = bDifferentQueueFamilies ? 2ui32 : 0ui32,
		.pQueueFamilyIndices = bDifferentQueueFamilies ? &pQueueFamilyIndices[0] : nullptr,
		.preTransform = (vkSurfaceCapabilitiesKHR.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : vkSurfaceCapabilitiesKHR.currentTransform,
		.compositeAlpha = (vkSurfaceCapabilitiesKHR.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0 ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		.presentMode = gPresentMode.Get<VkPresentModeKHR>(),
		.clipped = VK_TRUE,
		.oldSwapchain = vkOldSwapchain,
	};
	CHECK_VK(vkCreateSwapchainKHR(gpDeviceManager->mVkDevice, &vkSwapchainCreateInfoKHR, nullptr, &mVkSwapchainKHR));

	// Destroy old swapchain after successfully creating new one
	if (vkOldSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(gpDeviceManager->mVkDevice, vkOldSwapchain, nullptr);
	}
	VkName(VK_OBJECT_TYPE_SWAPCHAIN_KHR, mVkSwapchainKHR, "");
}

void SwapchainManager::CreateFramebuffers()
{
	uint32_t uiImageCount = 0;
	CHECK_VK(vkGetSwapchainImagesKHR(gpDeviceManager->mVkDevice, mVkSwapchainKHR, &uiImageCount, nullptr));
	ASSERT(uiImageCount != 0);
	std::vector<VkImage> swapchainImages(uiImageCount);
	CHECK_VK(vkGetSwapchainImagesKHR(gpDeviceManager->mVkDevice, mVkSwapchainKHR, &uiImageCount, swapchainImages.data()));

	VkImageAspectFlags vkImageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (gpInstanceManager->mDepthVkFormat == VK_FORMAT_D16_UNORM_S8_UINT || gpInstanceManager->mDepthVkFormat == VK_FORMAT_D24_UNORM_S8_UINT || gpInstanceManager->mDepthVkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
	{
		vkImageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	mDepthTexture.Create(TextureInfo
	{
		.textureFlags = {},
		.name = "Depth",
		.vkImageCreateFlags = 0,
		.vkFormat = gpInstanceManager->mDepthVkFormat,
		.vkExtent3D = VkExtent3D {.width = gpGraphics->mFramebufferVkExtent2D.width, .height = gpGraphics->mFramebufferVkExtent2D.height, .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = gMultisampling.Get<bool>() ? gSampleCount.Get<VkSampleCountFlagBits>() : VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = vkImageAspectFlags,
		.eTextureLayout = TextureLayout::kUndefined,
	});

	// Multisampling (F16 HDR MSAA attachment for mHdrVkRenderPass, resolved into mHdrTexture)
	if (gMultisampling.Get<bool>())
	{
		mMultisamplingTexture.Create(TextureInfo
		{
			.textureFlags = {},
			.name = "Multisampling",
			.vkImageCreateFlags = 0,
			.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
			.vkExtent3D = VkExtent3D {.width = gpGraphics->mFramebufferVkExtent2D.width, .height = gpGraphics->mFramebufferVkExtent2D.height, .depth = 1},
			.uiMipLevels = 1,
			.uiArrayLayers = 1,
			.vkSampleCountFlagBits = gSampleCount.Get<VkSampleCountFlagBits>(),
			.vkImageUsageFlags = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
			.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
			.eTextureLayout = TextureLayout::kUndefined,
		});
	}

	// HDR intermediate color: the scene render pass writes here; the resolve pass samples it.
	mHdrTexture.Create(TextureInfo
	{
		.textureFlags = {},
		.name = "Hdr",
		.vkImageCreateFlags = 0,
		.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
		.vkExtent3D = VkExtent3D {.width = gpGraphics->mFramebufferVkExtent2D.width, .height = gpGraphics->mFramebufferVkExtent2D.height, .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});

	// Single HDR framebuffer: none of its attachments (HDR color / depth / MSAA) are per-swapchain-image.
	// Attachment order matches mHdrVkRenderPass: HDR color (0), depth (1), MSAA (2).
	VkImageView pVkHdrImageViews[] {mHdrTexture.mVkImageView, mDepthTexture.mVkImageView, mMultisamplingTexture.mVkImageView};
	VkFramebufferCreateInfo vkHdrFramebufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = mHdrVkRenderPass,
		.attachmentCount = gMultisampling.Get<bool>() ? 3ui32 : 2ui32,
		.pAttachments = pVkHdrImageViews,
		.width = gpGraphics->mFramebufferVkExtent2D.width,
		.height = gpGraphics->mFramebufferVkExtent2D.height,
		.layers = 1,
	};
	CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkHdrFramebufferCreateInfo, nullptr, &mHdrVkFramebuffer));
	VkName(VK_OBJECT_TYPE_FRAMEBUFFER, mHdrVkFramebuffer, "Hdr");

	mFramebuffers.resize(uiImageCount);
	miFramebufferIndex = 0;
	for (int64_t i = 0; Framebuffer& rFrameBuffer : mFramebuffers)
	{
		rFrameBuffer.vkPresentImage = swapchainImages.at(i++);

		VkImageViewCreateInfo vkImageViewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = rFrameBuffer.vkPresentImage,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = gpInstanceManager->mFramebufferVkFormat,
			.components = VkComponentMapping
			{
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_G,
				.b = VK_COMPONENT_SWIZZLE_B,
				.a = VK_COMPONENT_SWIZZLE_A,
			},
			.subresourceRange = VkImageSubresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		CHECK_VK(vkCreateImageView(gpDeviceManager->mVkDevice, &vkImageViewCreateInfo, nullptr, &rFrameBuffer.vkPresentImageView));
		VkName(VK_OBJECT_TYPE_IMAGE, rFrameBuffer.vkPresentImage, std::format("Present {}", i - 1).c_str());
		VkName(VK_OBJECT_TYPE_IMAGE_VIEW, rFrameBuffer.vkPresentImageView, std::format("Present {}", i - 1).c_str());

		// The present pass uses one framebuffer per swapchain image because the acquired image is its color attachment; HDR color, depth, and MSAA
		// remain on mHdrVkFramebuffer.
		VkImageView pVkImageViews[] {rFrameBuffer.vkPresentImageView};
		VkFramebufferCreateInfo vkFramebufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = mVkRenderPass,
			.attachmentCount = 1,
			.pAttachments = pVkImageViews,
			.width = gpGraphics->mFramebufferVkExtent2D.width,
			.height = gpGraphics->mFramebufferVkExtent2D.height,
			.layers = 1,
		};
		CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkFramebufferCreateInfo, nullptr, &rFrameBuffer.vkPresentFramebuffer));
		VkName(VK_OBJECT_TYPE_FRAMEBUFFER, rFrameBuffer.vkPresentFramebuffer, std::format("Present {}", i - 1).c_str());
	}
}

void SwapchainManager::CreateSynchronizationObjects()
{
	mImageAvailableFences.resize(mFramebuffers.size());
	miFenceAvailableIndex = 0;
	for ([[maybe_unused]] int64_t i = 0; VkFence& rVkFence : mImageAvailableFences)
	{
		VkFenceCreateInfo vkFenceCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		CHECK_VK(vkCreateFence(gpDeviceManager->mVkDevice, &vkFenceCreateInfo, nullptr, &rVkFence));
		VkName(VK_OBJECT_TYPE_FENCE, rVkFence, std::format("ImageAvailable {}", i++).c_str());
	}

	mImageAvailableSemaphores.resize(mFramebuffers.size() + 1);
	miImageAvailableIndex = 0;
	for ([[maybe_unused]] int64_t i = 0; VkSemaphore& rVkSemaphore : mImageAvailableSemaphores)
	{
		VkSemaphoreCreateInfo vkSemaphoreCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};
		CHECK_VK(vkCreateSemaphore(gpDeviceManager->mVkDevice, &vkSemaphoreCreateInfo, nullptr, &rVkSemaphore));
		VkName(VK_OBJECT_TYPE_SEMAPHORE, rVkSemaphore, std::format("ImageAvailable {}", i++).c_str());
	}
}

SwapchainManager::~SwapchainManager()
{
	for (const VkFence& rVkFence : mImageAvailableFences)
	{
		vkDestroyFence(gpDeviceManager->mVkDevice, rVkFence, nullptr);
	}

	for (const VkSemaphore& rVkSemaphore : mImageAvailableSemaphores)
	{
		vkDestroySemaphore(gpDeviceManager->mVkDevice, rVkSemaphore, nullptr);
	}

	for (const Framebuffer& rFramebuffer : mFramebuffers)
	{
		vkDestroyFramebuffer(gpDeviceManager->mVkDevice, rFramebuffer.vkPresentFramebuffer, nullptr);

		vkDestroyImageView(gpDeviceManager->mVkDevice, rFramebuffer.vkPresentImageView, nullptr);
	}

	if (mVkSwapchainKHR != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(gpDeviceManager->mVkDevice, mVkSwapchainKHR, nullptr);
	}
	vkDestroyFramebuffer(gpDeviceManager->mVkDevice, mHdrVkFramebuffer, nullptr);
	vkDestroyRenderPass(gpDeviceManager->mVkDevice, mHdrVkRenderPass, nullptr);
	vkDestroyRenderPass(gpDeviceManager->mVkDevice, mVkRenderPass, nullptr);

	if (gpSwapchainManager == this)
	{
		gpSwapchainManager = nullptr;
	}
}

VkSwapchainKHR SwapchainManager::ReleaseHandleForRecreation()
{
	VkSwapchainKHR vkSwapchainKHR = mVkSwapchainKHR;
	mVkSwapchainKHR = VK_NULL_HANDLE;
	return vkSwapchainKHR;
}

void SwapchainManager::AcquireNextImage()
{
	ScopedCpuProfile scopedCpuProfile(kCpuTimerAcquireImage);

	if (mCurrentImageAvailableVkFence != VK_NULL_HANDLE)
	{
		// Make sure previous image has been fully acquired before proceeding
		ScopedCpuProfile scopedCpuProfileFence(kCpuTimerAcquireImageFence);
		CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &mCurrentImageAvailableVkFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));
		mCurrentImageAvailableVkFence = VK_NULL_HANDLE;
	}

	mCurrentImageAvailableVkFence = GetNextImageAvailableFence();
	CHECK_VK(vkResetFences(gpDeviceManager->mVkDevice, 1, &mCurrentImageAvailableVkFence));
	mImageAvailableVkSemaphore = GetNextImageAvailableSemaphore();
	uint32_t uiFramebufferIndex = 0xFFFFFFFF;
	VkResult vkResult = vkAcquireNextImageKHR(gpDeviceManager->mVkDevice, mVkSwapchainKHR, UINT64_MAX, mImageAvailableVkSemaphore, mCurrentImageAvailableVkFence, &uiFramebufferIndex);
	if (vkResult == VK_SUCCESS)
	{
		miFramebufferIndex = uiFramebufferIndex;
	}
	// Handle stale swapchain by requesting deferred recreation
	else if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
	{
		mCurrentImageAvailableVkFence = VK_NULL_HANDLE;
		// std::max, not plain assign: a same-frame kSurface escalation must never downgrade to kSwapchain.
		gpGraphics->meDestroyType = std::max(DestroyType::kSwapchain, gpGraphics->meDestroyType);
	}
	else
	{
		mCurrentImageAvailableVkFence = VK_NULL_HANDLE;
		CHECK_VK(vkResult);
	}
}

void SwapchainManager::PresentToQueue(int64_t iFramebufferIndex)
{
	CommandBuffers& rCommandBuffers = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebufferIndex);

	uint32_t uiCurrentFramebufferIndex = static_cast<uint32_t>(iFramebufferIndex);
	VkSemaphore vkWaitSemaphore = rCommandBuffers.mImGuiFinishedVkSemaphore;
	VkPresentInfoKHR vkPresentInfoKHR
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vkWaitSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &mVkSwapchainKHR,
		.pImageIndices = &uiCurrentFramebufferIndex,
		.pResults = nullptr,
	};

	gpProfileManager->CpuStart(kCpuTimerPresent);
	VkResult vkResult = vkQueuePresentKHR(gpDeviceManager->mPresentVkQueue, &vkPresentInfoKHR);
	gpProfileManager->CpuStop(kCpuTimerPresent);

	// Handle stale swapchain by requesting deferred recreation
	if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
	{
		// The kbRenderThread frame-tail mPresent.Wait() in Graphics::RenderMainPresentAcquire publishes this write before Refresh() in the next Create() and prevents the next frame's Global submit from racing vkQueuePresentKHR on the shared queue.
		// A same-frame kSurface escalation must never downgrade to kSwapchain.
		gpGraphics->meDestroyType = std::max(DestroyType::kSwapchain, gpGraphics->meDestroyType);
		return;
	}
	CHECK_VK(vkResult);
}

void SwapchainManager::Present(int64_t iFramebufferIndex)
{
	if constexpr (kbRenderThread)
	{
		mPresent.Wake([this, iFramebufferIndex]()
		{
			gpCommandBufferManager->mSubmitMain.Wait();
			PresentToQueue(iFramebufferIndex);
		});
	}
	else
	{
		PresentToQueue(iFramebufferIndex);
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
