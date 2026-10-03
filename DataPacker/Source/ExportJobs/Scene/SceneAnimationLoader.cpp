#include "SceneAnimationLoader.h"


static bool IsValidIndex(int64_t iIndex, int64_t iSize)
{
	return iIndex >= 0 && iIndex < iSize;
}

static size_t CheckedAnimationSize(size_t uiCount, size_t uiElementSize, std::string_view context)
{
	if (uiElementSize != 0 && uiCount > std::numeric_limits<size_t>::max() / uiElementSize)
	{
		throw std::runtime_error(std::format("{} has a byte size that overflows size_t.", context));
	}
	return uiCount * uiElementSize;
}

static const float* AccessorFloats(const tinygltf::Model& rModel, int iAccessor, size_t uiRequiredBytes, std::string_view context)
{
	if (!IsValidIndex(iAccessor, std::ssize(rModel.accessors)))
	{
		throw std::runtime_error(std::format("{} references accessor {} out of range (accessor count {}).", context, iAccessor, std::ssize(rModel.accessors)));
	}

	const tinygltf::Accessor& rAccessor = rModel.accessors.at(static_cast<size_t>(iAccessor));
	if (rAccessor.sparse.isSparse)
	{
		throw std::runtime_error(std::format("{} has sparse accessor {}.", context, iAccessor));
	}
	if (rAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		throw std::runtime_error(std::format("{} accessor {} does not use FLOAT components.", context, iAccessor));
	}
	if (rAccessor.count == 0)
	{
		throw std::runtime_error(std::format("{} accessor {} has zero elements.", context, iAccessor));
	}
	if (!IsValidIndex(rAccessor.bufferView, std::ssize(rModel.bufferViews)))
	{
		throw std::runtime_error(std::format("{} accessor {} references buffer view {} out of range (buffer view count {}).", context, iAccessor, rAccessor.bufferView, std::ssize(rModel.bufferViews)));
	}

	const tinygltf::BufferView& rBufferView = rModel.bufferViews.at(static_cast<size_t>(rAccessor.bufferView));
	if (!IsValidIndex(rBufferView.buffer, std::ssize(rModel.buffers)))
	{
		throw std::runtime_error(std::format("{} accessor {} buffer view {} references buffer {} out of range (buffer count {}).", context, iAccessor, rAccessor.bufferView, rBufferView.buffer, std::ssize(rModel.buffers)));
	}
	if (rBufferView.byteStride != 0)
	{
		throw std::runtime_error(std::format("{} accessor {} buffer view {} has unsupported animation byte stride {}.", context, iAccessor, rAccessor.bufferView, rBufferView.byteStride));
	}

	int32_t iComponentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(rAccessor.componentType));
	int32_t iComponentCount = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(rAccessor.type));
	if (iComponentSize <= 0)
	{
		throw std::runtime_error(std::format("{} accessor {} has an invalid component or element type.", context, iAccessor));
	}
	if (iComponentCount <= 0)
	{
		throw std::runtime_error(std::format("{} accessor {} has an invalid component or element type.", context, iAccessor));
	}

	const tinygltf::Buffer& rBuffer = rModel.buffers.at(static_cast<size_t>(rBufferView.buffer));
	if (rBufferView.byteOffset > rBuffer.data.size() || rBufferView.byteLength > rBuffer.data.size() - rBufferView.byteOffset)
	{
		throw std::runtime_error(std::format("{} accessor {} buffer view {} lies outside buffer {}.", context, iAccessor, rAccessor.bufferView, rBufferView.buffer));
	}

	size_t uiElementSize = CheckedAnimationSize(static_cast<size_t>(iComponentSize), static_cast<size_t>(iComponentCount), context);
	size_t uiAccessorBytes = CheckedAnimationSize(rAccessor.count, uiElementSize, context);
	if (rAccessor.byteOffset > rBufferView.byteLength || uiAccessorBytes > rBufferView.byteLength - rAccessor.byteOffset)
	{
		throw std::runtime_error(std::format("{} accessor {} lies outside buffer view {}.", context, iAccessor, rAccessor.bufferView));
	}
	if (uiRequiredBytes > uiAccessorBytes)
	{
		throw std::runtime_error(std::format("{} requires {} bytes but accessor {} declares only {} bytes.", context, uiRequiredBytes, iAccessor, uiAccessorBytes));
	}

	size_t uiBufferOffset = rBufferView.byteOffset + rAccessor.byteOffset;
	if (rAccessor.byteOffset % alignof(float) != 0)
	{
		throw std::runtime_error(std::format("{} accessor {} float data is misaligned.", context, iAccessor));
	}
	if (uiBufferOffset % alignof(float) != 0)
	{
		throw std::runtime_error(std::format("{} accessor {} float data is misaligned.", context, iAccessor));
	}
	if (uiRequiredBytes > rBuffer.data.size() - uiBufferOffset)
	{
		throw std::runtime_error(std::format("{} accessor {} read lies outside buffer {}.", context, iAccessor, rBufferView.buffer));
	}

	return reinterpret_cast<const float*>(rBuffer.data.data() + uiBufferOffset);
}

static bool KeepAnimationChannel(const tinygltf::Model& rModel, const tinygltf::AnimationChannel& rGltfChannel)
{
	if (!IsValidIndex(rGltfChannel.target_node, std::ssize(rModel.nodes)))
	{
		LOG(kDefault, kVerbose, "  FILTERED: channel targeting node {} out of range (node count {})", rGltfChannel.target_node, std::ssize(rModel.nodes));
		return false;
	}
	LOG(kDefault, kVerbose, "  KEPT: channel targeting node {} (\"{}\") -> node index {}", rGltfChannel.target_node, rModel.nodes.at(rGltfChannel.target_node).name, rGltfChannel.target_node);
	return true;
}

static bool MapAnimationChannel(const tinygltf::AnimationChannel& rGltfChannel, const tinygltf::AnimationSampler& rSampler, common::AnimationChannel& rChannel)
{
	rChannel.uiNodeIndex = static_cast<uint16_t>(rGltfChannel.target_node);

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
		return false;
	}

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

static void EmitCubicKeyframes(std::span<const float> times, std::span<const float> values, int64_t iValueStride, common::AnimationChannel& rChannel, float& rfDuration, std::vector<common::AnimationKeyframeCubic>& rCubicKeyframes)
{
	rChannel.uiKeyframeStart = static_cast<uint32_t>(rCubicKeyframes.size());

	for (int64_t j = 0; j < std::ssize(times); ++j)
	{
		common::AnimationKeyframeCubic keyframe {};
		keyframe.fTime = times[j];

		// CUBICSPLINE has 3 values per keyframe: in-tangent, value, out-tangent
		int64_t iBaseIndex = j * 3 * iValueStride;

		if (rChannel.uiTargetPath == common::AnimationChannel::kTargetPathRotation) // Rotation (vec4)
		{
			keyframe.f4InTangent = XMFLOAT4(values[iBaseIndex + 0], values[iBaseIndex + 1], values[iBaseIndex + 2], values[iBaseIndex + 3]);
			keyframe.f4Value = XMFLOAT4(values[iBaseIndex + iValueStride + 0], values[iBaseIndex + iValueStride + 1], values[iBaseIndex + iValueStride + 2], values[iBaseIndex + iValueStride + 3]);
			keyframe.f4OutTangent = XMFLOAT4(values[iBaseIndex + 2 * iValueStride + 0], values[iBaseIndex + 2 * iValueStride + 1], values[iBaseIndex + 2 * iValueStride + 2], values[iBaseIndex + 2 * iValueStride + 3]);
		}
		else // Translation/Scale (vec3)
		{
			keyframe.f4InTangent = XMFLOAT4(values[iBaseIndex + 0], values[iBaseIndex + 1], values[iBaseIndex + 2], 0.0f);
			keyframe.f4Value = XMFLOAT4(values[iBaseIndex + iValueStride + 0], values[iBaseIndex + iValueStride + 1], values[iBaseIndex + iValueStride + 2], 0.0f);
			keyframe.f4OutTangent = XMFLOAT4(values[iBaseIndex + 2 * iValueStride + 0], values[iBaseIndex + 2 * iValueStride + 1], values[iBaseIndex + 2 * iValueStride + 2], 0.0f);
		}

		rfDuration = std::max(rfDuration, keyframe.fTime);
		rCubicKeyframes.push_back(keyframe);
	}
}

static void EmitKeyframes(std::span<const float> times, std::span<const float> values, int64_t iValueStride, common::AnimationChannel& rChannel, float& rfDuration, std::vector<common::AnimationKeyframe>& rKeyframes)
{
	rChannel.uiKeyframeStart = static_cast<uint32_t>(rKeyframes.size());

	for (int64_t j = 0; j < std::ssize(times); ++j)
	{
		common::AnimationKeyframe keyframe {};
		keyframe.fTime = times[j];

		if (rChannel.uiTargetPath == common::AnimationChannel::kTargetPathRotation)
		{
			keyframe.f4Value = XMFLOAT4(values[j * iValueStride + 0], values[j * iValueStride + 1], values[j * iValueStride + 2], values[j * iValueStride + 3]);
		}
		else if (rChannel.uiTargetPath == common::AnimationChannel::kTargetPathTranslation)
		{
			keyframe.f4Value = XMFLOAT4(values[j * iValueStride + 0], values[j * iValueStride + 1], values[j * iValueStride + 2], 0.0f);
		}
		else
		{
			keyframe.f4Value = XMFLOAT4(values[j * iValueStride + 0], values[j * iValueStride + 1], values[j * iValueStride + 2], 0.0f);
		}

		rfDuration = std::max(rfDuration, keyframe.fTime);
		rKeyframes.push_back(keyframe);
	}
}


bool DetermineAnimationPath(const tinygltf::Model& rGltfModel)
{
	if (rGltfModel.skins.empty())
	{
		return false;
	}

	const tinygltf::Skin& rSkin = rGltfModel.skins.at(0);
	std::unordered_set<int> skinJoints(rSkin.joints.begin(), rSkin.joints.end());

	for (const tinygltf::Animation& rAnimation : rGltfModel.animations)
	{
		for (const tinygltf::AnimationChannel& rChannel : rAnimation.channels)
		{
			if (rChannel.target_node >= 0 && skinJoints.count(rChannel.target_node) == 0)
			{
				return false;
			}
		}
	}
	return true;
}

void LoadAnimations(const tinygltf::Model& rModel, AnimationOutput& rOutput)
{
	std::vector<common::AnimationClip>& rAnimations = rOutput.rAnimations;
	std::vector<common::AnimationChannel>& rChannels = rOutput.rChannels;
	std::vector<common::AnimationKeyframe>& rKeyframes = rOutput.rKeyframes;
	std::vector<common::AnimationKeyframeCubic>& rCubicKeyframes = rOutput.rCubicKeyframes;

	LOG(kDefault, kDebug, "LoadAnimations: {} nodes", std::ssize(rModel.nodes));

	for (const tinygltf::Animation& rAnimation : rModel.animations)
	{
		common::AnimationClip animation {};

		size_t iNameLength = std::min(rAnimation.name.size(), static_cast<size_t>(common::AnimationClip::kiMaxNameLength - 1));
		std::memcpy(animation.pcName, rAnimation.name.c_str(), iNameLength);
		animation.pcName[iNameLength] = '\0';

		animation.uiChannelStart = static_cast<uint32_t>(rChannels.size());
		animation.uiChannelCount = 0;
		animation.fDuration = 0.0f;

		for (const tinygltf::AnimationChannel& rGltfChannel : rAnimation.channels)
		{
			if (!KeepAnimationChannel(rModel, rGltfChannel))
			{
				continue;
			}

			if (!IsValidIndex(rGltfChannel.sampler, std::ssize(rAnimation.samplers)))
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\") references sampler {} out of range (sampler count {}).", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, std::ssize(rAnimation.samplers)));
			}
			const tinygltf::AnimationSampler& rSampler = rAnimation.samplers.at(static_cast<size_t>(rGltfChannel.sampler));

			common::AnimationChannel channel {};
			if (!MapAnimationChannel(rGltfChannel, rSampler, channel))
			{
				continue;
			}

			std::string context = std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {})", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler);
			if (!IsValidIndex(rSampler.input, std::ssize(rModel.accessors)))
			{
				throw std::runtime_error(std::format("{} references input accessor {} out of range (accessor count {}).", context, rSampler.input, std::ssize(rModel.accessors)));
			}
			if (!IsValidIndex(rSampler.output, std::ssize(rModel.accessors)))
			{
				throw std::runtime_error(std::format("{} references output accessor {} out of range (accessor count {}).", context, rSampler.output, std::ssize(rModel.accessors)));
			}

			const tinygltf::Accessor& rInputAccessor = rModel.accessors.at(static_cast<size_t>(rSampler.input));
			const tinygltf::Accessor& rOutputAccessor = rModel.accessors.at(static_cast<size_t>(rSampler.output));
			if (rInputAccessor.count == 0)
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) has no input keyframes.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler));
			}
			if (rInputAccessor.count > std::numeric_limits<uint32_t>::max())
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) input keyframe count {} exceeds the supported limit {}.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, rInputAccessor.count, std::numeric_limits<uint32_t>::max()));
			}
			if (rInputAccessor.type != TINYGLTF_TYPE_SCALAR)
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) input accessor {} is not SCALAR.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, rSampler.input));
			}

			int iRequiredOutputType = channel.uiTargetPath == common::AnimationChannel::kTargetPathRotation ? TINYGLTF_TYPE_VEC4 : TINYGLTF_TYPE_VEC3;
			if (rOutputAccessor.type != iRequiredOutputType)
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) output accessor {} has an incompatible element type.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, rSampler.output));
			}

			if (channel.uiInterpolation == common::AnimationChannel::kInterpolationCubicSpline)
			{
				if (rOutputAccessor.count % 3 != 0 || rOutputAccessor.count / 3 != rInputAccessor.count)
				{
					throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) CUBICSPLINE output count {} is not three times its input count {}.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, rOutputAccessor.count, rInputAccessor.count));
				}
			}
			else if (rOutputAccessor.count != rInputAccessor.count)
			{
				throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) output count {} does not match its input count {}.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler, rOutputAccessor.count, rInputAccessor.count));
			}

			int64_t iValueStride = (channel.uiTargetPath == common::AnimationChannel::kTargetPathRotation) ? 4 : 3; // Rotation is vec4, others vec3
			size_t uiInputBytes = CheckedAnimationSize(rInputAccessor.count, sizeof(float), context);
			size_t uiOutputElementCount = rInputAccessor.count;
			if (channel.uiInterpolation == common::AnimationChannel::kInterpolationCubicSpline)
			{
				uiOutputElementCount = CheckedAnimationSize(uiOutputElementCount, 3, context);
			}
			size_t uiOutputFloatCount = CheckedAnimationSize(uiOutputElementCount, static_cast<size_t>(iValueStride), context);
			size_t uiOutputBytes = CheckedAnimationSize(uiOutputFloatCount, sizeof(float), context);

			const float* pfTimes = AccessorFloats(rModel, rSampler.input, uiInputBytes, context + " input");
			for (int64_t i = 0; i < static_cast<int64_t>(rInputAccessor.count); ++i)
			{
				if (!std::isfinite(pfTimes[i]) || (i > 0 && pfTimes[i] <= pfTimes[i - 1]))
				{
					throw std::runtime_error(std::format("Animation \"{}\" channel (target node {}, path \"{}\", sampler {}) input times are not finite and strictly increasing.", rAnimation.name, rGltfChannel.target_node, rGltfChannel.target_path, rGltfChannel.sampler));
				}
			}
			const float* pfValues = AccessorFloats(rModel, rSampler.output, uiOutputBytes, context + " output");

			channel.uiKeyframeCount = static_cast<uint32_t>(rInputAccessor.count);

			if (channel.uiInterpolation == common::AnimationChannel::kInterpolationCubicSpline)
			{
				EmitCubicKeyframes(std::span<const float>(pfTimes, rInputAccessor.count), std::span<const float>(pfValues, uiOutputFloatCount), iValueStride, channel, animation.fDuration, rCubicKeyframes);
			}
			else // STEP or LINEAR
			{
				EmitKeyframes(std::span<const float>(pfTimes, rInputAccessor.count), std::span<const float>(pfValues, uiOutputFloatCount), iValueStride, channel, animation.fDuration, rKeyframes);
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
