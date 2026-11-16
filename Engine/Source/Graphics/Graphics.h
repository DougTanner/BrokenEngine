#pragma once

#if defined(BT_CLIENT)

namespace game
{

struct FrameInterpolate;

} // namespace game

namespace engine
{

struct CellStaticData;

class BufferManager;
class CommandBufferManager;
class DeviceManager;
class ImGuiManager;
class InstanceManager;
class Islands;
class ParticleManager;
class PipelineManager;
class SwapchainManager;
class TextureManager;
class Wrapper;

enum class DestroyType
{
	kNone,
	kCommandBuffers,
	kSamplers,
	kPipelines,
	kSwapchain,
	kSurface,
};

enum class DestroyFlags : uint16_t
{
	kShadowTextures     = 0x0004,
	kObjectShadows      = 0x0008,
	kLightingTextures   = 0x0010,
	kWaterMesh          = 0x0020,
	kTerrainElevation   = 0x0040,
	kLightingBlur       = 0x0080,
	kSmokeTextures      = 0x0400,
};
using DestroyFlags_t = common::Flags<DestroyFlags>;

enum class GraphicsFlags : uint8_t
{
	kDrawModelsOverWorld = 1 << 0,
	// The latest Create completed a requested recreation; cleared on every Create entry.
	kRecreated = 1 << 1,
};
using GraphicsFlags_t = common::Flags<GraphicsFlags>;

inline VkExtent2D gVkWantedFramebufferExtent2D {};

std::tuple<int64_t, int64_t> FullDetail();
std::tuple<int64_t, int64_t> WaterFullDetail();
float SmokeSimulationPixels();
float SmokeSimulationPixelsY();

// Agent/dev screenshot request queued into Graphics::mOptionalScreenshotRequest; consumed once at the capture site in
// PresentAcquire. Empty path -> default %TEMP%\Screenshots\agent_{N}.{jpg|png}. iMaxWidth <= 0 disables
// downscale; the image is downscaled preserving aspect when wider than iMaxWidth.
struct ScreenshotRequest
{
	std::filesystem::path path;
	int64_t iMaxWidth = 1'568;
	bool bPng = false;
	int64_t iQuality = 80;
	// Capture token minted and made active by ResetCaptureResult(); the async save publishes only while it remains
	// active, so an overlapping abandoned encoder cannot replace a newer request's result. 0 for F9 dev saves, whose
	// result no caller consumes.
	int64_t iCaptureToken = 0;
};

// Agent render-target dump request queued into Graphics::mOptionalDumpRenderTargetRequest; consumed at the same capture
// site. name selects a RenderTargetTextures member (validated at request time); iIndex/iChannel index the array
// members. Empty path -> default %TEMP%\Screenshots\dump_{name}_{N}. bRaw additionally writes the raw texel .bin.
struct DumpRenderTargetRequest
{
	std::string name;
	int64_t iIndex = 0;
	int64_t iChannel = 0;
	std::filesystem::path path;
	bool bRaw = false;
	// As ScreenshotRequest::iCaptureToken — publication succeeds only while this request remains active, so an
	// overlapping abandoned encoder cannot replace a newer request's result. 0 for non-agent dumps.
	int64_t iCaptureToken = 0;
};

class Graphics : public common::Singleton<Graphics>
{
public:

	Graphics(HINSTANCE hinstance, HWND hwnd);
	~Graphics();

	Graphics() = delete;

	void RenderGlobal(std::span<const CellStaticData* const> activeIslandCells, std::chrono::duration<float> currentTime);
	void RenderMainPresentAcquire(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords, GridCoord cameraCoord, std::chrono::duration<float> currentTime);
	// A world-hidden frame: waits only this framebuffer's fence, writes the active islands and runs the residency step as
	// RenderGlobal does, and submits only the acquire command buffer, when recorded, and the UI, recording no Global or Main work.
	void RenderUiPresentAcquire(std::span<const CellStaticData* const> activeIslandCells, int64_t iCommandBuffer);
	void Create();
	void Refresh();
	void Destroy();
	void RecreateResources();
	// Matching extents require both no deferred recreation and no pending swapchain-tier teardown to be settled.
	// Refresh copies the wanted extent before recreation can defer, and a failed acquire/present can leave a teardown pending.
	bool ExtentSettled() const;

	HINSTANCE mHinstance = nullptr;
	HWND mHwnd = nullptr;
	int64_t miMonitorRefreshRate = 60;
	int64_t miFrameCounter = 0;
	VkExtent2D mFramebufferVkExtent2D = gVkWantedFramebufferExtent2D;
	GraphicsFlags_t mFlags = kbDrawModelsOverWorld ? GraphicsFlags_t {GraphicsFlags::kDrawModelsOverWorld} : GraphicsFlags_t {};
	// Runtime host policy like mFlags, initialized from game::kVkHdrClearColor.
	VkClearColorValue mHdrVkClearColorValue;

	DestroyType meDestroyType = DestroyType::kNone;
	DestroyFlags_t mDestroyFlags;
	// Latch so a swapchain recreate deferred while the window is off-screen/minimized logs once per transition, not per frame.
	bool mbSwapchainRecreateDeferred = false;

	std::unique_ptr<InstanceManager> mpInstanceManager;
	std::unique_ptr<DeviceManager> mpDeviceManager;
	std::unique_ptr<SwapchainManager> mpSwapchainManager;
	std::unique_ptr<CommandBufferManager> mpCommandBufferManager;
	std::unique_ptr<BufferManager> mpBufferManager;
	std::unique_ptr<TextureManager> mpTextureManager;
	std::unique_ptr<Islands> mpIslands;
	std::unique_ptr<PipelineManager> mpPipelineManager;
	std::unique_ptr<ParticleManager> mpParticleManager;

	std::unique_ptr<ImGuiManager> mpImGuiManager;

	common::Timer mRenderFrameTimer;
	std::chrono::nanoseconds mRenderFrameDeltaNanoseconds = 0ns;

	common::InTheLastSecond mRendersInTheLastSecond;

	// One-shot capture mailboxes filled off the render path (agent Drain / F9 toggle), consumed once at the
	// PresentAcquire capture site (between the UI submit and Present — see the deadlock note there).
	std::optional<ScreenshotRequest> mOptionalScreenshotRequest;
	std::optional<DumpRenderTargetRequest> mOptionalDumpRenderTargetRequest;

private:

	// Queries surface caps into rVkSurfaceCapabilitiesKHR and returns true when the swapchain recreate must defer: a
	// zero-area defined currentExtent (window off-screen/minimized).
	bool SurfaceExtentZeroArea(VkSurfaceCapabilitiesKHR& rVkSurfaceCapabilitiesKHR);

	void WaitFramebuffer(int64_t iCommandBuffer);
	// Every presented frame after WaitFramebuffer: descriptor churn, publication, retired-resource destruction, and the
	// miFrameCounter advance.
	void UpdateFrameResidency(int64_t iCommandBuffer);
	void PresentAcquire(int64_t iCommandBuffer);

	template <typename T>
	void PollSetting(Wrapper& rWrapper, const char* pcLabel, DestroyType eTier, std::optional<DestroyFlags> optionalFlag = std::nullopt, bool bGate = true);
};

inline Graphics* gpGraphics = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
