#include "SceneSkeletonLoader.h"

std::unordered_map<int64_t, int64_t> BuildNodeParentMap(const tinygltf::Model& rModel)
{
	std::unordered_map<int64_t, int64_t> parentMap;
	for (int64_t i = 0; i < std::ssize(rModel.nodes); ++i)
	{
		for (int64_t iChildIndex : rModel.nodes.at(i).children)
		{
			parentMap.insert_or_assign(iChildIndex, i);
		}
	}
	return parentMap;
}

void CanonicalizeSceneSkin(tinygltf::Model& rModel)
{
	std::unordered_set<int64_t> referencedSkins;
	for (const tinygltf::Node& rNode : rModel.nodes)
	{
		if (rNode.skin < 0)
		{
			continue;
		}

		// tinygltf parses "skin" without range checking, and the glTF file is untrusted input
		if (rNode.skin >= std::ssize(rModel.skins))
		{
			throw std::runtime_error(std::format("ExportScene node references skin {}, but the model declares only {} skins", rNode.skin, std::ssize(rModel.skins)));
		}

		referencedSkins.insert(rNode.skin);
	}

	if (referencedSkins.size() > 1)
	{
		throw std::runtime_error(std::format("ExportScene references {} distinct glTF skins; the pack format carries one skeleton per scene", referencedSkins.size()));
	}

	if (referencedSkins.empty())
	{
		return;
	}

	int64_t iReferencedSkin = *referencedSkins.begin();
	if (iReferencedSkin == 0)
	{
		return;
	}

	std::swap(rModel.skins.at(0), rModel.skins.at(static_cast<size_t>(iReferencedSkin)));
	for (tinygltf::Node& rNode : rModel.nodes)
	{
		// No other node references a skin, so nothing maps onto the displaced slot
		if (rNode.skin == iReferencedSkin)
		{
			rNode.skin = 0;
		}
	}
}

SkeletonData LoadSkeletonData(const tinygltf::Model& rModel)
{
	LOG(kDefault, kDebug, "LoadSkeletonData: Loading all nodes...");

	std::unordered_map<int64_t, int64_t> parentMap = BuildNodeParentMap(rModel);

	SkeletonData skeletonData;
	skeletonData.skeleton.uiNodeCount = static_cast<uint16_t>(std::ssize(rModel.nodes));
	ASSERT(std::in_range<int16_t>(std::ssize(rModel.nodes)));

	// CanonicalizeSceneSkin runs before export so any node-referenced skin occupies slot 0.
	if (!rModel.skins.empty())
	{
		const tinygltf::Skin& rSkin = rModel.skins.at(0);
		skeletonData.skeleton.uiSkinJointCount = static_cast<uint16_t>(std::ssize(rSkin.joints));
		ASSERT(std::ssize(rSkin.joints) <= skeletonData.skeleton.uiNodeCount);

		skeletonData.skinJointToNode.resize(static_cast<size_t>(std::ssize(rSkin.joints)));
		for (int64_t i = 0; i < std::ssize(rSkin.joints); ++i)
		{
			skeletonData.skinJointToNode.at(i) = static_cast<uint16_t>(rSkin.joints.at(i));
		}

		const float* pfInverseBindMatrices = nullptr;
		if (rSkin.inverseBindMatrices != -1)
		{
			if (rSkin.inverseBindMatrices < 0)
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind accessor {} is outside the model's {} accessors", rSkin.inverseBindMatrices, std::ssize(rModel.accessors)));
			}
			if (rSkin.inverseBindMatrices >= std::ssize(rModel.accessors))
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind accessor {} is outside the model's {} accessors", rSkin.inverseBindMatrices, std::ssize(rModel.accessors)));
			}

			const tinygltf::Accessor& rAccessor = rModel.accessors.at(rSkin.inverseBindMatrices);
			if (rAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor must contain FLOAT MAT4 elements");
			}
			if (rAccessor.type != TINYGLTF_TYPE_MAT4)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor must contain FLOAT MAT4 elements");
			}
			if (rAccessor.count < rSkin.joints.size())
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind accessor contains {} matrices for {} skin joints", rAccessor.count, std::ssize(rSkin.joints)));
			}
			if (rAccessor.sparse.isSparse)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor cannot be sparse");
			}
			if (rAccessor.bufferView < 0)
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind accessor references buffer view {}, but the model declares only {} buffer views", rAccessor.bufferView, std::ssize(rModel.bufferViews)));
			}
			if (rAccessor.bufferView >= std::ssize(rModel.bufferViews))
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind accessor references buffer view {}, but the model declares only {} buffer views", rAccessor.bufferView, std::ssize(rModel.bufferViews)));
			}

			const tinygltf::BufferView& rBufferView = rModel.bufferViews.at(rAccessor.bufferView);
			if (rBufferView.buffer < 0)
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind buffer view references buffer {}, but the model declares only {} buffers", rBufferView.buffer, std::ssize(rModel.buffers)));
			}
			if (rBufferView.buffer >= std::ssize(rModel.buffers))
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind buffer view references buffer {}, but the model declares only {} buffers", rBufferView.buffer, std::ssize(rModel.buffers)));
			}
			if (rBufferView.byteStride != 0)
			{
				throw std::runtime_error(std::format("ExportScene inverse-bind accessor requires packed matrices, but its buffer view has byte stride {}", rBufferView.byteStride));
			}

			static constexpr size_t kuiMatrixSize = 16 * sizeof(float);
			if (rAccessor.count > std::numeric_limits<size_t>::max() / kuiMatrixSize)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor byte span overflows");
			}
			size_t uiAccessorSpan = rAccessor.count * kuiMatrixSize;
			if (rAccessor.byteOffset > rBufferView.byteLength)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor byte span exceeds its buffer view");
			}
			if (uiAccessorSpan > rBufferView.byteLength - rAccessor.byteOffset)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor byte span exceeds its buffer view");
			}

			const tinygltf::Buffer& rBuffer = rModel.buffers.at(rBufferView.buffer);
			if (rBufferView.byteOffset > rBuffer.data.size())
			{
				throw std::runtime_error("ExportScene inverse-bind buffer view byte span exceeds its buffer");
			}
			if (rBufferView.byteLength > rBuffer.data.size() - rBufferView.byteOffset)
			{
				throw std::runtime_error("ExportScene inverse-bind buffer view byte span exceeds its buffer");
			}
			size_t uiDataOffset = rBufferView.byteOffset + rAccessor.byteOffset;
			if (uiDataOffset % alignof(float) != 0)
			{
				throw std::runtime_error("ExportScene inverse-bind accessor data is not float-aligned");
			}

			if (uiAccessorSpan != 0)
			{
				pfInverseBindMatrices = reinterpret_cast<const float*>(rBuffer.data.data() + uiDataOffset);
			}
		}

		skeletonData.inverseBindMatrices.resize(static_cast<size_t>(std::ssize(rSkin.joints)));
		for (int64_t i = 0; i < std::ssize(rSkin.joints); ++i)
		{
			if (pfInverseBindMatrices != nullptr)
			{
				const float* pMatrix = &pfInverseBindMatrices[i * 16];
				XMMATRIX matInverseBind = XMMATRIX(pMatrix);
				XMStoreFloat4x4(&skeletonData.inverseBindMatrices.at(i), matInverseBind);
			}
			else
			{
				XMStoreFloat4x4(&skeletonData.inverseBindMatrices.at(i), XMMatrixIdentity());
			}
		}

		LOG(kDefault, kDebug, "LoadSkeletonData: Loaded {} skin joints from skin", skeletonData.skeleton.uiSkinJointCount);
	}
	else
	{
		skeletonData.skeleton.uiSkinJointCount = 0;
	}

	LOG(kDefault, kDebug, "  Total nodes: {}", skeletonData.skeleton.uiNodeCount);

	skeletonData.nodes.resize(static_cast<size_t>(std::ssize(rModel.nodes)));
	for (int64_t i = 0; i < std::ssize(rModel.nodes); ++i)
	{
		common::ModelNode& rNode = skeletonData.nodes.at(i);
		const tinygltf::Node& rGltfNode = rModel.nodes.at(i);

		rNode.iParentIndex = -1;
		auto parentIt = parentMap.find(i);
		if (parentIt != parentMap.end())
		{
			// Nodes ship in source order and the runtime builds world matrices in one forward pass, so a parent
			// that follows its child would only be caught at load (AnimationData::Load, std::ios_base::failure).
			if (parentIt->second >= i)
			{
				throw std::runtime_error(std::format("ExportScene node {} has parent node {}, which does not precede it; glTF nodes must be ordered parent-before-child", i, parentIt->second));
			}

			rNode.iParentIndex = static_cast<int16_t>(parentIt->second);
		}

		if (std::ssize(rGltfNode.translation) == 3)
		{
			rNode.f4BindTranslation = XMFLOAT4(static_cast<float>(rGltfNode.translation.at(0)), static_cast<float>(rGltfNode.translation.at(1)), static_cast<float>(rGltfNode.translation.at(2)), 0.0f);
		}
		else
		{
			rNode.f4BindTranslation = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		}

		if (std::ssize(rGltfNode.rotation) == 4)
		{
			rNode.f4BindRotation = XMFLOAT4(static_cast<float>(rGltfNode.rotation.at(0)), static_cast<float>(rGltfNode.rotation.at(1)), static_cast<float>(rGltfNode.rotation.at(2)), static_cast<float>(rGltfNode.rotation.at(3)));
		}
		else
		{
			rNode.f4BindRotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		}

		if (std::ssize(rGltfNode.scale) == 3)
		{
			rNode.f4BindScale = XMFLOAT4(static_cast<float>(rGltfNode.scale.at(0)), static_cast<float>(rGltfNode.scale.at(1)), static_cast<float>(rGltfNode.scale.at(2)), 1.0f);
		}
		else
		{
			rNode.f4BindScale = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		if (std::ssize(rGltfNode.matrix) == 16)
		{
			// glTF stores matrices in column-major order, DirectXMath uses row-major
			// Loading column-major data as row-major puts translation into row 3, which is correct for DirectXMath
			XMMATRIX matNode = XMMATRIX(static_cast<float>(rGltfNode.matrix.at(0)), static_cast<float>(rGltfNode.matrix.at(1)), static_cast<float>(rGltfNode.matrix.at(2)), static_cast<float>(rGltfNode.matrix.at(3)), static_cast<float>(rGltfNode.matrix.at(4)), static_cast<float>(rGltfNode.matrix.at(5)), static_cast<float>(rGltfNode.matrix.at(6)), static_cast<float>(rGltfNode.matrix.at(7)), static_cast<float>(rGltfNode.matrix.at(8)), static_cast<float>(rGltfNode.matrix.at(9)), static_cast<float>(rGltfNode.matrix.at(10)), static_cast<float>(rGltfNode.matrix.at(11)), static_cast<float>(rGltfNode.matrix.at(12)), static_cast<float>(rGltfNode.matrix.at(13)), static_cast<float>(rGltfNode.matrix.at(14)), static_cast<float>(rGltfNode.matrix.at(15)));
			XMStoreFloat4x4(&rNode.f4x4BindMatrix, matNode);
		}
		else
		{
			XMStoreFloat4x4(&rNode.f4x4BindMatrix, XMMatrixIdentity());
		}
	}

	return skeletonData;
}
