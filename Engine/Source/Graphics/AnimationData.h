#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// One frame's skinning output block for every instance one Render call draws; the caller keeps it for that call only.
struct SkinningReservation
{
	common::MeshData* pMeshData = nullptr;
	common::JointMatrix* pJointMatrices = nullptr;
	int64_t iMeshDataBase = 0;
	int64_t iJointBase = 0;
	int64_t iMaterialCount = 0;
	int64_t iJointsPerInstance = 0;
};

class AnimationData
{
public:

	void Load(std::span<const std::byte> animationData, int64_t iMaterialCount, common::crc_t crc);
	void EvaluateWorldMatrices(int64_t iAnimationIndex, float fTime, XMMATRIX* pmatWorldMatrices) const;
	void EvaluateMaterial(int64_t iMaterialIndex, const XMMATRIX* pmatWorldMatrices, common::MeshData* pMeshData, common::JointMatrix* pJointMatrices, int64_t iJointMatrixOffset) const;
	void EvaluateAnimation(int64_t iAnimationIndex, float fTime, std::span<common::MeshData> meshData, common::JointMatrix* pJointMatrices, int64_t iJointMatrixOffset) const;
	int64_t SkinnedMaterialCount(int64_t iMaterialCount) const;
	// Main thread only: it bump-allocates from the command buffer's skinning buffers. The mapped pointers it returns stay
	// valid only until the next skinning allocation on that command buffer, which can grow and move those buffers.
	SkinningReservation ReserveSkinning(int64_t iCommandBuffer, int64_t iInstanceCount) const;
	// Safe on worker threads for distinct instances: it reserves no skinning space and writes only instance iInstance's slabs.
	int64_t EvaluateInstance(const SkinningReservation& rReservation, int64_t iInstance, float fTime) const;
	std::chrono::duration<float> AdvanceLoopingTime(std::chrono::duration<float> time, std::chrono::duration<float> deltaTime) const;

	common::crc_t mCrc = 0;
	common::AnimationHeader mHeader {};
	int64_t miMaterialCount = 0;

	// Pointers into eagerly-loaded pack memory (zero-copy)
	const common::ModelNode* mpNodes = nullptr;
	const uint16_t* mpuiSkinJointToNode = nullptr;
	const common::AnimationClip* mpAnimations = nullptr;
	const common::MaterialInfo* mpMaterialInfos = nullptr;
	const common::AnimationChannel* mpChannels = nullptr;
	const common::AnimationKeyframe* mpKeyframes = nullptr;
	const common::AnimationKeyframeCubic* mpCubicKeyframes = nullptr;

	// Pre-computed at load time, runtime-sized to the header counts (see Load)
	common::AlignedUniquePtr<XMMATRIX> mpBindPoseLocalMatrices;       // uiNodeCount entries
	std::vector<int64_t> mAnimatedNodes;                             // uiAnimationCount * uiNodeCount, row stride uiNodeCount
	common::AlignedUniquePtr<XMMATRIX> mpAlignedInverseBindMatrices;  // uiSkinJointCount entries
	common::AlignedUniquePtr<XMMATRIX> mpAlignedRelativeTransforms;   // SceneHeader::uiMaterialCount entries

private:

	XMVECTOR InterpolateKeyframes(const common::AnimationChannel& rChannel, float fTime) const;
};

// Game collections update client-only simulation animation clocks from mpAnimations[].fDuration.
// These clocks stay outside SharedCrcMembers because they depend on client-only pack data.
inline std::unordered_map<common::crc_t, AnimationData> gAnimationDataMap;

void LoadAnimationDataFromEagerChunks();

} // namespace engine

#endif // defined(BT_CLIENT)
