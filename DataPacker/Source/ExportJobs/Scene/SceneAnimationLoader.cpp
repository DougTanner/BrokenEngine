#include "SceneAnimationLoader.h"

namespace
{

bool IsValidIndex(int iIndex, size_t uiSize)
{
	return iIndex >= 0 && static_cast<size_t>(iIndex) < uiSize;
}

size_t CheckedAnimationSize(size_t uiCount, size_t uiElementSize, const std::string& rContext)
{
	if (uiElementSize != 0 && uiCount > std::numeric_limits<size_t>::max() / uiElementSize)
	{
		throw std::runtime_error(std::format("{} has a byte size that overflows size_t.", rContext));
	}
	return uiCount * uiElementSize;
}

const float* AccessorFloats(const tinygltf::Model& rModel, int iAccessor, size_t uiRequiredBytes, const std::string& rContext)
{
	if (!IsValidIndex(iAccessor, rModel.accessors.size()))
	{
		throw std::runtime_error(std::format("{} references accessor {} out of range (accessor count {}).", rContext, iAccessor, rModel.accessors.size()));
	}

	const tinygltf::Accessor& rAccessor = rModel.accessors.at(static_cast<size_t>(iAccessor));
	if (rAccessor.sparse.isSparse)
	{
		throw std::runtime_error(std::format("{} has sparse accessor {}.", rContext, iAccessor));
	}
	if (rAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		throw std::runtime_error(std::format("{} accessor {} does not use FLOAT components.", rContext, iAccessor));
	}
	if (rAccessor.count == 0)
	{
		throw std::runtime_error(std::format("{} accessor {} has zero elements.", rContext, iAccessor));
	}
	if (!IsValidIndex(rAccessor.bufferView, rModel.bufferViews.size()))
	{
		throw std::runtime_error(std::format("{} accessor {} references buffer view {} out of range (buffer view count {}).", rContext, iAccessor, rAccessor.bufferView, rModel.bufferViews.size()));
	}

	const tinygltf::BufferView& rBufferView = rModel.bufferViews.at(static_cast<size_t>(rAccessor.bufferView));
	if (!IsValidIndex(rBufferView.buffer, rModel.buffers.size()))
	{
		throw std::runtime_error(std::format("{} accessor {} buffer view {} references buffer {} out of range (buffer count {}).", rContext, iAccessor, rAccessor.bufferView, rBufferView.buffer, rModel.buffers.size()));
	}
	if (rBufferView.byteStride != 0)
	{
		throw std::runtime_error(std::format("{} accessor {} buffer view {} has unsupported animation byte stride {}.", rContext, iAccessor, rAccessor.bufferView, rBufferView.byteStride));
	}

	int32_t iComponentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(rAccessor.componentType));
	int32_t iComponentCount = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(rAccessor.type));
	if (iComponentSize <= 0)
	{
		throw std::runtime_error(std::format("{} accessor {} has an invalid component or element type.", rContext, iAccessor));
	}
	if (iComponentCount <= 0)
	{
		throw std::runtime_error(std::format("{} accessor {} has an invalid component or element type.", rContext, iAccessor));
	}

	const tinygltf::Buffer& rBuffer = rModel.buffers.at(static_cast<size_t>(rBufferView.buffer));
	if (rBufferView.byteOffset > rBuffer.data.size() || rBufferView.byteLength > rBuffer.data.size() - rBufferView.byteOffset)
	{
		throw std::runtime_error(std::format("{} accessor {} buffer view {} lies outside buffer {}.", rContext, iAccessor, rAccessor.bufferView, rBufferView.buffer));
	}

	size_t uiElementSize = CheckedAnimationSize(static_cast<size_t>(iComponentSize), static_cast<size_t>(iComponentCount), rContext);
	size_t uiAccessorBytes = CheckedAnimationSize(rAccessor.count, uiElementSize, rContext);
	if (rAccessor.byteOffset > rBufferView.byteLength || uiAccessorBytes > rBufferView.byteLength - rAccessor.byteOffset)
	{
		throw std::runtime_error(std::format("{} accessor {} lies outside buffer view {}.", rContext, iAccessor, rAccessor.bufferView));
	}
	if (uiRequiredBytes > uiAccessorBytes)
	{
		throw std::runtime_error(std::format("{} requires {} bytes but accessor {} declares only {} bytes.", rContext, uiRequiredBytes, iAccessor, uiAccessorBytes));
	}

	size_t uiBufferOffset = rBufferView.byteOffset + rAccessor.byteOffset;
	if (rAccessor.byteOffset % alignof(float) != 0)
	{
		throw std::runtime_error(std::format("{} accessor {} float data is misaligned.", rContext, iAccessor));
	}
	if (uiBufferOffset % alignof(float) != 0)
	{
		throw std::runtime_error(std::format("{} accessor {} float data is misaligned.", rContext, iAccessor));
	}
	if (uiRequiredBytes > rBuffer.data.size() - uiBufferOffset)
	{
		throw std::runtime_error(std::format("{} accessor {} read lies outside buffer {}.", rContext, iAccessor, rBufferView.buffer));
	}

	return reinterpret_cast<const float*>(rBuffer.data.data() + uiBufferOffset);
}

bool KeepAnimationChannel(const tinygltf::Model& rModel, const tinygltf::AnimationChannel& rGltfChannel)
{
	// Skip channels targeting an invalid / out-of-range node
	if (rGltfChannel.target_node < 0 || rGltfChannel.target_node >= static_cast<int>(rModel.nodes.size()))
	{
		LOG(kDefault, kVerbose, "  FILTERED: channel targeting node {} out of range (node count {})", rGltfChannel.target_node, rModel.nodes.size());
		return false;
	}
	LOG(kDefault, kVerbose, "  KEPT: channel targeting node {} (\"{}\") -> node index {}", rGltfChannel.target_node, rModel.nodes.at(rGltfChannel.target_node).name, rGltfChannel.target_node);
	return true;
}

bool MapAnimationChannel(const tinygltf::AnimationChannel& rGltfChannel, const tinygltf::AnimationSampler& rSampler, common::AnimationChannel& rChannel)
{
	rChannel.uiNodeIndex = static_cast<uint16_t>(rGltfChannel.target_node);

	// Target path
	if (rGltfChannel.target_path == "translation")
	{
		rChannel.uiTargetPath = common::AnimationChannel::kTargetPathTranslation;
	}
	else if (rGltfChannel.target_path == "rotation")
	{
		rChannel.uiTargetPath = common::AnimationChannel::kTargetPathRotation;
	}
	else if (rGltfChannel.target_path == "scale")
	{
		rChannel.uiTargetPath = common::AnimationChannel::kTargetPathScale;
	}
	else
	{
		return false; // Skip unknown paths
	}

	// Interpolation
	if (rSampler.interpolation == "STEP")
	{
		rChannel.uiInterpolation = common::AnimationChannel::kInterpolationStep;
	}
	else if (rSampler.interpolation == "CUBICSPLINE")
	{
		rChannel.uiInterpolation = common::AnimationChannel::kInterpolationCubicSpline;
	}
	else
	{
		rChannel.uiInterpolation = common::AnimationChannel::kInterpolationLinear;
	}
	return true;
}

void EmitCubicKeyframes(const float* pfTimes, const float* pfValues, int64_t iKeyframeCount, int iValueStride, common::AnimationChannel& rChannel, float& rfDuration, std::vector<common::AnimationKeyframeCubic>& rCubicKeyframes)
{
	rChannel.uiKeyframeStart = static_cast<uint32_t>(rCubicKeyframes.size());

	for (int64_t j = 0; j < iKeyframeCount; ++j)
	{
		common::AnimationKeyframeCubic keyframe {};
		keyframe.fTime = pfTimes[j];

		// CUBICSPLINE has 3 values per keyframe: in-tangent, value, out-tangent
		int64_t iBaseIndex = j * 3 * iValueStride;

		if (rChannel.uiTargetPath == common::AnimationChannel::kTargetPathRotation) // Rotation (vec4)
		{
			keyframe.f4InTangent = XMFLOAT4(pfValues[iBaseIndex + 0], pfValues[iBaseIndex + 1], pfValues[iBaseIndex + 2], pfValues[iBaseIndex + 3]);
			keyframe.f4Value = XMFLOAT4(pfValues[iBaseIndex + iValueStride + 0], pfValues[iBaseIndex + iValueStride + 1], pfValues[iBaseIndex + iValueStride + 2], pfValues[iBaseIndex + iValueStride + 3]);
			keyframe.f4OutTangent = XMFLOAT4(pfValues[iBaseIndex + 2 * iValueStride + 0], pfValues[iBaseIndex + 2 * iValueStride + 1], pfValues[iBaseIndex + 2 * iValueStride + 2], pfValues[iBaseIndex + 2 * iValueStride + 3]);
		}
		else // Translation/Scale (vec3)
		{
			keyframe.f4InTangent = XMFLOAT4(pfValues[iBaseIndex + 0], pfValues[iBaseIndex + 1], pfValues[iBaseIndex + 2], 0.0f);
			keyframe.f4Value = XMFLOAT4(pfValues[iBaseIndex + iValueStride + 0], pfValues[iBaseIndex + iValueStride + 1], pfValues[iBaseIndex + iValueStride + 2], 0.0f);
			keyframe.f4OutTangent = XMFLOAT4(pfValues[iBaseIndex + 2 * iValueStride + 0], pfValues[iBaseIndex + 2 * iValueStride + 1], pfValues[iBaseIndex + 2 * iValueStride + 2], 0.0f);
		}

		rfDuration = std::max(rfDuration, keyframe.fTime);
		rCubicKeyframes.push_back(keyframe);
	}
}

void EmitKeyframes(const float* pfTimes, const float* pfValues, int64_t iKeyframeCount, int iValueStride, common::AnimationChannel& rChannel, float& rfDuration, std::vector<common::AnimationKeyframe>& rKeyframes)
{
	rChannel.uiKeyframeStart = static_cast<uint32_t>(rKeyframes.size());

	for (int64_t j = 0; j < iKeyframeCount; ++j)
	{
		common::AnimationKeyframe keyframe {};
		keyframe.fTime = pfTimes[j];

		if (rChannel.uiTargetPath == common::AnimationChannel::kTargetPathRotation)
		{
			// Rotation (quaternion)
			keyframe.f4Value = XMFLOAT4(pfValues[j * iValueStride + 0], pfValues[j * iValueStride + 1], pfValues[j * iValueStride + 2], pfValues[j * iValueStride + 3]);
		}
		else if (rChannel.uiTargetPath == common::AnimationChannel::kTargetPathTranslation)
		{
			// Translation
			keyframe.f4Value = XMFLOAT4(pfValues[j * iValueStride + 0], pfValues[j * iValueStride + 1], pfValues[j * iValueStride + 2], 0.0f);
		}
		else
		{
			// Scale
			keyframe.f4Value = XMFLOAT4(pfValues[j * iValueStride + 0], pfValues[j * iValueStride + 1], pfValues[j * iValueStride + 2], 0.0f);
		}

		rfDuration = std::max(rfDuration, keyframe.fTime);
		rKeyframes.push_back(keyframe);
	}
}

}

bool DetermineAnimationPath(const tinygltf::Model& rGltfModel)
{
	if (rGltfModel.skins.empty())
	{
		return false;
	}

	const tinygltf::Skin& rSkin = rGltfModel.skins[0];
	std::unordered_set<int> skinJoints(rSkin.joints.begin(), rSkin.joints.end());

	for (const tinygltf::Animation& rAnim : rGltfModel.animations)
	{
		for (const tinygltf::AnimationChannel& rChannel : rAnim.channels)
		{
			if (rChannel.target_node >= 0 && skinJoints.count(rChannel.target_node) == 0)
			{
				return false;
			}
		}
	}
	return true;
}

void LoadAnimations(const tinygltf::Model& rModel, AnimationOutput& rOut)
{
	std::vector<common::AnimationClip>& rAnimations = rOut.rAnimations;
	std::vector<common::AnimationChannel>& rChannels = rOut.rChannels;
	std::vector<common::AnimationKeyframe>& rKeyframes = rOut.rKeyframes;
	std::vector<common::AnimationKeyframeCubic>& rCubicKeyframes = rOut.rCubicKeyframes;

	LOG(kDefault, kDebug, "LoadAnimations: {} nodes", rModel.nodes.size());

	for (const tinygltf::Animation& rAnim : rModel.animations)
	{
		common::AnimationClip animation {};

		// Set name
		size_t iNameLength = std::min(rAnim.name.size(), static_cast<size_t>(common::AnimationClip::kiMaxNameLength - 1));
		std::memcpy(animation.pcName, rAnim.name.c_str(), iNameLength);
		animation.pcName[iNameLength] = '\0';

		animation.uiChannelStart = static_cast<uint32_t>(rChannels.size());
		animation.uiChannelCount = 0;
		animation.fDuration = 0.0f;

		for (const tinygltf::AnimationChannel& rGltfChannel : rAnim.channels)
		{
			if (!KeepAnimationChannel(rModel, rGltfChannel))
			{
				continue;
			}

			if (!IsValidIndex(rGltfChannel.sampler, rAnim.samplers.size()))
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\") references sampler {} out of range (sampler count {}).", rAnim.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, rAnim.samplers.size()));
			}
			const tinygltf::AnimationSampler& rSampler = rAnim.samplers.at(static_cast<size_t>(rGltfChannel.sampler));

			common::AnimationChannel channel {};
			if (!MapAnimationChannel(rGltfChannel, rSampler, channel))
			{
				continue;
			}

			std::string context = std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {})", rAnim.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler);
			if (!IsValidIndex(rSampler.input, rModel.accessors.size()))
			{
				throw std::runtime_error(std::format("{} references input accessor {} out of range (accessor count {}).", context, rSampler.input, rModel.accessors.size()));
			}
			if (!IsValidIndex(rSampler.output, rModel.accessors.size()))
			{
				throw std::runtime_error(std::format("{} references output accessor {} out of range (accessor count {}).", context, rSampler.output, rModel.accessors.size()));
			}

			const tinygltf::Accessor& rInputAccessor = rModel.accessors.at(static_cast<size_t>(rSampler.input));
			size_t uiInputBytes = CheckedAnimationSize(rInputAccessor.count, sizeof(float), context);

			channel.uiKeyframeCount = static_cast<uint32_t>(rInputAccessor.count);

			int iValueStride = (channel.uiTargetPath == common::AnimationChannel::kTargetPathRotation) ? 4 : 3; // Rotation is vec4, others vec3
			size_t uiOutputElementCount = rInputAccessor.count;
			if (channel.uiInterpolation == common::AnimationChannel::kInterpolationCubicSpline)
			{
				uiOutputElementCount = CheckedAnimationSize(uiOutputElementCount, 3, context);
			}
			size_t uiOutputFloatCount = CheckedAnimationSize(uiOutputElementCount, static_cast<size_t>(iValueStride), context);
			size_t uiOutputBytes = CheckedAnimationSize(uiOutputFloatCount, sizeof(float), context);

			const float* pfTimes = AccessorFloats(rModel, rSampler.input, uiInputBytes, context + " input");
			const float* pfValues = AccessorFloats(rModel, rSampler.output, uiOutputBytes, context + " output");

			if (channel.uiInterpolation == common::AnimationChannel::kInterpolationCubicSpline) // CUBICSPLINE
			{
				EmitCubicKeyframes(pfTimes, pfValues, static_cast<int64_t>(rInputAccessor.count), iValueStride, channel, animation.fDuration, rCubicKeyframes);
			}
			else // STEP or LINEAR
			{
				EmitKeyframes(pfTimes, pfValues, static_cast<int64_t>(rInputAccessor.count), iValueStride, channel, animation.fDuration, rKeyframes);
			}

			rChannels.push_back(channel);
			++animation.uiChannelCount;
		}

		if (animation.uiChannelCount > 0)
		{
			rAnimations.push_back(animation);
		}
	}
}
