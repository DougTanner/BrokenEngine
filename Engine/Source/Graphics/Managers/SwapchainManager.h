#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct Framebuffer
{
	Framebuffer() = default;
	Framebuffer(const Framebuffer&) = delete;
	Framebuffer& operator=(const Framebuffer&) = delete;
	// std::vector<Framebuffer>::resize requires move construction. Defaulted moves copy the handles; SwapchainManager frees them, and Framebuffer destruction releases nothing.
	Framebuffer(Framebuffer&&) = default;
	Framebuffer& operator=(Framebuffer&&) = default;

	VkImage vkPresentImage = VK_NULL_HANDLE;
	VkImageView vkPresentImageView = VK_NULL_HANDLE;
	VkFramebuffer vkPresentFramebuffer = VK_NULL_HANDLE;
};

class SwapchainManager
{
public:

	SwapchainManager(VkSwapchainKHR vkOldSwapchain = VK_NULL_HANDLE);
	~SwapchainManager();

	void AcquireNextImage();
	void Present(int64_t iFramebufferIndex);

	// The transferred handle remains live across swapchain recreation.
	VkSwapchainKHR ReleaseHandleForRecreation();

	float mfAspectRatio = 1.0f;

	Texture mDepthTexture;
	Texture mMultisamplingTexture;

	// HDR intermediate: the scene renders into this F16 target (mHdrVkRenderPass / mHdrVkFramebuffer);
	// the fullscreen HDR-resolve pass samples it and writes the swapchain via mVkRenderPass. Single
	// framebuffer (no per-swapchain-image attachment among the HDR pass's color/MSAA/depth).
	Texture mHdrTexture;

	std::vector<Framebuffer> mFramebuffers;
	int64_t miFramebufferIndex = 0;

	VkSemaphore mImageAvailableVkSemaphore = VK_NULL_HANDLE;

	VkRenderPass mVkRenderPass = VK_NULL_HANDLE;
	VkRenderPass mHdrVkRenderPass = VK_NULL_HANDLE;
	VkFramebuffer mHdrVkFramebuffer = VK_NULL_HANDLE;
	VkSwapchainKHR mVkSwapchainKHR = VK_NULL_HANDLE;

	common::PersistentWorker mPresent = common::PersistentWorker(common::kThreadPresent, common::kiMinWorkbufferSize);

private:

	void CreateRenderPass();
	void CreateSwapchain(VkSwapchainKHR vkOldSwapchain);
	void CreateFramebuffers();
	void CreateSynchronizationObjects();

	// Round-robin acquire sync objects, advanced by the GetNext* accessors below.
	std::vector<VkSemaphore> mImageAvailableSemaphores;
	int64_t miImageAvailableIndex = 0;
	std::vector<VkFence> mImageAvailableFences;
	int64_t miFenceAvailableIndex = 0;

	inline VkSemaphore GetNextImageAvailableSemaphore()
	{
		VkSemaphore vkSemaphore = mImageAvailableSemaphores.at(miImageAvailableIndex);

		++miImageAvailableIndex;
		if (miImageAvailableIndex == std::ssize(mImageAvailableSemaphores))
		{
			miImageAvailableIndex = 0;
		}

		return vkSemaphore;
	}

	inline VkFence GetNextImageAvailableFence()
	{
		VkFence vkFence = mImageAvailableFences.at(miFenceAvailableIndex);

		++miFenceAvailableIndex;
		if (miFenceAvailableIndex == std::ssize(mImageAvailableFences))
		{
			miFenceAvailableIndex = 0;
		}

		return vkFence;
	}

	void PresentToQueue(int64_t iFramebufferIndex);

	VkFence mCurrentImageAvailableVkFence = VK_NULL_HANDLE;
};

inline SwapchainManager* gpSwapchainManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
