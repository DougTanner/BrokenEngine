#if defined(BT_CLIENT)

#include "AnimationData.h"

#include "File/PackChunks.h"

namespace engine
{

template <typename KEYFRAME>
static void FindKeyframePair(std::span<const KEYFRAME> keyframes, float fTime, int64_t& riKeyframe0, int64_t& riKeyframe1)
{
	int64_t iKeyframeCount = std::ssize(keyframes);
	if (fTime <= keyframes[0].fTime)
	{
		riKeyframe0 = 0;
		riKeyframe1 = 0;
		return;
	}
	if (fTime >= keyframes[iKeyframeCount - 1].fTime)
	{
		riKeyframe0 = iKeyframeCount - 1;
		riKeyframe1 = iKeyframeCount - 1;
		return;
	}

	// Find first keyframe after fTime, then pair it with the preceding keyframe.
	auto it = std::upper_bound(keyframes.begin(), keyframes.end(), fTime, [](float fValue, const KEYFRAME& rKeyframe)
	{
		return fValue < rKeyframe.fTime;
	});
	int64_t iUpperIndex = it - keyframes.begin();
	riKeyframe0 = iUpperIndex > 0 ? iUpperIndex - 1 : 0;
	riKeyframe1 = iUpperIndex < iKeyframeCount ? iUpperIndex : iKeyframeCount - 1;
}

void AnimationData::Load(std::span<const std::byte> animationData, int64_t iMaterialCount, common::crc_t crc)
{
	const std::byte* pAnimationData = animationData.data();
	int64_t iAnimationBytes = std::ssize(animationData);
	mCrc = crc;

	// The animation extent is EagerChunk::iDataSize (ChunkLocation::uiSize - kiChunkDataOffset) minus the scene-array prefix.
	if (iAnimationBytes < static_cast<int64_t>(sizeof(mHeader)))
	{
		throw std::ios_base::failure("AnimationData::Load");
	}

	std::memcpy(&mHeader, pAnimationData, sizeof(mHeader));
	pAnimationData += sizeof(mHeader);

	// Animation and material counts have structural maxima; the material count is the scene header's, which the renderers also use. Node count is bounded by its uint16_t field, the byte extent, and the exporter's int16_t parent-index ASSERT; skin-joint count cannot exceed it.
	// Channels and keyframes have no producer structural maximum, so the deserialization ceiling bounds their arithmetic.
	// Counts size allocations and advance aliases; invalid counts can over-allocate or leave the eager buffer.
	// Animated chunks require at least one clip because evaluation indexes the clip array.
	if (mHeader.skeleton.uiSkinJointCount > mHeader.skeleton.uiNodeCount
	 || mHeader.uiAnimationCount > common::AnimationHeader::kiMaxAnimations || mHeader.uiAnimationCount == 0
	 || iMaterialCount > common::SceneHeader::kiMaxMaterials || mHeader.uiChannelCount > common::kiMaxDeserializedCapacity
	 || mHeader.uiKeyframeCount > common::kiMaxDeserializedCapacity || mHeader.uiCubicKeyframeCount > common::kiMaxDeserializedCapacity)
	{
		throw std::ios_base::failure("AnimationData::Load");
	}

	LOG(kLoading, kDebug, "AnimationData::Load: animations {}, channels {}, keyframes {}, cubicKeyframes {}, nodes {}, skinJoints {}", mHeader.uiAnimationCount, mHeader.uiChannelCount, mHeader.uiKeyframeCount, mHeader.uiCubicKeyframeCount, mHeader.skeleton.uiNodeCount, mHeader.skeleton.uiSkinJointCount);

	// BoundAdvance checks each section against the chunk-table extent before advancing the alias cursor; ChunkHeader::iSize excludes appended animation data.
	int64_t iOffset = static_cast<int64_t>(sizeof(mHeader));
	auto BoundAdvance = [&](int64_t iSectionBytes)
	{
		if (iOffset > iAnimationBytes - iSectionBytes) // overflow-safe form of iOffset + iSectionBytes > iAnimationBytes
		{
			throw std::ios_base::failure("AnimationData::Load");
		}
		iOffset += iSectionBytes;
		pAnimationData += iSectionBytes;
	};

	mpNodes = reinterpret_cast<const common::ModelNode*>(pAnimationData);
	BoundAdvance(mHeader.skeleton.uiNodeCount * static_cast<int64_t>(sizeof(common::ModelNode)));

	mpuiSkinJointToNode = reinterpret_cast<const uint16_t*>(pAnimationData);
	BoundAdvance(common::RoundUp<int64_t, common::kiAnimationSectionAlignment>(mHeader.skeleton.uiSkinJointCount * static_cast<int64_t>(sizeof(uint16_t))));

	// Inverse bind matrices: read via pointer, pre-compute into aligned array
	const XMFLOAT4X4* pInverseBindMatrices = reinterpret_cast<const XMFLOAT4X4*>(pAnimationData);
	BoundAdvance(mHeader.skeleton.uiSkinJointCount * static_cast<int64_t>(sizeof(XMFLOAT4X4)));

	mpAnimations = reinterpret_cast<const common::AnimationClip*>(pAnimationData);
	BoundAdvance(mHeader.uiAnimationCount * static_cast<int64_t>(sizeof(common::AnimationClip)));

	mpMaterialInfos = reinterpret_cast<const common::MaterialInfo*>(pAnimationData);
	BoundAdvance(iMaterialCount * static_cast<int64_t>(sizeof(common::MaterialInfo)));

	mpChannels = reinterpret_cast<const common::AnimationChannel*>(pAnimationData);
	BoundAdvance(mHeader.uiChannelCount * static_cast<int64_t>(sizeof(common::AnimationChannel)));

	mpKeyframes = reinterpret_cast<const common::AnimationKeyframe*>(pAnimationData);
	BoundAdvance(mHeader.uiKeyframeCount * static_cast<int64_t>(sizeof(common::AnimationKeyframe)));

	mpCubicKeyframes = reinterpret_cast<const common::AnimationKeyframeCubic*>(pAnimationData);
	BoundAdvance(mHeader.uiCubicKeyframeCount * static_cast<int64_t>(sizeof(common::AnimationKeyframeCubic)));

	// The exporter writes exactly the summed sections, so leftover bytes mean a count disagrees with the data, such as a scene material count that does not match the material infos written.
	if (iOffset != iAnimationBytes)
	{
		throw std::ios_base::failure("AnimationData::Load");
	}

	// Secondary indices require validation independently of counts: channels write the animated-node mask and skin joints read world matrices.
	// Validate material parents, channel and clip ranges, skin-joint mappings, and topologically ordered node parents once at load time so per-frame evaluation stays unchecked.
	for (int64_t i = 0; i < mHeader.skeleton.uiSkinJointCount; ++i)
	{
		if (mpuiSkinJointToNode[i] >= mHeader.skeleton.uiNodeCount)
		{
			throw std::ios_base::failure("AnimationData::Load");
		}
	}
	for (int64_t i = 0; i < iMaterialCount; ++i)
	{
		if (static_cast<int32_t>(mpMaterialInfos[i].iParentNodeIndex) >= static_cast<int32_t>(mHeader.skeleton.uiNodeCount))
		{
			throw std::ios_base::failure("AnimationData::Load");
		}
		// Corrupt chunk: a skinned material in a skeleton with no skin joints
		ASSERT(!(mpMaterialInfos[i].flags & common::MaterialFlags::kSkinned) || mHeader.skeleton.uiSkinJointCount > 0);
	}
	for (int64_t i = 0; i < mHeader.uiChannelCount; ++i)
	{
		const common::AnimationChannel& rChannel = mpChannels[i];
		int64_t iKeyframeTotal = rChannel.uiInterpolation == common::AnimationChannel::kiInterpolationCubicSpline ? mHeader.uiCubicKeyframeCount : mHeader.uiKeyframeCount;
		if (rChannel.uiNodeIndex >= mHeader.skeleton.uiNodeCount || rChannel.uiKeyframeCount == 0
		 || rChannel.uiKeyframeStart > iKeyframeTotal || rChannel.uiKeyframeCount > iKeyframeTotal - rChannel.uiKeyframeStart)
		{
			throw std::ios_base::failure("AnimationData::Load");
		}
	}
	for (int64_t i = 0; i < mHeader.uiAnimationCount; ++i)
	{
		const common::AnimationClip& rClip = mpAnimations[i];
		if (rClip.uiChannelStart > mHeader.uiChannelCount || rClip.uiChannelCount > mHeader.uiChannelCount - rClip.uiChannelStart)
		{
			throw std::ios_base::failure("AnimationData::Load");
		}
	}

	// Verify the exact root sentinel and topological order required by the single-pass world-matrix build.
	for (int64_t i = 0; i < mHeader.skeleton.uiNodeCount; ++i)
	{
		int64_t iParentIndex = mpNodes[i].iParentIndex;
		if (iParentIndex != -1 && (iParentIndex < 0 || iParentIndex >= static_cast<int64_t>(i)))
		{
			throw std::ios_base::failure("AnimationData::Load");
		}
	}

	mpBindPoseLocalMatrices = common::MakeAligned<XMMATRIX>(mHeader.skeleton.uiNodeCount);
	for (int64_t i = 0; i < mHeader.skeleton.uiNodeCount; ++i)
	{
		const common::ModelNode& rNode = mpNodes[i];
		XMMATRIX matBind = XMLoadFloat4x4(&rNode.f4x4BindMatrix);
		XMMATRIX matScale = XMMatrixScalingFromVector(XMLoadFloat4(&rNode.f4BindScale));
		XMMATRIX matRotation = XMMatrixRotationQuaternion(XMLoadFloat4(&rNode.f4BindRotation));
		XMMATRIX matTranslation = XMMatrixTranslationFromVector(XMLoadFloat4(&rNode.f4BindTranslation));
		mpBindPoseLocalMatrices[i] = matBind * matScale * matRotation * matTranslation;
	}

	// Build per-animation animated node masks (1D, row stride = uiNodeCount; uiNodeIndex bounded above)
	mAnimatedNodes.assign(static_cast<size_t>(mHeader.uiAnimationCount) * mHeader.skeleton.uiNodeCount, 0);
	for (int64_t i = 0; i < mHeader.uiAnimationCount; ++i)
	{
		const common::AnimationClip& rClip = mpAnimations[i];
		for (int64_t j = rClip.uiChannelStart; j < rClip.uiChannelStart + rClip.uiChannelCount; ++j)
		{
			mAnimatedNodes.at(i * mHeader.skeleton.uiNodeCount + mpChannels[j].uiNodeIndex) = 1;
		}
	}

	mpAlignedInverseBindMatrices = common::MakeAligned<XMMATRIX>(mHeader.skeleton.uiSkinJointCount);
	for (int64_t i = 0; i < mHeader.skeleton.uiSkinJointCount; ++i)
	{
		mpAlignedInverseBindMatrices[i] = XMLoadFloat4x4(&pInverseBindMatrices[i]);
	}

	mpAlignedRelativeTransforms = common::MakeAligned<XMMATRIX>(iMaterialCount);
	for (int64_t i = 0; i < iMaterialCount; ++i)
	{
		mpAlignedRelativeTransforms[i] = XMLoadFloat4x4(&mpMaterialInfos[i].f4x4RelativeTransform);
	}
}

int64_t AnimationData::SkinnedMaterialCount(int64_t iMaterialCount) const
{
	int64_t iSkinnedMaterialCount = 0;
	for (int64_t i = 0; i < iMaterialCount; ++i)
	{
		if (mpMaterialInfos[i].flags & common::MaterialFlags::kSkinned)
		{
			++iSkinnedMaterialCount;
		}
	}
	return iSkinnedMaterialCount;
}

void AnimationData::EvaluateAnimation(int64_t iAnimationIndex, float fTime, std::span<common::MeshData> meshData, common::JointMatrix* pJointMatrices, int64_t iJointMatrixOffset) const
{
	auto worldMatricesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMMATRIX*>(mHeader.skeleton.uiNodeCount * static_cast<int64_t>(sizeof(XMMATRIX)));
	EvaluateWorldMatrices(iAnimationIndex, fTime, worldMatricesAllocation.mpData);

	for (int64_t i = 0; i < std::ssize(meshData); ++i)
	{
		EvaluateMaterial(i, worldMatricesAllocation.mpData, &meshData[i], pJointMatrices, iJointMatrixOffset);

		const common::MaterialInfo& rMaterialInfo = mpMaterialInfos[i];
		if (rMaterialInfo.flags & common::MaterialFlags::kSkinned)
		{
			iJointMatrixOffset += mHeader.skeleton.uiSkinJointCount;
		}
	}
}

int64_t AnimationData::FindAnimation(std::string_view name) const
{
	for (int64_t i = 0; i < static_cast<int64_t>(mHeader.uiAnimationCount); ++i)
	{
		if (name == mpAnimations[i].pcName)
		{
			return i;
		}
	}
	return -1;
}

XMVECTOR AnimationData::InterpolateKeyframes(const common::AnimationChannel& rChannel, float fTime) const
{
	int64_t iKeyframeCount = rChannel.uiKeyframeCount;

	// CUBICSPLINE path: uses AnimationKeyframeCubic with tangent fields
	if (rChannel.uiInterpolation == common::AnimationChannel::kiInterpolationCubicSpline)
	{
		const common::AnimationKeyframeCubic* pKeyframes = &mpCubicKeyframes[rChannel.uiKeyframeStart];

		int64_t iKeyframe0 = 0;
		int64_t iKeyframe1 = 0;

		FindKeyframePair(std::span(pKeyframes, static_cast<size_t>(iKeyframeCount)), fTime, iKeyframe0, iKeyframe1);

		const common::AnimationKeyframeCubic& rKey0 = pKeyframes[iKeyframe0];
		const common::AnimationKeyframeCubic& rKey1 = pKeyframes[iKeyframe1];

		if (iKeyframe0 == iKeyframe1)
		{
			return XMLoadFloat4(&rKey0.f4Value);
		}

		float fDelta = rKey1.fTime - rKey0.fTime;
		float fInterpolationFraction = (fTime - rKey0.fTime) / fDelta;
		float fInterpolationFractionSquared = fInterpolationFraction * fInterpolationFraction;
		float fInterpolationFractionCubed = fInterpolationFractionSquared * fInterpolationFraction;

		// glTF 2.0 maps m0 to keyframe k's OUT tangent and m1 to keyframe k+1's IN tangent.
		float fStartValueWeight = 2.0f * fInterpolationFractionCubed - 3.0f * fInterpolationFractionSquared + 1.0f;  // p0 coefficient
		float fStartTangentWeight = fInterpolationFractionCubed - 2.0f * fInterpolationFractionSquared + fInterpolationFraction;           // m0 coefficient
		float fEndValueWeight = -2.0f * fInterpolationFractionCubed + 3.0f * fInterpolationFractionSquared;        // p1 coefficient
		float fEndTangentWeight = fInterpolationFractionCubed - fInterpolationFractionSquared;                        // m1 coefficient

		XMVECTOR vecStartValue = XMLoadFloat4(&rKey0.f4Value);
		XMVECTOR vecStartTangent = XMVectorScale(XMLoadFloat4(&rKey0.f4OutTangent), fDelta);
		XMVECTOR vecEndValue = XMLoadFloat4(&rKey1.f4Value);
		XMVECTOR vecEndTangent = XMVectorScale(XMLoadFloat4(&rKey1.f4InTangent), fDelta);

		XMVECTOR vecResult = XMVectorAdd(XMVectorAdd(XMVectorScale(vecStartValue, fStartValueWeight), XMVectorScale(vecStartTangent, fStartTangentWeight)), XMVectorAdd(XMVectorScale(vecEndValue, fEndValueWeight), XMVectorScale(vecEndTangent, fEndTangentWeight)));

		if (rChannel.uiTargetPath == common::AnimationChannel::kiTargetPathRotation)
		{
			vecResult = XMQuaternionNormalize(vecResult);
		}

		return vecResult;
	}

	// STEP/LINEAR path: uses compact AnimationKeyframe without tangent fields
	const common::AnimationKeyframe* pKeyframes = &mpKeyframes[rChannel.uiKeyframeStart];

	int64_t iKeyframe0 = 0;
	int64_t iKeyframe1 = 0;

	FindKeyframePair(std::span(pKeyframes, static_cast<size_t>(iKeyframeCount)), fTime, iKeyframe0, iKeyframe1);

	const common::AnimationKeyframe& rKey0 = pKeyframes[iKeyframe0];
	const common::AnimationKeyframe& rKey1 = pKeyframes[iKeyframe1];

	if (rChannel.uiInterpolation == common::AnimationChannel::kiInterpolationStep)
	{
		return XMLoadFloat4(&rKey0.f4Value);
	}
	if (iKeyframe0 == iKeyframe1)
	{
		return XMLoadFloat4(&rKey0.f4Value);
	}

	float fDelta = rKey1.fTime - rKey0.fTime;
	float fInterpolationFraction = (fTime - rKey0.fTime) / fDelta;

	XMVECTOR vecStartValue = XMLoadFloat4(&rKey0.f4Value);
	XMVECTOR vecEndValue = XMLoadFloat4(&rKey1.f4Value);

	if (rChannel.uiTargetPath == common::AnimationChannel::kiTargetPathRotation)
	{
		return XMQuaternionSlerp(vecStartValue, vecEndValue, fInterpolationFraction);
	}

	return XMVectorLerp(vecStartValue, vecEndValue, fInterpolationFraction);
}

// Matrix convention: DirectXMath row-major storage, GLSL column-major interpretation
// When GLSL reads row-major bytes as column-major mat4, it naturally receives the transpose,
// which converts row-vector convention (v*M) to column-vector convention (M*v)
void AnimationData::EvaluateWorldMatrices(int64_t iAnimationIndex, float fTime, XMMATRIX* pmatWorldMatrices) const
{
	const common::AnimationClip& rAnimation = mpAnimations[iAnimationIndex];

	int64_t iVectorSize = mHeader.skeleton.uiNodeCount * static_cast<int64_t>(sizeof(XMVECTOR));
	int64_t iTotalSize = 3 * iVectorSize;

	auto bufferAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<std::byte*>(iTotalSize);
	std::byte* pBuffer = bufferAllocation.mpData;
	XMVECTOR* pvecTranslations = reinterpret_cast<XMVECTOR*>(pBuffer);
	XMVECTOR* pvecRotations    = reinterpret_cast<XMVECTOR*>(pBuffer + iVectorSize);
	XMVECTOR* pvecScales       = reinterpret_cast<XMVECTOR*>(pBuffer + 2 * iVectorSize);

	// Initialize node transforms from bind pose (only for animated nodes)
	const int64_t* piAnimatedNodes = mAnimatedNodes.data() + iAnimationIndex * mHeader.skeleton.uiNodeCount;
	for (int64_t i = 0; i < mHeader.skeleton.uiNodeCount; ++i)
	{
		if (piAnimatedNodes[i])
		{
			const common::ModelNode& rNode = mpNodes[i];
			pvecTranslations[i] = XMLoadFloat4(&rNode.f4BindTranslation);
			pvecRotations[i] = XMLoadFloat4(&rNode.f4BindRotation);
			pvecScales[i] = XMLoadFloat4(&rNode.f4BindScale);
		}
	}

	for (int64_t i = rAnimation.uiChannelStart; i < rAnimation.uiChannelStart + rAnimation.uiChannelCount; ++i)
	{
		const common::AnimationChannel& rChannel = mpChannels[i];
		XMVECTOR vecValue = InterpolateKeyframes(rChannel, fTime);

		switch (rChannel.uiTargetPath)
		{
			case common::AnimationChannel::kiTargetPathTranslation:
				pvecTranslations[rChannel.uiNodeIndex] = vecValue;
				break;
			case common::AnimationChannel::kiTargetPathRotation:
				pvecRotations[rChannel.uiNodeIndex] = vecValue;
				break;
			case common::AnimationChannel::kiTargetPathScale:
				pvecScales[rChannel.uiNodeIndex] = vecValue;
				break;
			default:
				ASSERT(false);
				std::unreachable();
		}
	}

	// Parent indices precede child indices, so each parent world matrix is ready before its child.
	for (int64_t i = 0; i < mHeader.skeleton.uiNodeCount; ++i)
	{
		XMMATRIX matLocal {};
		if (piAnimatedNodes[i])
		{
			const common::ModelNode& rNode = mpNodes[i];
			XMMATRIX matBindMatrix = XMLoadFloat4x4(&rNode.f4x4BindMatrix);
			XMMATRIX matScale = XMMatrixScalingFromVector(pvecScales[i]);
			XMMATRIX matRotation = XMMatrixRotationQuaternion(pvecRotations[i]);
			XMMATRIX matTranslation = XMMatrixTranslationFromVector(pvecTranslations[i]);
			// Combine: matrix * S * R * T (row-major; equivalent to T * R * S * matrix in column-major)
			matLocal = matBindMatrix * matScale * matRotation * matTranslation;
		}
		else
		{
			matLocal = mpBindPoseLocalMatrices[i];
		}

		int64_t iParent = mpNodes[i].iParentIndex;
		pmatWorldMatrices[i] = iParent >= 0 ? matLocal * pmatWorldMatrices[iParent] : matLocal;
	}
}

void AnimationData::EvaluateMaterial(int64_t iMaterialIndex, const XMMATRIX* pmatWorldMatrices, common::MeshData* pMeshData, common::JointMatrix* pJointMatrices, int64_t iJointMatrixOffset) const
{
	const common::MaterialInfo& rMaterialInfo = mpMaterialInfos[iMaterialIndex];

	// The shader skins only when the joint count is nonzero
	pMeshData->uiJointCount = (rMaterialInfo.flags & common::MaterialFlags::kSkinned) ? mHeader.skeleton.uiSkinJointCount : 0ui32;
	pMeshData->uiJointMatrixOffset = static_cast<uint32_t>(iJointMatrixOffset);

	XMMATRIX matMeshWorld = XMMatrixIdentity();
	if (rMaterialInfo.iParentNodeIndex >= 0)
	{
		XMMATRIX matRelative = mpAlignedRelativeTransforms[iMaterialIndex];
		matMeshWorld = matRelative * pmatWorldMatrices[rMaterialInfo.iParentNodeIndex];
	}

	XMStoreFloat4x4(&pMeshData->matrix, matMeshWorld);

	// Compute normal matrix: transpose(inverse(mat3(meshWorld)))
	XMMATRIX matNormal = XMMatrixTranspose(XMMatrixInverse(nullptr, matMeshWorld));
	XMStoreFloat4(&pMeshData->normalMatrix[0], matNormal.r[0]);
	XMStoreFloat4(&pMeshData->normalMatrix[1], matNormal.r[1]);
	XMStoreFloat4(&pMeshData->normalMatrix[2], matNormal.r[2]);

	if (rMaterialInfo.flags & common::MaterialFlags::kSkinned)
	{
		XMMATRIX matMeshWorldInverse = XMMatrixInverse(nullptr, matMeshWorld);

		for (int64_t i = 0; i < mHeader.skeleton.uiSkinJointCount; ++i)
		{
			int64_t iNodeIndex = mpuiSkinJointToNode[i];
			XMMATRIX matInverseBind = mpAlignedInverseBindMatrices[i];
			XMMATRIX matJoint = matInverseBind * pmatWorldMatrices[iNodeIndex] * matMeshWorldInverse;
			// Store 3 rows with translation packed into .w components:
			// r[0].w=0 -> Tx, r[1].w=0 -> Ty, r[2].w=0 -> Tz
			XMStoreFloat4(&pJointMatrices[iJointMatrixOffset + i].rows[0], matJoint.r[0]);
			XMStoreFloat4(&pJointMatrices[iJointMatrixOffset + i].rows[1], matJoint.r[1]);
			XMStoreFloat4(&pJointMatrices[iJointMatrixOffset + i].rows[2], matJoint.r[2]);
			XMFLOAT4 f4Translation {};
			XMStoreFloat4(&f4Translation, matJoint.r[3]);
			pJointMatrices[iJointMatrixOffset + i].rows[0].w = f4Translation.x;
			pJointMatrices[iJointMatrixOffset + i].rows[1].w = f4Translation.y;
			pJointMatrices[iJointMatrixOffset + i].rows[2].w = f4Translation.z;
		}
	}
}

void LoadAnimationDataFromEagerChunks()
{
	// Device loss recreates Graphics in place — the map and the pack memory it points into outlive Graphics
	if (!gAnimationDataMap.empty())
	{
		return;
	}

	for (const auto& [rCrc, rChunk] : gpFileManager->mpPackChunks->GetEagerChunkMap())
	{
		if (rChunk.pHeader->flags & common::ChunkFlags::kScene && rChunk.pHeader->sceneHeader.bHasAnimation)
		{
			// Animation data comes after scene arrays and material data (aligned to 16 bytes, matching export)
			int64_t iAnimationSectionOffset = common::SceneHeader::AnimationSectionOffset(rChunk.pHeader->sceneHeader.uiTextureCount, rChunk.pHeader->sceneHeader.uiMaterialCount);

			AnimationData& rAnimationData = gAnimationDataMap.try_emplace(rCrc).first->second;
			// Trust boundary: eager scene chunks are on-disk pack bytes parsed at boot. A corrupt animation
			// header count throws from Load; boot-required asset, so log kError and let it propagate to
			// MainThread's try/catch (HandleException — crash report + exit), matching the boot hard-fail tier.
			int64_t iAnimationBytes = rChunk.iDataSize - iAnimationSectionOffset;
			try
			{
				if (iAnimationBytes < 0)
				{
					throw std::ios_base::failure("AnimationData::Load");
				}
				rAnimationData.Load(std::span(rChunk.pData + iAnimationSectionOffset, static_cast<size_t>(iAnimationBytes)), rChunk.pHeader->sceneHeader.uiMaterialCount, rCrc);
			}
			catch (const std::ios_base::failure& rException)
			{
				char pcHex[20] {};
				LOG(kLoading, kError, "Corrupt animation data for GLTF CRC {}: {}", common::ToHex(std::span(pcHex), rCrc), rException.what());
				throw;
			}
			LOG(kLoading, kDebug, "Loaded animation data for GLTF CRC {}: {} nodes, {} skin joints, {} animations", rCrc, rAnimationData.mHeader.skeleton.uiNodeCount, rAnimationData.mHeader.skeleton.uiSkinJointCount, rAnimationData.mHeader.uiAnimationCount);
		}
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
