#pragma once

#include "Profile/ProfileManagerBase.h"

#include "Frame/AreaDamage.h"
#include "Frame/Collision.h"
#include "Frame/FrameRegistry.h"
#include "Frame/TimeStep.h"

#include "File/FileManager.h"
#include "File/DifferenceStream.h"

#include "Network/NetworkManager.h"
#include "Network/NetworkProtocol.h"
#include "Network/NetworkMessages.h"
#include "Network/NetworkSessionContract.h"
#include "Network/NetworkSerialization.h"
#include "Network/NetworkSimulation.h"

#include "LaunchOptions.h"
#include "Agent/AgentCommandServer.h"
#include "Agent/AgentCommandsShared.h"

#if defined(BT_CLIENT)

#include "Graphics/Objects/Buffer.h"
#include "Graphics/Objects/CommandBuffers.h"
#include "Graphics/Objects/Pipeline.h"
#include "Graphics/Objects/ModelPipeline.h"
#include "Graphics/Objects/Shader.h"
#include "Graphics/Objects/Texture.h"

#include "Graphics/Graphics.h"
#include "Graphics/GraphicsUtils.h"
#include "Graphics/Managers/BufferManager.h"
#include "Graphics/Managers/CommandBufferManager.h"
#include "Graphics/Managers/DeviceManager.h"
#include "Graphics/Managers/ImGuiManager.h"
#include "Graphics/Managers/InstanceManager.h"
#include "Graphics/Managers/ParticleManager.h"
#include "Graphics/Managers/PipelineManager.h"
#include "Graphics/Managers/SwapchainManager.h"
#include "Graphics/Managers/TextureManager.h"
#include "Graphics/Managers/TextureUploadManager.h"

#include "Graphics/AnimationData.h"
#include "Graphics/EngineCamera.h"
#include "Graphics/OneShotCommandBuffer.h"
#include "Graphics/Screenshot.h"

#include "Graphics/Render/Render.h"

#include "Graphics/Debug/DebugRender.h"

// Audio (StaticVoice/StreamingVoice headers deliberately not aggregated)
#include "Audio/AudioUtility.h"
#include "Audio/AudioManager.h"

#include "Input/Input.h"
#include "Input/RawInputManager.h"

// Agent client-only synthetic input + UI widget registry (self-guarded; grouped here in the BT_CLIENT span)
#include "Agent/AgentUiRegistry.h"
#include "Agent/AgentInput.h"

#include "Network/NetworkDiscoveryScanner.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"

#include "Graphics/Islands.h"

#endif // BT_CLIENT

#include "Frame/IslandTerrain.h"

#if defined(BT_SERVER)
#include "Network/NetworkDiscoveryResponder.h"
#include "Network/Server/OwnedEntityRegistry.h"
#include "Network/Server/Server.h"
#include "Network/Server/ServerSessionRuntime.h"
#endif

#include "GameBase.h"

#if defined(BT_CLIENT)
// Generic client agent commands: declared after GameBase.h because the interface names its UiState and GameFlags types.
#include "Agent/AgentCommandsClientGeneric.h"
#endif // BT_CLIENT

// Formatters for engine types used by LogDifference.
// GlobalId's formatter stays with its type in Frame/Collections/CollectionId.h.
template<>
struct std::formatter<engine::AlignmentIdentifier> : std::formatter<uint32_t>
{
	template<typename CONTEXT>
	auto format(const engine::AlignmentIdentifier alignment, CONTEXT& rContext) const
	{
		return std::formatter<uint32_t>::format(alignment.uiValue, rContext);
	}
};

template<>
struct std::formatter<engine::Alignments> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const engine::Alignments& rAlignments, CONTEXT& rContext) const
	{
		char pcBuffer[48] {};
		char* pWrite = pcBuffer;
		for (char cCharacter : std::string_view("Alignments("))
		{
			*(pWrite++) = cCharacter;
		}
		pWrite = std::to_chars(pWrite, pcBuffer + sizeof(pcBuffer), std::ssize(rAlignments.alignmentPairs)).ptr;
		*(pWrite++) = ')';
		return std::formatter<std::string_view>::format(std::string_view(pcBuffer, pWrite - pcBuffer), rContext);
	}
};

template<>
struct std::formatter<engine::Uuid> : std::formatter<int64_t>
{
	template<typename CONTEXT>
	auto format(const engine::Uuid identifier, CONTEXT& rContext) const
	{
		return std::formatter<int64_t>::format(identifier.iValue, rContext);
	}
};

template<typename T>
struct std::formatter<engine::Id<T>> : std::formatter<int64_t>
{
	template<typename CONTEXT>
	auto format(const engine::Id<T> identifier, CONTEXT& rContext) const
	{
		return std::formatter<int64_t>::format(identifier.uuid.iValue, rContext);
	}
};
