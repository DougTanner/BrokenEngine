#include "DebugRender.h"

#if defined(BT_CLIENT)

#include "Graphics/Objects/PipelineDescriptorWriter.h"

namespace engine
{

constexpr int64_t kiInitialDebugRender = 128 * 1'024;

struct DebugRenderType
{
	common::crc_t crc = 0;
	Pipelines ePipeline = kPipelineCount;
	int64_t iCount = 0;
	// Each sTypes entry relies on this default member initializer for its reservation limit.
	common::StableVector<shaders::DebugRenderLayout> layouts = common::StableVector<shaders::DebugRenderLayout>(64 * kiInitialDebugRender);
};

static DebugRenderType sTypes[]
{
	{.crc = common::CrcConsteval("DebugBox"), .ePipeline = kPipelineDebugBox, .iCount = 0},
	{.crc = common::CrcConsteval("DebugSphere"), .ePipeline = kPipelineDebugSphere, .iCount = 0},
	{.crc = common::CrcConsteval("DebugCircle"), .ePipeline = kPipelineDebugCircle, .iCount = 0},
	{.crc = common::CrcConsteval("DebugLine"), .ePipeline = kPipelineDebugLine, .iCount = 0},
};

constexpr int64_t kiBox = 0;
constexpr int64_t kiSphere = 1;
constexpr int64_t kiCircle = 2;
constexpr int64_t kiLine = 3;

static void AddLayout(int64_t iType, const XMFLOAT4A& rf4Row0, const XMFLOAT4A& rf4Row1, const XMFLOAT4A& rf4Row2, const XMFLOAT4A& rf4Color)
{
	if constexpr (kbDebugRender)
	{
		DebugRenderType& rType = sTypes[iType];

		if (rType.layouts.Size() == 0)
		{
			rType.layouts.Resize(kiInitialDebugRender);
		}
		else if (rType.iCount >= rType.layouts.Size())
		{
			LOG(kDefault, kVerbose, "DebugRender: growing staging for type {} ({} -> {})", iType, rType.layouts.Size(), rType.layouts.Size() * 2);
			rType.layouts.Resize(rType.layouts.Size() * 2);
		}

		shaders::DebugRenderLayout& rLayout = rType.layouts[rType.iCount];
		rLayout.f3x4Transform[0] = rf4Row0;
		rLayout.f3x4Transform[1] = rf4Row1;
		rLayout.f3x4Transform[2] = rf4Row2;
		rLayout.f4Color = rf4Color;
		++rType.iCount;
	}
}

void DebugRender::Box(const XMFLOAT3A& rf3Position, const XMFLOAT3A& rf3Scale, const XMFLOAT4A& rf4Color)
{
	if constexpr (kbDebugRender)
	{
		if (!msbEnabled)
		{
			return;
		}
		// Row-major 3x4: scale on diagonal, translation in w
		AddLayout(kiBox, {rf3Scale.x, 0.0f, 0.0f, rf3Position.x}, {0.0f, rf3Scale.y, 0.0f, rf3Position.y}, {0.0f, 0.0f, rf3Scale.z, rf3Position.z}, rf4Color);
	}
}

void DebugRender::Sphere(const XMFLOAT3A& rf3Center, float fRadius, const XMFLOAT4A& rf4Color)
{
	if constexpr (kbDebugRender)
	{
		if (!msbEnabled)
		{
			return;
		}
		AddLayout(kiSphere, {fRadius, 0.0f, 0.0f, rf3Center.x}, {0.0f, fRadius, 0.0f, rf3Center.y}, {0.0f, 0.0f, fRadius, rf3Center.z}, rf4Color);
	}
}

void DebugRender::Circle(const XMFLOAT3A& rf3Center, float fRadius, const XMFLOAT4A& rf4Color)
{
	if constexpr (kbDebugRender)
	{
		if (!msbEnabled)
		{
			return;
		}
		AddLayout(kiCircle, {fRadius, 0.0f, 0.0f, rf3Center.x}, {0.0f, fRadius, 0.0f, rf3Center.y}, {0.0f, 0.0f, fRadius, rf3Center.z}, rf4Color);
	}
}

void DebugRender::Line(const XMFLOAT3A& rf3Start, const XMFLOAT3A& rf3End, const XMFLOAT4A& rf4Color)
{
	if constexpr (kbDebugRender)
	{
		if (!msbEnabled)
		{
			return;
		}
		// Line mesh is (0,0,0) to (1,0,0) along +X
		// Transform: X axis = direction, translation = start
		float fDeltaX = rf3End.x - rf3Start.x;
		float fDeltaY = rf3End.y - rf3Start.y;
		float fDeltaZ = rf3End.z - rf3Start.z;

		AddLayout(kiLine, {fDeltaX, 0.0f, 0.0f, rf3Start.x}, {fDeltaY, 0.0f, 0.0f, rf3Start.y}, {fDeltaZ, 0.0f, 0.0f, rf3Start.z}, rf4Color);
	}
}


void DebugRender::BeginRender(int64_t iCommandBuffer)
{
	if constexpr (kbDebugRender)
	{
		for (const DebugRenderType& rType : sTypes)
		{
			if (rType.iCount == 0)
			{
				continue;
			}

			if (Buffer* pBuffer = gpBufferManager->ResizeDynamicBufferIfNeeded(rType.crc, kBufferMain, "DebugRender", sizeof(shaders::DebugRenderLayout), rType.iCount, iCommandBuffer); pBuffer != nullptr)
			{
				PipelineDescriptorWriter::UpdateStorageBuffer(gpPipelineManager->mpPipelines[rType.ePipeline], iCommandBuffer, 2, pBuffer);
			}

			[[maybe_unused]] auto [pLayouts, iBufferCapacity] = gpBufferManager->GetDynamicStorageBuffer<shaders::DebugRenderLayout>(rType.crc, kBufferMain, iCommandBuffer);
			std::memcpy(pLayouts, rType.layouts.Data(), rType.iCount * sizeof(shaders::DebugRenderLayout));
		}
	}
}

void DebugRender::EndRender(int64_t iCommandBuffer)
{
	if constexpr (kbDebugRender)
	{
		for (DebugRenderType& rType : sTypes)
		{
			gpPipelineManager->mpPipelines[rType.ePipeline].WriteIndirectBuffer(iCommandBuffer, rType.iCount);
			rType.iCount = 0;
		}
	}
}

} // namespace engine

#endif // BT_CLIENT
