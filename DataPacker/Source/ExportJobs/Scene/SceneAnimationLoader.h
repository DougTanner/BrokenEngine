#pragma once

namespace tinygltf
{
	class Model;
} // namespace tinygltf

// Determine whether to use skeletal or node-based animation.
// Returns true for skeletal (all channels target skin joints), false for node-based.
bool DetermineAnimationPath(const tinygltf::Model& rGltfModel);

// Bundle of LoadAnimations output buffers — appended to in place.
struct AnimationOutput
{
	std::vector<common::AnimationClip>& rAnimations;
	std::vector<common::AnimationChannel>& rChannels;
	std::vector<common::AnimationKeyframe>& rKeyframes;
	std::vector<common::AnimationKeyframeCubic>& rCubicKeyframes;
};

void LoadAnimations(const tinygltf::Model& rModel, AnimationOutput& rOutput);

common::AnimationClip BeginAnimationClip(std::string_view name, int64_t iChannelStart);
void AppendAnimationChannel(const common::AnimationChannel& rChannel, common::AnimationClip& rAnimation, std::vector<common::AnimationChannel>& rChannels);
void FinishAnimationClip(const common::AnimationClip& rAnimation, std::vector<common::AnimationClip>& rAnimations);
void EmitKeyframes(std::span<const float> times, std::span<const float> values, int64_t iValueStride, common::AnimationChannel& rChannel, float& rfDuration, std::vector<common::AnimationKeyframe>& rKeyframes);
