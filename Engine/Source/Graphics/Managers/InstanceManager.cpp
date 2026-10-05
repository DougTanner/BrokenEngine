#if defined(BT_CLIENT)

#include "InstanceManager.h"

#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Game.h"

namespace engine
{

constexpr char kpcKhronosValidation[] = "VK_LAYER_KHRONOS_validation";
constexpr const char* kppcValidationLayers[]
{
	kpcKhronosValidation,
	"VK_LAYER_KHRONOS_synchronization2",
};

constexpr const char* kppcInstanceExtensionNames[]
{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	VK_EXT_LAYER_SETTINGS_EXTENSION_NAME,
};
constexpr int64_t kiBaseExtensionCount = 3;
constexpr int64_t kiDebugExtensionCount = 5;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT vkMessageSeverity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT vkMessageType, [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* pVkCallbackData, [[maybe_unused]] void* pUserData)
{
	if constexpr (kbVulkanDebugLayers)
	{
		if (pVkCallbackData->pMessageIdName != nullptr && std::strstr(pVkCallbackData->pMessageIdName, "TransitionUndefinedToReadOnly") != nullptr)
		{
			// Lazy-loaded textures transition from UNDEFINED to SHADER_READ_ONLY before their contents are uploaded.
			return VK_FALSE;
		}

		// VK_KHR_maintenance9 permits sampled/transfer images to omit queue-family ownership transfers;
		// ConcurrentUsageOfExclusiveImage does not account for that extension. Startup's GeneratePhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTable draws
		// reference transfer-uploaded textures; regular-render commands are recorded before texture adoption.
		if (pVkCallbackData->pMessageIdName != nullptr && std::strstr(pVkCallbackData->pMessageIdName, "ConcurrentUsageOfExclusiveImage") != nullptr)
		{
			return VK_FALSE;
		}

		if (pVkCallbackData->pMessageIdName != nullptr && std::strstr(pVkCallbackData->pMessageIdName, "VkDescriptorSetAllocateInfo-descriptorCount") != nullptr)
		{
			LOG(kDefault, kError, "Double the number of descriptor sets in DeviceManager::DeviceManager() {}", pVkCallbackData->pMessage);
			// DEBUG_BREAK, not ASSERT: the Vulkan C callback frame is not a supported unwind path, so a throw here
			// terminates the process over a validation-layer message instead of returning to the driver
			DEBUG_BREAK();
			return VK_FALSE;
		}

		// VK_SUBOPTIMAL_KHR from present is expected while a window resize / fullscreen transition races the surface: presentation still succeeds and the next frame's swapchain recreate resolves it. Suppress the best-practices warning; the recreate path already handles the result code.
		if (pVkCallbackData->pMessageIdName != nullptr && std::strstr(pVkCallbackData->pMessageIdName, "SuboptimalSwapchain") != nullptr)
		{
			return VK_FALSE;
		}

		// GPU-AV adjusts settings for unsupported rayTracingInvocationReorder/meshShader features at vkCreateDevice; BuildValidationLayerSettings disables those checks.
		if (pVkCallbackData->pMessageIdName != nullptr && std::strstr(pVkCallbackData->pMessageIdName, "WARNING-Setting-Limit-Adjusted") != nullptr)
		{
			return VK_FALSE;
		}

		if (pVkCallbackData->pMessageIdName != nullptr && std::strstr(pVkCallbackData->pMessageIdName, "DEBUG-PRINTF") != nullptr)
		{
			// Zero-alloc: log the last line of the message (the printf output) without splitting into heap strings — this
			// callback is reachable at per-draw frequency in kbDebugPrintf builds with allocation tracking live.
			// find_last_of returns npos when there is no newline; npos + 1 == 0 yields the whole message via substr.
			std::string_view message(pVkCallbackData->pMessage);
			std::string_view lastLine = message.substr(message.find_last_of('\n') + 1);
			LOG(kGraphics, kInfo, "[debugPrintfEXT] {}", lastLine);
			return VK_FALSE;
		}

		if ((vkMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0 || (vkMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
		{
			return VK_FALSE;
		}

		LOG(kDefault, kError, "DebugUtilsCallback {} {} \"{}\" \"{}\"", static_cast<uint64_t>(vkMessageSeverity), static_cast<uint64_t>(vkMessageType), pVkCallbackData->pMessageIdName, pVkCallbackData->pMessage);
		DEBUG_BREAK();
	}

	return VK_FALSE;
}

static VkSampleCountFlagBits SelectSampleCount(VkSampleCountFlags vkSampleCountFlags)
{
	return static_cast<VkSampleCountFlagBits>(std::bit_floor(vkSampleCountFlags | VK_SAMPLE_COUNT_1_BIT));
}

VkSampleCountFlagBits InstanceManager::SelectSupportedSampleCount(VkSampleCountFlagBits vkRequested) const
{
	VkSampleCountFlags vkSupported = mVkPhysicalDeviceProperties.limits.framebufferColorSampleCounts & mVkPhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if ((vkSupported & vkRequested) != 0)
	{
		return vkRequested;
	}

	// Highest supported count above one that does not exceed the request; when there is none (only an unsupported
	// request of 2), 4 is returned because the Vulkan required limits guarantee it.
	return SelectSampleCount((vkSupported & (vkRequested | (vkRequested - 1))) | VK_SAMPLE_COUNT_4_BIT);
}

// Stable storage for the VkLayerSettingEXT pValues pointers — Vulkan reads them at vkCreateInstance, after BuildValidationLayerSettings has returned, so they must outlive the helper's stack frame.
[[maybe_unused]] constexpr VkBool32 kVkLayerSettingTrue = VK_TRUE;
[[maybe_unused]] constexpr VkBool32 kVkLayerSettingFalse = VK_FALSE;

// The caller-owned settings array must remain alive until vkCreateInstance returns.
static int64_t BuildValidationLayerSettings(VkLayerSettingEXT (&rVkLayerSettings)[7])
{
	rVkLayerSettings[0] =
	{
		.pLayerName = kpcKhronosValidation,
		.pSettingName = "validate_best_practices",
		.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
		.valueCount = 1,
		.pValues = &kVkLayerSettingTrue,
	};
	rVkLayerSettings[1] =
	{
		.pLayerName = kpcKhronosValidation,
		.pSettingName = "validate_sync",
		.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
		.valueCount = 1,
		.pValues = &kVkLayerSettingTrue,
	};
	int64_t iLayerSettingCount = 2;
	if constexpr (kbGraphicsProcessingUnitAssistedValidation)
	{
		rVkLayerSettings[iLayerSettingCount++] =
		{
			.pLayerName = kpcKhronosValidation,
			.pSettingName = "gpuav_enable",
			.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
			.valueCount = 1,
			.pValues = &kVkLayerSettingTrue,
		};
		rVkLayerSettings[iLayerSettingCount++] =
		{
			.pLayerName = kpcKhronosValidation,
			.pSettingName = "gpuav_shader_instrumentation",
			.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
			.valueCount = 1,
			.pValues = &kVkLayerSettingTrue,
		};
		rVkLayerSettings[iLayerSettingCount++] =
		{
			.pLayerName = kpcKhronosValidation,
			.pSettingName = "gpuav_validate_ray_query",
			.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
			.valueCount = 1,
			.pValues = &kVkLayerSettingFalse,
		};
		// Disable GPU-AV sub-checks whose device features this hardware lacks (rayTracingInvocationReorder / meshShader). Otherwise GPU-AV defaults them on and the layer logs a WARNING-Setting-Limit-Adjusted at vkCreateDevice while auto-disabling them. gpuav_validate_ray_hit_object is a valid internal key (the layer prints it) but is absent from the JSON manifest.
		rVkLayerSettings[iLayerSettingCount++] =
		{
			.pLayerName = kpcKhronosValidation,
			.pSettingName = "gpuav_validate_ray_hit_object",
			.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
			.valueCount = 1,
			.pValues = &kVkLayerSettingFalse,
		};
		rVkLayerSettings[iLayerSettingCount++] =
		{
			.pLayerName = kpcKhronosValidation,
			.pSettingName = "gpuav_mesh_shading",
			.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
			.valueCount = 1,
			.pValues = &kVkLayerSettingFalse,
		};
	}
	else if constexpr (kbDebugPrintf)
	{
		rVkLayerSettings[iLayerSettingCount++] =
		{
			.pLayerName = kpcKhronosValidation,
			.pSettingName = "printf_enable",
			.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
			.valueCount = 1,
			.pValues = &kVkLayerSettingTrue,
		};
	}

	ASSERT(iLayerSettingCount <= std::ssize(rVkLayerSettings));
	return iLayerSettingCount;
}

// Opt-in force-load: lets RenderDoc's "Attach to running instance" find us without launching through RenderDoc. Triggers the layer-disable branch in the ctor, so the runtime --renderdoc launch option gates it (atop compile-time kbRenderDoc) to avoid sacrificing validation in normal debug runs.
static void TryLoadRenderDocDll()
{
	if constexpr (kbRenderDoc)
	{
		if (!(gLaunchOptions.flags & LaunchOptionFlags::kRenderDoc))
		{
			return;
		}

		if (GetModuleHandle("renderdoc.dll") == nullptr)
		{
			// Default RenderDoc installer doesn't add itself to PATH, so plain LoadLibrary("renderdoc.dll") fails. Read the install dir from the Vulkan loader's implicit-layer JSON registration — renderdoc.dll lives in the same folder.
			HKEY hKey = nullptr;
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
			{
				char pcValueName[MAX_PATH] {};
				for (int64_t i = 0; ; ++i)
				{
					DWORD uiValueNameCharacterCount = MAX_PATH;
					if (RegEnumValue(hKey, static_cast<DWORD>(i), pcValueName, &uiValueNameCharacterCount, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
					{
						break;
					}
					if (std::strstr(pcValueName, "renderdoc.json") != nullptr)
					{
						char* pcLastSeparator = std::strrchr(pcValueName, '\\');
						if (pcLastSeparator != nullptr)
						{
							strcpy_s(pcLastSeparator + 1, MAX_PATH - (pcLastSeparator + 1 - pcValueName), "renderdoc.dll");
							LoadLibrary(pcValueName);
						}
						break;
					}
				}
				RegCloseKey(hKey);
			}

			// PATH-relative fallback for users who manually added RenderDoc to PATH
			if (GetModuleHandle("renderdoc.dll") == nullptr)
			{
				LoadLibrary("renderdoc.dll");
			}

			if (GetModuleHandle("renderdoc.dll") == nullptr)
			{
				LOG(kGraphics, kWarning, "--renderdoc requested but renderdoc.dll could not be loaded. Confirm RenderDoc is installed and registered in HKLM\\SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers.");
			}
		}
	}
}

InstanceManager::InstanceManager(HINSTANCE hInstance, HWND hWindow)
{
	ASSERT(gpInstanceManager == nullptr);

	gpInstanceManager = this;

	ScopedBootTimer scopedBootTimer(kBootTimerInstanceManager);

	if constexpr (kbVulkanDebugLayers)
	{
		ReadLayerProperties();
	}

	VkApplicationInfo vkApplicationInfo
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = game::kGameName.data(),
		.applicationVersion = game::kiGameVersion,
		.pEngineName = nullptr,
		.engineVersion = 0,
		.apiVersion = VK_API_VERSION_1_2, // Also update "--target-env vulkan1.2" in DataPacker
	};
	VkLayerSettingEXT pVkLayerSettings[7] {};
	int64_t iLayerSettingCount = BuildValidationLayerSettings(pVkLayerSettings);

	VkLayerSettingsCreateInfoEXT vkLayerSettingsCreateInfoEXT =
	{
		.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
		.pNext = nullptr,
		.settingCount = static_cast<uint32_t>(iLayerSettingCount),
		.pSettings = pVkLayerSettings,
	};
	VkInstanceCreateInfo vkInstanceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = kbVulkanDebugLayers ? &vkLayerSettingsCreateInfoEXT : nullptr,
		.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
		.pApplicationInfo = &vkApplicationInfo,
		.enabledLayerCount = kbVulkanDebugLayers ? static_cast<uint32_t>(std::ssize(mValidationLayers)) : 0,
		.ppEnabledLayerNames = kbVulkanDebugLayers ? mValidationLayers.data() : nullptr,
		.enabledExtensionCount = static_cast<uint32_t>(kbVulkanDebugLayers ? kiDebugExtensionCount : kiBaseExtensionCount),
		.ppEnabledExtensionNames = kppcInstanceExtensionNames,
	};

	TryLoadRenderDocDll();

	HMODULE hRenderDocModule = GetModuleHandle("renderdoc.dll");
	if (hRenderDocModule != nullptr)
	{
		LOG(kGraphics, kInfo, "renderDocHmodule: {}", reinterpret_cast<uint64_t>(hRenderDocModule));

		// RenderDoc doesn't ship VK_LAYER_KHRONOS_validation, so VK_EXT_layer_settings is unavailable. Keep first 4 entries of kppcInstanceExtensionNames (surface, win32 surface, portability enumeration, debug utils). pNext must be cleared because VkLayerSettingsCreateInfoEXT requires the layer settings extension we just dropped. VUID-VkInstanceCreateInfo-flags-06559 holds because VK_KHR_portability_enumeration is retained.
		vkInstanceCreateInfo.pNext = nullptr;
		vkInstanceCreateInfo.enabledLayerCount = 0;
		vkInstanceCreateInfo.ppEnabledLayerNames = nullptr;
		vkInstanceCreateInfo.enabledExtensionCount = 4;

		if constexpr (kbRenderDoc)
		{
			auto pGetRenderDocApi = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(hRenderDocModule, "RENDERDOC_GetAPI"));
			if (pGetRenderDocApi != nullptr)
			{
				pGetRenderDocApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&mpRenderDocApi));
				LOG(kGraphics, kInfo, "RenderDoc API initialized: {}", reinterpret_cast<uint64_t>(mpRenderDocApi));
			}

			if (mpRenderDocApi != nullptr)
			{
				// Captures land beside the screenshot default: %TEMP%\RenderDoc\agent_frameNNN.rdc. RenderDoc creates
				// missing directories; absolute paths are read back per capture via GetCapture().
				std::u8string captureTemplate = (std::filesystem::temp_directory_path() / "RenderDoc" / "agent").u8string();
				mpRenderDocApi->SetCaptureFilePathTemplate(reinterpret_cast<const char*>(captureTemplate.c_str()));
			}
		}
	}

	VkResult vkResultCreateInstance = vkCreateInstance(&vkInstanceCreateInfo, nullptr, &mVkInstance);

	if (vkResultCreateInstance != VK_SUCCESS)
	{
		// Clean-machine fallback: no Vulkan SDK => no validation layer. Drop pNext (layer settings), clear portability flag (its extension is also dropped), keep only surface + win32 surface.
		LOG(kGraphics, kWarning, "vkCreateInstance returned {}, re-trying with minimal configuration", vkResultCreateInstance);

		vkInstanceCreateInfo.pNext = nullptr;
		vkInstanceCreateInfo.flags = 0;
		vkInstanceCreateInfo.enabledLayerCount = 0;
		vkInstanceCreateInfo.ppEnabledLayerNames = nullptr;
		vkInstanceCreateInfo.enabledExtensionCount = 2;
		vkResultCreateInstance = vkCreateInstance(&vkInstanceCreateInfo, nullptr, &mVkInstance);
	}

	if (vkResultCreateInstance != VK_SUCCESS)
	{
		const char* pcResult = string_VkResult(vkResultCreateInstance);
		LOG(kDefault, kError, "vkCreateInstance failed with {}, Vulkan 1.2 is required", pcResult);
		std::string errorMessage = "Failed to create Vulkan instance.\n\nVulkan 1.2 or higher is required.\n\nError: ";
		errorMessage += pcResult;

		if ((gLaunchOptions.iAgentPort == 0))
		{
			MessageBox(nullptr, errorMessage.c_str(), game::kGameName.data(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
		}

		throw std::runtime_error("Vulkan 1.2 not available");
	}

	volkLoadInstance(mVkInstance);

	if constexpr (kbVulkanDebugLayers)
	{
		VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfoEXT
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugUtilsCallback,
		};

		if (vkCreateDebugUtilsMessengerEXT != nullptr)
		{
			CHECK_VK(vkCreateDebugUtilsMessengerEXT(mVkInstance, &vkDebugUtilsMessengerCreateInfoEXT, nullptr, &mVkDebugUtilsMessengerEXT));
		}
	}

	VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR
	{
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.hinstance = hInstance,
		.hwnd = hWindow,
	};
	CHECK_VK(vkCreateWin32SurfaceKHR(mVkInstance, &vkWin32SurfaceCreateInfoKHR, nullptr, &mVkSurfaceKHR));

	SelectBestPhysicalDevice();
	ValidatePhysicalDeviceCapabilities();
	SelectQueueFamilies();
	SelectSurfaceFormat();
	SelectDepthFormat();

	// Elevation, lighting, smoke, and wind prepasses enable MAX/ADD blending with kMax/kAdd.
	// Their attachment formats require VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT; absence makes pipeline creation undefined behavior.
	const VkFormat pVkBlendedRenderTargetFormats[] {shaders::kVkFormatElevation, shaders::kVkFormatLighting, shaders::kVkFormatSmoke, shaders::kVkFormatWind};
	for (const VkFormat& rVkFormat : pVkBlendedRenderTargetFormats)
	{
		if (!SupportsColorAttachmentBlend(rVkFormat))
		{
			LOG(kGraphics, kError, "Device does not advertise VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT for blended render-target format {}", string_VkFormat(rVkFormat));
			ASSERT(false);
		}
	}
}

void InstanceManager::SelectBestPhysicalDevice()
{
	uint32_t uiPhysicalDeviceCount = 0;
	LOG(kGraphics, kInfo, "\nEnumerate physical devices");
	CHECK_VK(vkEnumeratePhysicalDevices(mVkInstance, &uiPhysicalDeviceCount, nullptr));
	LOG(kGraphics, kInfo, "  uiPhysicalDeviceCount: {}", uiPhysicalDeviceCount);
	if (uiPhysicalDeviceCount == 0)
	{
		throw std::runtime_error("No physical devices found");
	}
	std::vector<VkPhysicalDevice> physicalDevices(uiPhysicalDeviceCount);
	VkResult vkResult = vkEnumeratePhysicalDevices(mVkInstance, &uiPhysicalDeviceCount, physicalDevices.data());
	if (vkResult != VK_SUCCESS && vkResult != VK_INCOMPLETE)
	{
		CHECK_VK(vkResult);
	}

	LOG(kGraphics, kInfo, "  Physical devices:");
	for (const VkPhysicalDevice& rVkPhysicalDevice : physicalDevices)
	{
		VkPhysicalDeviceProperties vkPhysicalDeviceProperties {};
		vkGetPhysicalDeviceProperties(rVkPhysicalDevice, &vkPhysicalDeviceProperties);
		LOG(kGraphics, kInfo, "    \"{}\"{}", vkPhysicalDeviceProperties.deviceName, vkPhysicalDeviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? " (Discrete) " : "");

		uint32_t uiDeviceApiVersion = vkPhysicalDeviceProperties.apiVersion;
		if (uiDeviceApiVersion < VK_API_VERSION_1_2)
		{
			LOG(kGraphics, kInfo, "      Skipping device: API version {}.{}.{} < required 1.2.0", VK_VERSION_MAJOR(uiDeviceApiVersion), VK_VERSION_MINOR(uiDeviceApiVersion), VK_VERSION_PATCH(uiDeviceApiVersion));
			continue;
		}

		bool bSupportsPresent = false;
		uint32_t uiPhysicalDeviceQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(rVkPhysicalDevice, &uiPhysicalDeviceQueueFamilyCount, nullptr);
		for (int64_t i = 0; i < uiPhysicalDeviceQueueFamilyCount; ++i)
		{
			VkBool32 vkSupportsPresent = VK_FALSE;
			CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(rVkPhysicalDevice, static_cast<uint32_t>(i), mVkSurfaceKHR, &vkSupportsPresent));
			bSupportsPresent |= vkSupportsPresent == VK_TRUE;
		}

		if (!bSupportsPresent)
		{
			continue;
		}

		if (mVkPhysicalDevice == VK_NULL_HANDLE || vkPhysicalDeviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			mVkPhysicalDevice = rVkPhysicalDevice;
		}

		if (vkPhysicalDeviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			break;
		}
	}
	ASSERT(mVkPhysicalDevice != VK_NULL_HANDLE);
}

void InstanceManager::ValidatePhysicalDeviceCapabilities()
{
	vkGetPhysicalDeviceProperties(mVkPhysicalDevice, &mVkPhysicalDeviceProperties);
	LOG(kGraphics, kInfo, "  Selected device API version: {}.{}.{}", VK_VERSION_MAJOR(mVkPhysicalDeviceProperties.apiVersion), VK_VERSION_MINOR(mVkPhysicalDeviceProperties.apiVersion), VK_VERSION_PATCH(mVkPhysicalDeviceProperties.apiVersion));
	LOG(kGraphics, kInfo, "  maxImageDimension2D: {}", mVkPhysicalDeviceProperties.limits.maxImageDimension2D);
	LOG(kGraphics, kInfo, "  maxImageDimensionCube: {}", mVkPhysicalDeviceProperties.limits.maxImageDimensionCube);
	LOG(kGraphics, kInfo, "  maxPerStageResources: {}", mVkPhysicalDeviceProperties.limits.maxPerStageResources);
	// Check capabilities only after device selection; weaker devices rejected during enumeration must not fail initialization.
	ASSERT(mVkPhysicalDeviceProperties.limits.maxPerStageResources > 200);
	ASSERT(mVkPhysicalDeviceProperties.limits.maxUniformBufferRange >= 65'536);
	vkGetPhysicalDeviceMemoryProperties(mVkPhysicalDevice, &mVkPhysicalDeviceMemoryProperties);

	vkGetPhysicalDeviceFeatures2(mVkPhysicalDevice, &mVkPhysicalDeviceFeatures2);

	struct RequiredFeature
	{
		const VkBool32* pVkFeature = nullptr;
		const char* pcName = nullptr;
		const char* pcReason = nullptr;
	};
	const RequiredFeature pRequiredFeatures[]
	{
		{.pVkFeature = &mVkPhysicalDeviceFeatures2.features.shaderStorageImageExtendedFormats, .pcName = "shaderStorageImageExtendedFormats", .pcReason = "R16_SFLOAT smoke storage images"},
		{.pVkFeature = &mVkPhysicalDeviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind, .pcName = "descriptorBindingStorageBufferUpdateAfterBind", .pcReason = "VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT"},
		{.pVkFeature = &mVkPhysicalDeviceVulkan12Features.shaderSampledImageArrayNonUniformIndexing, .pcName = "shaderSampledImageArrayNonUniformIndexing", .pcReason = "non-uniform descriptor indexing"},
		{.pVkFeature = &mVkPhysicalDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind, .pcName = "descriptorBindingSampledImageUpdateAfterBind", .pcReason = "texture streaming"},
		{.pVkFeature = &mVkPhysicalDeviceVulkan12Features.descriptorBindingPartiallyBound, .pcName = "descriptorBindingPartiallyBound", .pcReason = "bindless texture arrays"},
		{.pVkFeature = &mVkPhysicalDeviceVulkan12Features.runtimeDescriptorArray, .pcName = "runtimeDescriptorArray", .pcReason = "bindless texture arrays"},
	};
	for (const RequiredFeature& rRequiredFeature : pRequiredFeatures)
	{
		if (*rRequiredFeature.pVkFeature != VK_TRUE)
		{
			std::string errorMessage = "Required Vulkan feature not supported.\n\n";
			errorMessage += rRequiredFeature.pcName;
			errorMessage += " is required for ";
			errorMessage += rRequiredFeature.pcReason;
			errorMessage += ".";

			LOG(kGraphics, kError, "Required Vulkan feature {} not supported ({})", rRequiredFeature.pcName, rRequiredFeature.pcReason);

			if ((gLaunchOptions.iAgentPort == 0))
			{
				MessageBox(nullptr, errorMessage.c_str(), game::kGameName.data(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
			}

			std::string throwMessage = rRequiredFeature.pcName;
			throwMessage += " not supported";
			throw std::runtime_error(throwMessage);
		}
	}

	if (!SupportsStorageImage(shaders::kVkFormatSmoke))
	{
		LOG(kGraphics, kError, "Device does not advertise VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT for smoke format {}", string_VkFormat(shaders::kVkFormatSmoke));
		throw std::runtime_error("Smoke texture format does not support storage images");
	}

	ASSERT(mVkPhysicalDeviceFeatures2.features.sampleRateShading == VK_TRUE);
	ASSERT(mVkPhysicalDeviceFeatures2.features.samplerAnisotropy == VK_TRUE);
	if constexpr (kbShaderRealtimeClock)
	{
		ASSERT(mVkPhysicalDeviceShaderClockFeaturesKHR.shaderSubgroupClock == VK_TRUE);
		ASSERT(mVkPhysicalDeviceShaderClockFeaturesKHR.shaderDeviceClock == VK_TRUE);
		ASSERT(mVkPhysicalDeviceFeatures2.features.shaderInt64 == VK_TRUE);
	}
	ASSERT(mVkPhysicalDeviceFeatures2.features.textureCompressionBC == VK_TRUE);
	mMaxMultisampleCountVkSampleCountFlagBits = SelectSampleCount(mVkPhysicalDeviceProperties.limits.framebufferColorSampleCounts & mVkPhysicalDeviceProperties.limits.framebufferDepthSampleCounts);
	LOG(kGraphics, kInfo, "  Max multisample count: {}\n", static_cast<int64_t>(mMaxMultisampleCountVkSampleCountFlagBits));
	ASSERT(mMaxMultisampleCountVkSampleCountFlagBits > VK_SAMPLE_COUNT_1_BIT);
	VkSampleCountFlagBits vkSampleCount = SelectSupportedSampleCount(gSampleCount.Get<VkSampleCountFlagBits>());
	if (vkSampleCount != gSampleCount.Get<VkSampleCountFlagBits>())
	{
		gSampleCount.Reset<VkSampleCountFlagBits>(vkSampleCount);
	}
}

void InstanceManager::SelectQueueFamilies()
{
	uint32_t uiPhysicalDeviceQueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &uiPhysicalDeviceQueueFamilyCount, nullptr);
	ASSERT(uiPhysicalDeviceQueueFamilyCount != 0);
	mVkQueueFamilyProperties.resize(uiPhysicalDeviceQueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &uiPhysicalDeviceQueueFamilyCount, mVkQueueFamilyProperties.data());
	ASSERT(uiPhysicalDeviceQueueFamilyCount != 0);

	LOG(kGraphics, kInfo, "Physical device queues ({}):", uiPhysicalDeviceQueueFamilyCount);
	for (int64_t i = 0; i < uiPhysicalDeviceQueueFamilyCount; ++i)
	{
		VkBool32 vkSupportsPresent = VK_FALSE;
		CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(mVkPhysicalDevice, static_cast<uint32_t>(i), mVkSurfaceKHR, &vkSupportsPresent));
		LOG(kGraphics, kInfo, "  {} | {} | {} | {}", (mVkQueueFamilyProperties.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 ? "VK_QUEUE_GRAPHICS_BIT" : "                     ", vkSupportsPresent == VK_TRUE ? "Supports present" : "                ", (mVkQueueFamilyProperties.at(i).queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 ? "VK_QUEUE_COMPUTE_BIT" : "                    ", (mVkQueueFamilyProperties.at(i).queueFlags & VK_QUEUE_TRANSFER_BIT) != 0 ? "VK_QUEUE_TRANSFER_BIT" : "                     ");
	}
	LOG(kGraphics, kInfo, "");

	miGraphicsQueueFamilyIndex = UINT32_MAX;
	miPresentQueueFamilyIndex = UINT32_MAX;
	miTransferQueueFamilyIndex = UINT32_MAX;
	for (int64_t i = 0; i < uiPhysicalDeviceQueueFamilyCount; ++i)
	{
		if ((mVkQueueFamilyProperties.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			// Search for a graphics queue in the array of queue families, prefer one that supports both
			VkBool32 vkSupportsPresent = VK_FALSE;
			CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(mVkPhysicalDevice, static_cast<uint32_t>(i), mVkSurfaceKHR, &vkSupportsPresent));
			if (vkSupportsPresent == VK_TRUE)
			{
				miGraphicsQueueFamilyIndex = i;
				miPresentQueueFamilyIndex = i;
			}

			if (miGraphicsQueueFamilyIndex == UINT32_MAX)
			{
				miGraphicsQueueFamilyIndex = i;
			}
		}

		if ((mVkQueueFamilyProperties.at(i).queueFlags & VK_QUEUE_TRANSFER_BIT) != 0)
		{
			// Look for a queue that supports only transfer, this will be the fastest for concurrent uploads (won't stall the other queues)
			if ((mVkQueueFamilyProperties.at(i).queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0)
			{
				miTransferQueueFamilyIndex = i;
			}

			if (miTransferQueueFamilyIndex == UINT32_MAX)
			{
				miTransferQueueFamilyIndex = i;
			}
		}
	}

	if (miPresentQueueFamilyIndex == UINT32_MAX)
	{
		for (int64_t i = 0; i < uiPhysicalDeviceQueueFamilyCount; ++i)
		{
			VkBool32 vkSupportsPresent = VK_FALSE;
			CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(mVkPhysicalDevice, static_cast<uint32_t>(i), mVkSurfaceKHR, &vkSupportsPresent));
			if (vkSupportsPresent == VK_TRUE)
			{
				miPresentQueueFamilyIndex = i;
				break;
			}
		}
	}

	if (miTransferQueueFamilyIndex == miPresentQueueFamilyIndex && miPresentQueueFamilyIndex != miGraphicsQueueFamilyIndex)
	{
		miTransferQueueFamilyIndex = miGraphicsQueueFamilyIndex;
	}

	// Vulkan spec allows graphics/compute families to support transfer without advertising VK_QUEUE_TRANSFER_BIT, so the
	// transfer index can legally stay UINT32_MAX. Fall back to the graphics family (always transfer-capable) so the .at()
	// below cannot throw — trust boundary on the queue-family enumeration result.
	if (miTransferQueueFamilyIndex == UINT32_MAX)
	{
		miTransferQueueFamilyIndex = miGraphicsQueueFamilyIndex;
	}

	ASSERT(miGraphicsQueueFamilyIndex != UINT32_MAX && miPresentQueueFamilyIndex != UINT32_MAX && miTransferQueueFamilyIndex != UINT32_MAX);
	LOG(kGraphics, kInfo, "Selected queue families: graphics {}, present {}, transfer {}", miGraphicsQueueFamilyIndex, miPresentQueueFamilyIndex, miTransferQueueFamilyIndex);

	mTransferImageGranularityVkExtent3D = mVkQueueFamilyProperties.at(miTransferQueueFamilyIndex).minImageTransferGranularity;
	LOG(kGraphics, kDebug, "Transfer queue minImageTransferGranularity: ({}, {}, {})", mTransferImageGranularityVkExtent3D.width, mTransferImageGranularityVkExtent3D.height, mTransferImageGranularityVkExtent3D.depth);
}

void InstanceManager::SelectSurfaceFormat()
{
	uint32_t uiFormatCount = 0;
	CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysicalDevice, mVkSurfaceKHR, &uiFormatCount, nullptr));
	ASSERT(uiFormatCount != 0);
	std::vector<VkSurfaceFormatKHR> physicalDeviceSurfaceFormats(uiFormatCount);
	CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysicalDevice, mVkSurfaceKHR, &uiFormatCount, physicalDeviceSurfaceFormats.data()));

	LOG(kGraphics, kInfo, "Surface formats ({}):", std::ssize(physicalDeviceSurfaceFormats));
	for ([[maybe_unused]] const VkSurfaceFormatKHR& rVkSurfaceFormatKHR : physicalDeviceSurfaceFormats)
	{
		LOG(kGraphics, kInfo, "  {} ({})", string_VkFormat(rVkSurfaceFormatKHR.format), string_VkColorSpaceKHR(rVkSurfaceFormatKHR.colorSpace));
	}
	LOG(kGraphics, kInfo, "");

	// If the format list includes just one entry of VK_FORMAT_UNDEFINED, the surface has no preferred format
	if (uiFormatCount == 1 && physicalDeviceSurfaceFormats.at(0).format == VK_FORMAT_UNDEFINED)
	{
		mFramebufferVkFormat = VK_FORMAT_B8G8R8A8_UNORM;
		LOG(kGraphics, kInfo, "Selected framebuffer format: {} with color space: {} (no preferred format)\n", string_VkFormat(mFramebufferVkFormat), string_VkColorSpaceKHR(mFramebufferVkColorSpaceKHR));
	}
	else
	{
		bool bFoundPreferredFormat = false;
		for (const VkSurfaceFormatKHR& rVkSurfaceFormatKHR : physicalDeviceSurfaceFormats)
		{
			if (rVkSurfaceFormatKHR.format == VK_FORMAT_B8G8R8A8_UNORM || rVkSurfaceFormatKHR.format == VK_FORMAT_R8G8B8A8_UNORM)
			{
				mFramebufferVkFormat = rVkSurfaceFormatKHR.format;
				mFramebufferVkColorSpaceKHR = rVkSurfaceFormatKHR.colorSpace;
				bFoundPreferredFormat = true;
				LOG(kGraphics, kInfo, "Selected framebuffer format: {} with color space: {}\n", string_VkFormat(mFramebufferVkFormat), string_VkColorSpaceKHR(mFramebufferVkColorSpaceKHR));
				break;
			}
		}

		if (!bFoundPreferredFormat)
		{
			mFramebufferVkFormat = physicalDeviceSurfaceFormats.at(0).format;
			mFramebufferVkColorSpaceKHR = physicalDeviceSurfaceFormats.at(0).colorSpace;
			LOG(kGraphics, kInfo, "Using fallback surface format: {} with color space: {} (preferred formats not available)\n", string_VkFormat(mFramebufferVkFormat), string_VkColorSpaceKHR(mFramebufferVkColorSpaceKHR));
		}
	}
}

void InstanceManager::SelectDepthFormat()
{
	// Prefer high precision depth formats
	VkFormat pVkFormats[] {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT};

	for (const VkFormat& rVkFormat : pVkFormats)
	{
		VkFormatProperties vkFormatProperties {};
		vkGetPhysicalDeviceFormatProperties(mVkPhysicalDevice, rVkFormat, &vkFormatProperties);

		if ((vkFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			LOG(kGraphics, kInfo, "Depth format selected: {} (optimal)\n", string_VkFormat(rVkFormat));
			mDepthVkFormat = rVkFormat;
			break;
		}
	}

	if (mDepthVkFormat == VK_FORMAT_UNDEFINED)
	{
		for (const VkFormat& rVkFormat : pVkFormats)
		{
			VkFormatProperties vkFormatProperties {};
			vkGetPhysicalDeviceFormatProperties(mVkPhysicalDevice, rVkFormat, &vkFormatProperties);

			if ((vkFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
			{
				LOG(kGraphics, kInfo, "Depth format selected: {} (linear)\n", string_VkFormat(rVkFormat));
				mDepthVkFormat = rVkFormat;
				break;
			}
		}
	}

	if (mDepthVkFormat == VK_FORMAT_UNDEFINED)
	{
		throw std::runtime_error("Unable to find VkFormat for depth buffer");
	}
}

InstanceManager::~InstanceManager()
{
	vkDestroySurfaceKHR(mVkInstance, mVkSurfaceKHR, nullptr);

	if constexpr (kbVulkanDebugLayers)
	{
		if (mVkDebugUtilsMessengerEXT != nullptr)
		{
			vkDestroyDebugUtilsMessengerEXT(mVkInstance, mVkDebugUtilsMessengerEXT, nullptr);
		}
	}

	vkDestroyInstance(mVkInstance, nullptr);

	if (gpInstanceManager == this)
	{
		gpInstanceManager = nullptr;
	}
}

void InstanceManager::ReadLayerProperties()
{
	uint32_t uiLayerCount = 0;
	CHECK_VK(vkEnumerateInstanceLayerProperties(&uiLayerCount, nullptr));

	LOG(kGraphics, kInfo, "\nFound {} Vulkan validation layers:", uiLayerCount);

	if (uiLayerCount == 0)
	{
		return;
	}

	std::vector<VkLayerProperties> instanceLayerProperties(uiLayerCount);
	CHECK_VK(vkEnumerateInstanceLayerProperties(&uiLayerCount, instanceLayerProperties.data()));

	for (const VkLayerProperties& rVkLayerProperties : instanceLayerProperties)
	{
		LOG(kGraphics, kInfo, "  {} {}.{}", rVkLayerProperties.layerName, VK_VERSION_PATCH(rVkLayerProperties.specVersion), rVkLayerProperties.implementationVersion);

		for (const char* pcValidationLayer : kppcValidationLayers)
		{
			if (std::strcmp(rVkLayerProperties.layerName, pcValidationLayer) == 0)
			{
				mValidationLayers.push_back(pcValidationLayer);
			}
		}

		uint32_t uiExtensionPropertiesCount = 0;
		CHECK_VK(vkEnumerateInstanceExtensionProperties(rVkLayerProperties.layerName, &uiExtensionPropertiesCount, nullptr));

		if (uiExtensionPropertiesCount == 0)
		{
			continue;
		}

		std::vector<VkExtensionProperties> extensionProperties(uiExtensionPropertiesCount);
		CHECK_VK(vkEnumerateInstanceExtensionProperties(rVkLayerProperties.layerName, &uiExtensionPropertiesCount, extensionProperties.data()));
		for (const VkExtensionProperties& rVkExtensionProperties : extensionProperties)
		{
			LOG(kGraphics, kInfo, "    Extension: {} {}", rVkExtensionProperties.extensionName, VK_VERSION_PATCH(rVkExtensionProperties.specVersion));
		}
	}

	LOG(kGraphics, kInfo, "");
	// NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores) — pcLayer is read only by the LOG below, which compiles out below the kGraphics threshold
	for (const char* pcLayer : mValidationLayers)
	{
		LOG(kGraphics, kInfo, "Found \"{}\"", pcLayer);
	}
	LOG(kGraphics, kInfo, "");
}

} // namespace engine

#endif // defined(BT_CLIENT)
