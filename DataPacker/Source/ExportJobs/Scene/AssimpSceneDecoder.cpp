#include "AssimpSceneDecoder.h"

#include "ExportJobs/SourceReadValidation.h"
#include "SceneAnimationLoader.h"
#include "SceneVerticesLoader.h"

// Assimp owns the adapter; the dependency vector outlives its importer.
class SceneIOSystem : public Assimp::IOSystem
{
public:

	explicit SceneIOSystem(std::vector<std::filesystem::path>& rDependencies) : mrDependencies(rDependencies)
	{
	}
	bool Exists(const char* pcFile) const override
	{
		return mDefault.Exists(pcFile);
	}
	char getOsSeparator() const override
	{
		return mDefault.getOsSeparator();
	}
	Assimp::IOStream* Open(const char* pcFile, const char* pcMode) override
	{
		Assimp::IOStream* pStream = mDefault.Open(pcFile, pcMode);
		if (pStream != nullptr)
		{
			mrDependencies.push_back(std::filesystem::weakly_canonical(pcFile));
		}
		else if (mFailedRead.empty())
		{
			mFailedRead = pcFile;
		}
		return pStream;
	}
	void Close(Assimp::IOStream* pStream) override
	{
		mDefault.Close(pStream);
	}
	bool ComparePaths(const char* pcFirst, const char* pcSecond) const override
	{
		return mDefault.ComparePaths(pcFirst, pcSecond);
	}
	void ValidateReads() const
	{
		if (!mFailedRead.empty())
		{
			throw std::runtime_error(std::format("Assimp failed to open scene dependency '{}'", mFailedRead));
		}
	}

private:

	Assimp::DefaultIOSystem mDefault;
	std::vector<std::filesystem::path>& mrDependencies;
	std::string mFailedRead;
};

static XMMATRIX AssimpMatrix(const aiMatrix4x4& rMatrix)
{
	return XMMATRIX(rMatrix.a1, rMatrix.b1, rMatrix.c1, rMatrix.d1, rMatrix.a2, rMatrix.b2, rMatrix.c2, rMatrix.d2, rMatrix.a3, rMatrix.b3, rMatrix.c3, rMatrix.d3, rMatrix.a4, rMatrix.b4, rMatrix.c4, rMatrix.d4);
}

static bool IsFiniteVector(FXMVECTOR vecValue)
{
	return !XMVector3IsNaN(vecValue) && !XMVector3IsInfinite(vecValue);
}

static SceneImage LoadAssimpImage(const aiScene& rScene, const std::filesystem::path& rSource, const aiString& rName, std::vector<std::filesystem::path>& rDependencies)
{
	SceneImage image;
	image.uri = rName.C_Str();
	image.iComponents = 4;
	const aiTexture* pEmbedded = rScene.GetEmbeddedTexture(rName.C_Str());
	if (pEmbedded != nullptr && pEmbedded->mHeight != 0)
	{
		image.iWidth = pEmbedded->mWidth;
		image.iHeight = pEmbedded->mHeight;
		uintmax_t uiBytes = MultiplySourceBytes(MultiplySourceBytes(pEmbedded->mWidth, pEmbedded->mHeight, "Assimp image"), 4, "Assimp image");
		image.pixels.resize(static_cast<size_t>(uiBytes));
		for (int64_t i = 0; i < image.iWidth * image.iHeight; ++i)
		{
			const aiTexel& rTexel = pEmbedded->pcData[i];
			image.pixels.at(4 * i) = rTexel.r;
			image.pixels.at(4 * i + 1) = rTexel.g;
			image.pixels.at(4 * i + 2) = rTexel.b;
			image.pixels.at(4 * i + 3) = rTexel.a;
		}
		return image;
	}

	int iWidth = 0;
	int iHeight = 0;
	int iComponents = 0;
	stbi_uc* puiPixels = nullptr;
	if (pEmbedded != nullptr)
	{
		ASSERT(std::in_range<int>(pEmbedded->mWidth));
		puiPixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(pEmbedded->pcData), static_cast<int>(pEmbedded->mWidth), &iWidth, &iHeight, &iComponents, STBI_rgb_alpha);
	}
	else
	{
		std::filesystem::path imagePath = std::filesystem::weakly_canonical(rSource.parent_path() / std::filesystem::path(rName.C_Str()));
		puiPixels = stbi_load(reinterpret_cast<const char*>(imagePath.u8string().c_str()), &iWidth, &iHeight, &iComponents, STBI_rgb_alpha);
		if (puiPixels != nullptr)
		{
			rDependencies.push_back(std::move(imagePath));
		}
	}
	common::ScopedLambda freePixels([&]() { stbi_image_free(puiPixels); });
	if (puiPixels == nullptr)
	{
		throw std::runtime_error(std::format("Assimp failed to decode image '{}'", rName.C_Str()));
	}
	if (iWidth <= 0)
	{
		throw std::runtime_error(std::format("Assimp failed to decode image '{}'", rName.C_Str()));
	}
	if (iHeight <= 0)
	{
		throw std::runtime_error(std::format("Assimp failed to decode image '{}'", rName.C_Str()));
	}
	image.iWidth = iWidth;
	image.iHeight = iHeight;
	uintmax_t uiBytes = MultiplySourceBytes(MultiplySourceBytes(iWidth, iHeight, "Assimp image"), 4, "Assimp image");
	image.pixels.assign(puiPixels, puiPixels + uiBytes);
	return image;
}

static void LoadAssimpTexture(const aiScene& rScene, const aiMaterial& rMaterial, aiTextureType eType, VkFormat vkFormat, uint8_t& ruiIndex, int32_t& riSet, const std::filesystem::path& rPath, SceneExportData& rData, std::vector<std::filesystem::path>& rDependencies)
{
	ASSERT(rMaterial.GetTextureCount(eType) <= 1);
	if (rMaterial.GetTextureCount(eType) == 0)
	{
		return;
	}
	aiString name;
	aiTextureMapping eMapping = aiTextureMapping_UV;
	unsigned int uiUv = 0;
	ai_real fBlend = 1.0f;
	aiTextureOp eOperation = aiTextureOp_Multiply;
	aiTextureMapMode peModes[2] {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, };
	ASSERT(rMaterial.GetTexture(eType, 0, &name, &eMapping, &uiUv, &fBlend, &eOperation, peModes) == AI_SUCCESS);
	ASSERT(eMapping == aiTextureMapping_UV && uiUv < 5 && fBlend == 1.0f && eOperation == aiTextureOp_Multiply);
	ASSERT(peModes[0] == aiTextureMapMode_Wrap && peModes[1] == aiTextureMapMode_Wrap);
	aiUVTransform uvTransform;
	if (rMaterial.Get(AI_MATKEY_UVTRANSFORM(eType, 0), uvTransform) == AI_SUCCESS)
	{
		ASSERT(uvTransform.mTranslation == aiVector2D(0.0f, 0.0f) && uvTransform.mScaling == aiVector2D(1.0f, 1.0f) && uvTransform.mRotation == 0.0f);
	}
	int64_t iSource = 0;
	for (; iSource < std::ssize(rData.images); ++iSource)
	{
		if (rData.images.at(iSource).uri == name.C_Str())
		{
			break;
		}
	}
	if (iSource == std::ssize(rData.images))
	{
		rData.images.push_back(LoadAssimpImage(rScene, rPath, name, rDependencies));
	}
	int64_t iTexture = 0;
	for (; iTexture < std::ssize(rData.textures); ++iTexture)
	{
		if (rData.textures.at(iTexture).iSource == iSource && rData.textures.at(iTexture).vkFormat == vkFormat)
		{
			break;
		}
	}
	if (iTexture == std::ssize(rData.textures))
	{
		ASSERT(iTexture < common::SceneHeader::kiMaxTextures);
		rData.textures.push_back({.iSource = iSource, .vkFormat = vkFormat, });
	}
	ruiIndex = static_cast<uint8_t>(iTexture);
	riSet = static_cast<int32_t>(uiUv);
}

static common::MaterialShaderData LoadAssimpMaterial(const aiScene& rScene, const aiMaterial& rMaterial, const std::filesystem::path& rPath, SceneExportData& rData, std::vector<std::filesystem::path>& rDependencies)
{
	common::MaterialShaderData data;
	data.f3EmissiveFactor = XMFLOAT3(0.0f, 0.0f, 0.0f);
	data.fMetallicFactor = 0.0f;
	aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
	if (rMaterial.Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
	{
		std::ignore = rMaterial.Get(AI_MATKEY_COLOR_DIFFUSE, color);
	}
	data.f4BaseColorFactor = XMFLOAT4(color.r, color.g, color.b, color.a);
	aiColor3D emissive(0.0f, 0.0f, 0.0f);
	std::ignore = rMaterial.Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
	data.f3EmissiveFactor = XMFLOAT3(emissive.r, emissive.g, emissive.b);
	std::ignore = rMaterial.Get(AI_MATKEY_METALLIC_FACTOR, data.fMetallicFactor);
	std::ignore = rMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, data.fRoughnessFactor);
	ai_real fOpacity = 1.0f;
	std::ignore = rMaterial.Get(AI_MATKEY_OPACITY, fOpacity);
	data.f4BaseColorFactor.w *= fOpacity;
	if (data.f4BaseColorFactor.w < 1.0f)
	{
		data.fAlphaMask = 2.0f;
	}
	aiColor3D specular(0.0f, 0.0f, 0.0f);
	std::ignore = rMaterial.Get(AI_MATKEY_COLOR_SPECULAR, specular);
	int iShading = aiShadingMode_Gouraud;
	std::ignore = rMaterial.Get(AI_MATKEY_SHADING_MODEL, iShading);
	ASSERT(iShading == aiShadingMode_Flat || iShading == aiShadingMode_Gouraud || iShading == aiShadingMode_Phong || iShading == aiShadingMode_PBR_BRDF);
	if (iShading == aiShadingMode_Phong)
	{
		ai_real fSpecularStrength = 1.0f;
		std::ignore = rMaterial.Get(AI_MATKEY_SHININESS_STRENGTH, fSpecularStrength);
		// A nonzero legacy specular lobe cannot be represented without approximation.
		ASSERT(specular == aiColor3D(0.0f, 0.0f, 0.0f) || fSpecularStrength == 0.0f);
	}
	if (iShading == aiShadingMode_PBR_BRDF)
	{
		ASSERT(specular == aiColor3D(0.0f, 0.0f, 0.0f));
	}
	int iWireframe = 0;
	std::ignore = rMaterial.Get(AI_MATKEY_ENABLE_WIREFRAME, iWireframe);
	ASSERT(iWireframe == 0);
	int iTwoSided = 0;
	std::ignore = rMaterial.Get(AI_MATKEY_TWOSIDED, iTwoSided);
	// PLY synthesizes two-sided polygon materials; imported geometry remains one-sided.
	ASSERT(rPath.extension() == ".ply" || iTwoSided == 0);
	for (int64_t i = 1; i <= AI_TEXTURE_TYPE_MAX; ++i)
	{
		aiTextureType eType = static_cast<aiTextureType>(i);
		bool bSupported = eType == aiTextureType_DIFFUSE || eType == aiTextureType_BASE_COLOR || eType == aiTextureType_NORMALS
			|| eType == aiTextureType_EMISSIVE || eType == aiTextureType_EMISSION_COLOR || eType == aiTextureType_AMBIENT_OCCLUSION
			|| eType == aiTextureType_LIGHTMAP || eType == aiTextureType_GLTF_METALLIC_ROUGHNESS;
		ASSERT(bSupported || rMaterial.GetTextureCount(eType) == 0);
	}
	aiColor3D sheen(0.0f, 0.0f, 0.0f);
	std::ignore = rMaterial.Get(AI_MATKEY_SHEEN_COLOR_FACTOR, sheen);
	ASSERT(sheen == aiColor3D(0.0f, 0.0f, 0.0f));
	for (const char* pcFeature : {"$mat.clearcoat.factor", "$mat.transmission.factor", "$mat.anisotropyFactor", "$mat.volume.thicknessFactor", "$mat.reflectivity", })
	{
		ai_real fValue = 0.0f;
		std::ignore = rMaterial.Get(pcFeature, 0, 0, fValue);
		ASSERT(fValue == 0.0f);
	}
	ASSERT(rMaterial.GetTextureCount(aiTextureType_BASE_COLOR) == 0 || rMaterial.GetTextureCount(aiTextureType_DIFFUSE) == 0);
	ASSERT(rMaterial.GetTextureCount(aiTextureType_EMISSION_COLOR) == 0 || rMaterial.GetTextureCount(aiTextureType_EMISSIVE) == 0);
	ASSERT(rMaterial.GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) == 0 || rMaterial.GetTextureCount(aiTextureType_LIGHTMAP) == 0);
	LoadAssimpTexture(rScene, rMaterial, rMaterial.GetTextureCount(aiTextureType_BASE_COLOR) != 0 ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE, VK_FORMAT_BC7_UNORM_BLOCK, data.uiColorTextureIndex, data.iColorTextureSet, rPath, rData, rDependencies);
	LoadAssimpTexture(rScene, rMaterial, aiTextureType_GLTF_METALLIC_ROUGHNESS, VK_FORMAT_BC7_UNORM_BLOCK, data.uiPhysicalDescriptorTextureIndex, data.iPhysicalDescriptorTextureSet, rPath, rData, rDependencies);
	LoadAssimpTexture(rScene, rMaterial, aiTextureType_NORMALS, VK_FORMAT_BC5_UNORM_BLOCK, data.uiNormalTextureIndex, data.iNormalTextureSet, rPath, rData, rDependencies);
	LoadAssimpTexture(rScene, rMaterial, rMaterial.GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) != 0 ? aiTextureType_AMBIENT_OCCLUSION : aiTextureType_LIGHTMAP, VK_FORMAT_BC4_UNORM_BLOCK, data.uiOcclusionTextureIndex, data.iOcclusionTextureSet, rPath, rData, rDependencies);
	LoadAssimpTexture(rScene, rMaterial, rMaterial.GetTextureCount(aiTextureType_EMISSION_COLOR) != 0 ? aiTextureType_EMISSION_COLOR : aiTextureType_EMISSIVE, VK_FORMAT_BC7_UNORM_BLOCK, data.uiEmissiveTextureIndex, data.iEmissiveTextureSet, rPath, rData, rDependencies);
	for (float fValue : {color.r, color.g, color.b, color.a, emissive.r, emissive.g, emissive.b, fOpacity, data.fMetallicFactor, data.fRoughnessFactor, })
	{
		ASSERT(std::isfinite(fValue));
	}
	return data;
}

static bool AssimpMatricesEqual(const XMMATRIX& rFirst, const XMMATRIX& rSecond)
{
	for (int64_t i = 0; i < 4; ++i)
	{
		XMVECTOR vecTolerance = XMVectorScale(XMVectorMax(XMVectorAbs(rFirst.r[i]), XMVectorReplicate(1.0f)), 1.0e-5f);
		if (!XMVector4NearEqual(rFirst.r[i], rSecond.r[i], vecTolerance))
		{
			return false;
		}
	}
	return true;
}

static void ValidateAssimpMatrix(const XMMATRIX& rMatrix)
{
	for (int64_t i = 0; i < 4; ++i)
	{
		ASSERT(!XMVector4IsNaN(rMatrix.r[i]) && !XMVector4IsInfinite(rMatrix.r[i]));
	}
	ASSERT(XMVectorGetW(rMatrix.r[0]) == 0.0f && XMVectorGetW(rMatrix.r[1]) == 0.0f && XMVectorGetW(rMatrix.r[2]) == 0.0f && XMVectorGetW(rMatrix.r[3]) == 1.0f);
	float fDeterminant = XMVectorGetX(XMMatrixDeterminant(rMatrix));
	ASSERT(std::isfinite(fDeterminant) && fDeterminant != 0.0f);
}

static common::ModelNode AssimpModelNode(const XMMATRIX& rLocal, int64_t iParent)
{
	ValidateAssimpMatrix(rLocal);
	common::ModelNode node {};
	node.iParentIndex = static_cast<int16_t>(iParent);
	node.f4BindRotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	node.f4BindScale = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMStoreFloat4x4(&node.f4x4BindMatrix, rLocal);
	return node;
}

static void LoadAssimpHierarchy(const aiNode& rNode, int64_t iParent, const std::unordered_set<std::string>& rAnimatedNames, SceneExportData& rData, std::unordered_map<std::string, int64_t>& rNodeNames, std::vector<XMMATRIX>& rWorldMatrices)
{
	int64_t iNode = std::ssize(rData.skeletonData.nodes);
	ASSERT(iNode < std::numeric_limits<int16_t>::max());
	XMMATRIX matLocal = AssimpMatrix(rNode.mTransformation);
	common::ModelNode node = AssimpModelNode(matLocal, iParent);
	if (rAnimatedNames.contains(rNode.mName.C_Str()))
	{
		aiVector3D scale;
		aiQuaternion rotation;
		aiVector3D translation;
		rNode.mTransformation.Decompose(scale, rotation, translation);
		XMVECTOR vecScale = XMVectorSet(scale.x, scale.y, scale.z, 1.0f);
		XMVECTOR vecRotation = XMVectorSet(rotation.x, rotation.y, rotation.z, rotation.w);
		XMVECTOR vecTranslation = XMVectorSet(translation.x, translation.y, translation.z, 0.0f);
		ASSERT(!XMVector4IsNaN(vecRotation) && !XMVector4IsInfinite(vecRotation) && std::abs(XMVectorGetX(XMVector4LengthSq(vecRotation)) - 1.0f) <= 1.0e-5f);
		XMMATRIX matReconstructed = XMMatrixScalingFromVector(vecScale) * XMMatrixRotationQuaternion(vecRotation) * XMMatrixTranslationFromVector(vecTranslation);
		ASSERT(AssimpMatricesEqual(matLocal, matReconstructed));
		XMStoreFloat4(&node.f4BindScale, vecScale);
		XMStoreFloat4(&node.f4BindRotation, vecRotation);
		XMStoreFloat4(&node.f4BindTranslation, vecTranslation);
		XMStoreFloat4x4(&node.f4x4BindMatrix, XMMatrixIdentity());
	}
	rData.skeletonData.nodes.push_back(node);
	rWorldMatrices.push_back(matLocal * rWorldMatrices.at(iParent));
	auto [it, bInserted] = rNodeNames.try_emplace(rNode.mName.C_Str(), iNode);
	if (!bInserted)
	{
		it->second = -1;
	}
	for (int64_t i = 0; i < rNode.mNumChildren; ++i)
	{
		LoadAssimpHierarchy(*rNode.mChildren[i], iNode, rAnimatedNames, rData, rNodeNames, rWorldMatrices);
	}
}

static int64_t ResolveAssimpNode(const aiString& rName, const std::unordered_map<std::string, int64_t>& rNodeNames)
{
	auto it = rNodeNames.find(rName.C_Str());
	ASSERT(it != rNodeNames.end() && it->second >= 0);
	return it->second;
}

static XMFLOAT4A AssimpAnimationValue(const aiVector3D& rValue)
{
	return XMFLOAT4A(rValue.x, rValue.y, rValue.z, 0.0f);
}

static XMFLOAT4A AssimpAnimationValue(const aiQuaternion& rValue)
{
	return XMFLOAT4A(rValue.x, rValue.y, rValue.z, rValue.w);
}

static bool AssimpAnimationValuesEqual(const XMFLOAT4& rFirst, const XMFLOAT4& rSecond, bool bRotation, float fTolerance)
{
	XMVECTOR vecFirst = XMLoadFloat4(&rFirst);
	XMVECTOR vecSecond = XMLoadFloat4(&rSecond);
	return XMVector4NearEqual(vecFirst, vecSecond, XMVectorReplicate(fTolerance))
		|| (bRotation && XMVector4NearEqual(vecFirst, XMVectorNegate(vecSecond), XMVectorReplicate(fTolerance)));
}

template <typename KEY>
static void LoadAssimpComponent(std::span<const KEY> keys, int64_t iTarget, int64_t iNode, const XMFLOAT4& rDefault, const aiNodeAnim& rSourceChannel, const aiAnimation& rSourceAnimation, common::AnimationClip& rAnimation, SceneExportData& rData)
{
	if (keys.empty())
	{
		return;
	}
	ASSERT(std::ssize(keys) <= common::kiMaxDeserializedCapacity - std::ssize(rData.keyframes));
	ASSERT(std::ssize(rData.channels) < common::kiMaxDeserializedCapacity);
	bool bRotation = iTarget == common::AnimationChannel::kiTargetPathRotation;
	int64_t iStride = bRotation ? 4 : 3;
	std::vector<float> times;
	std::vector<float> values;
	times.reserve(keys.size());
	values.reserve(keys.size() * static_cast<size_t>(iStride));
	aiAnimInterpolation eInterpolation = keys.front().mInterpolation;
	bool bConstant = true;
	XMFLOAT4A f4First = AssimpAnimationValue(keys.front().mValue);
	for (int64_t i = 0; i < std::ssize(keys); ++i)
	{
		double fSeconds = keys[i].mTime / rSourceAnimation.mTicksPerSecond;
		float fTime = static_cast<float>(fSeconds);
		ASSERT(std::isfinite(fSeconds) && std::isfinite(fTime));
		ASSERT(i == 0 || (keys[i].mTime > keys[i - 1].mTime && fTime > times.back()));
		if (i + 1 < std::ssize(keys))
		{
			ASSERT(keys[i].mInterpolation == eInterpolation);
			// Assimp's ordinary quaternion Interpolator uses shortest-path slerp.
			ASSERT(eInterpolation == aiAnimInterpolation_Step || eInterpolation == aiAnimInterpolation_Linear || (bRotation && eInterpolation == aiAnimInterpolation_Spherical_Linear));
		}
		XMFLOAT4A f4Value = AssimpAnimationValue(keys[i].mValue);
		XMVECTOR vecValue = XMLoadFloat4A(&f4Value);
		ASSERT(!XMVector4IsNaN(vecValue) && !XMVector4IsInfinite(vecValue));
		if (bRotation)
		{
			ASSERT(std::abs(XMVectorGetX(XMVector4LengthSq(vecValue)) - 1.0f) <= 1.0e-5f);
		}
		bConstant = bConstant && AssimpAnimationValuesEqual(f4Value, f4First, bRotation, 0.0f);
		times.push_back(fTime);
		values.push_back(f4Value.x);
		values.push_back(f4Value.y);
		values.push_back(f4Value.z);
		if (bRotation)
		{
			values.push_back(f4Value.w);
		}
	}
	if (iTarget == common::AnimationChannel::kiTargetPathScale)
	{
		float fDuration = static_cast<float>(rSourceAnimation.mDuration / rSourceAnimation.mTicksPerSecond);
		for (int64_t i = 0; i < std::ssize(keys); ++i)
		{
			for (int64_t j = 0; j < 3; ++j)
			{
				double fValue = values.at(3 * i + j);
				ASSERT(fValue != 0.0);
				if (i == 0)
				{
					continue;
				}
				if (eInterpolation == aiAnimInterpolation_Step)
				{
					continue;
				}
				double fPrevious = values.at(3 * (i - 1) + j);
				if ((fPrevious < 0.0) != (fValue < 0.0))
				{
					// Mesh-world inversion requires nonzero scale throughout the played interval.
					double fZeroTime = times.at(i - 1) + (static_cast<double>(times.at(i)) - times.at(i - 1)) * (-fPrevious / (fValue - fPrevious));
					ASSERT(fZeroTime < 0.0 || fZeroTime > fDuration);
				}
			}
			if (i == 0)
			{
				continue;
			}
			if (eInterpolation == aiAnimInterpolation_Step)
			{
				continue;
			}
			for (float fBoundaryTime : {0.0f, fDuration})
			{
				if (fBoundaryTime <= times.front() || fBoundaryTime >= times.back() || fBoundaryTime < times.at(i - 1) || fBoundaryTime >= times.at(i))
				{
					continue;
				}
				float fDelta = times.at(i) - times.at(i - 1);
				float fInterpolationFraction = (fBoundaryTime - times.at(i - 1)) / fDelta;
				XMFLOAT4A f4Start(values.at(3 * (i - 1)), values.at(3 * (i - 1) + 1), values.at(3 * (i - 1) + 2), 0.0f);
				XMFLOAT4A f4End(values.at(3 * i), values.at(3 * i + 1), values.at(3 * i + 2), 0.0f);
				XMFLOAT4A f4Boundary {};
				XMStoreFloat4A(&f4Boundary, XMVectorLerp(XMLoadFloat4A(&f4Start), XMLoadFloat4A(&f4End), fInterpolationFraction));
				ASSERT(f4Boundary.x != 0.0f && f4Boundary.y != 0.0f && f4Boundary.z != 0.0f);
			}
		}
	}
	// The packed clip plays over [0, duration]; only reached boundary behavior matters.
	auto ValidateBoundary = [&](aiAnimBehaviour eBehavior, int64_t iEndpoint, int64_t iNeighbor)
	{
		XMFLOAT4A f4Endpoint = AssimpAnimationValue(keys[iEndpoint].mValue);
		switch (eBehavior)
		{
			case aiAnimBehaviour_CONSTANT:
				return;
			case aiAnimBehaviour_DEFAULT:
				ASSERT(AssimpAnimationValuesEqual(f4Endpoint, rDefault, bRotation, 1.0e-5f));
				return;
			case aiAnimBehaviour_LINEAR:
				ASSERT(AssimpAnimationValuesEqual(f4Endpoint, AssimpAnimationValue(keys[iNeighbor].mValue), bRotation, 0.0f));
				return;
			case aiAnimBehaviour_REPEAT:
				ASSERT(bConstant);
				return;
			default:
				ASSERT(false);
		}
	};
	if (keys.front().mTime > 0.0)
	{
		ValidateBoundary(rSourceChannel.mPreState, 0, std::min<int64_t>(1, std::ssize(keys) - 1));
	}
	if (keys.back().mTime < rSourceAnimation.mDuration)
	{
		ValidateBoundary(rSourceChannel.mPostState, std::ssize(keys) - 1, std::max<int64_t>(0, std::ssize(keys) - 2));
	}
	common::AnimationChannel channel {};
	channel.uiNodeIndex = static_cast<uint16_t>(iNode);
	channel.uiTargetPath = static_cast<uint8_t>(iTarget);
	channel.uiInterpolation = static_cast<uint8_t>(eInterpolation == aiAnimInterpolation_Step ? common::AnimationChannel::kiInterpolationStep : common::AnimationChannel::kiInterpolationLinear);
	channel.uiKeyframeCount = static_cast<uint32_t>(keys.size());
	EmitKeyframes(times, values, iStride, channel, rAnimation.fDuration, rData.keyframes);
	AppendAnimationChannel(channel, rAnimation, rData.channels);
}

static void LoadAssimpAnimations(const aiScene& rScene, const std::unordered_map<std::string, int64_t>& rNodeNames, SceneExportData& rData)
{
	ASSERT(rScene.mNumAnimations <= common::AnimationHeader::kiMaxAnimations);
	for (int64_t i = 0; i < rScene.mNumAnimations; ++i)
	{
		const aiAnimation& rSource = *rScene.mAnimations[i];
		ASSERT(std::isfinite(rSource.mTicksPerSecond) && rSource.mTicksPerSecond > 0.0);
		ASSERT(std::isfinite(rSource.mDuration) && rSource.mDuration >= 0.0);
		double fDuration = rSource.mDuration / rSource.mTicksPerSecond;
		ASSERT(std::isfinite(fDuration) && std::isfinite(static_cast<float>(fDuration)));
		common::AnimationClip animation = BeginAnimationClip(rSource.mName.C_Str(), std::ssize(rData.channels));
		std::unordered_set<int64_t> targets;
		for (int64_t j = 0; j < rSource.mNumChannels; ++j)
		{
			const aiNodeAnim& rChannel = *rSource.mChannels[j];
			int64_t iNode = ResolveAssimpNode(rChannel.mNodeName, rNodeNames);
			ASSERT(targets.insert(iNode).second);
			const common::ModelNode& rNode = rData.skeletonData.nodes.at(iNode);
			LoadAssimpComponent(std::span<const aiVectorKey>(rChannel.mPositionKeys, rChannel.mNumPositionKeys), common::AnimationChannel::kiTargetPathTranslation, iNode, rNode.f4BindTranslation, rChannel, rSource, animation, rData);
			LoadAssimpComponent(std::span<const aiQuatKey>(rChannel.mRotationKeys, rChannel.mNumRotationKeys), common::AnimationChannel::kiTargetPathRotation, iNode, rNode.f4BindRotation, rChannel, rSource, animation, rData);
			XMFLOAT4A f4Scale(rNode.f4BindScale.x, rNode.f4BindScale.y, rNode.f4BindScale.z, 0.0f);
			LoadAssimpComponent(std::span<const aiVectorKey>(rChannel.mScalingKeys, rChannel.mNumScalingKeys), common::AnimationChannel::kiTargetPathScale, iNode, f4Scale, rChannel, rSource, animation, rData);
		}
		animation.fDuration = static_cast<float>(fDuration);
		ASSERT(animation.uiChannelCount == 0 || animation.fDuration > 0.0f);
		FinishAnimationClip(animation, rData.animations);
	}
}

static void LoadAssimpNode(const aiScene& rScene, const aiNode& rNode, const XMMATRIX& rMatParent, SceneExportData& rData, const std::vector<common::MaterialShaderData>& rSourceMaterials, MaterialNodeMap& rMaterialNodeMap, const std::unordered_map<std::string, int64_t>& rNodeNames, std::unordered_map<int64_t, int64_t>& rNodeJoints, bool bHasSkeleton, int64_t& riNode)
{
	int64_t iNode = riNode++;
	ASSERT(riNode <= std::numeric_limits<int16_t>::max());
	XMMATRIX matWorld = AssimpMatrix(rNode.mTransformation) * rMatParent;
	XMVECTOR vecDeterminant = XMMatrixDeterminant(matWorld);
	float fDeterminant = XMVectorGetX(vecDeterminant);
	ASSERT(std::isfinite(fDeterminant) && fDeterminant != 0.0f);
	XMMATRIX matNormal = XMMatrixTranspose(XMMatrixInverse(nullptr, matWorld));
	for (int64_t i = 0; i < rNode.mNumMeshes; ++i)
	{
		ASSERT(rNode.mMeshes[i] < rScene.mNumMeshes);
		const aiMesh& rMesh = *rScene.mMeshes[rNode.mMeshes[i]];
		ASSERT(rMesh.mMaterialIndex < rData.iSourceMaterialCount);
		ASSERT(rMesh.HasPositions() && rMesh.HasNormals());
		ASSERT(rMesh.mNumVertices <= static_cast<uintmax_t>(std::numeric_limits<uint32_t>::max()) - static_cast<uintmax_t>(std::ssize(rData.vertices)));
		for (int64_t j = 0; j < AI_MAX_NUMBER_OF_COLOR_SETS; ++j)
		{
			ASSERT(!rMesh.HasVertexColors(static_cast<unsigned int>(j)));
		}
		for (int64_t j = 5; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++j)
		{
			ASSERT(!rMesh.HasTextureCoords(static_cast<unsigned int>(j)));
		}
		int64_t iOriginal = rMesh.mMaterialIndex;
		const common::MaterialShaderData& rShaderData = rSourceMaterials.at(iOriginal);
		for (int64_t iSet : {rShaderData.iColorTextureSet, rShaderData.iPhysicalDescriptorTextureSet, rShaderData.iNormalTextureSet, rShaderData.iOcclusionTextureSet, rShaderData.iEmissiveTextureSet, })
		{
			ASSERT(iSet < 0 || rMesh.HasTextureCoords(static_cast<unsigned int>(iSet)));
		}
		bool bSkinned = rMesh.mNumBones != 0;
		int64_t iEffective = ResolveEffectiveMaterial(rData.materials, rData.materialNodeInfos, rMaterialNodeMap, iOriginal, iNode, bSkinned, matWorld);
		ASSERT(std::ssize(rData.materials) <= common::SceneHeader::kiMaxMaterials);
		Material& rMaterial = rData.materials.at(iEffective);
		int64_t iStart = std::ssize(rData.vertices);
		rData.vertices.reserve(static_cast<size_t>(std::ssize(rData.vertices)) + rMesh.mNumVertices);
		for (int64_t j = 0; j < rMesh.mNumVertices; ++j)
		{
			common::ModelVertex vertex {};
			const aiVector3D& rPosition = rMesh.mVertices[j];
			const aiVector3D& rNormal = rMesh.mNormals[j];
			XMVECTOR vecPosition = XMVectorSet(rPosition.x, rPosition.y, rPosition.z, 1.0f);
			XMVECTOR vecNormal = XMVectorSet(rNormal.x, rNormal.y, rNormal.z, 0.0f);
			if (!bHasSkeleton)
			{
				vecPosition = XMVector3TransformCoord(vecPosition, matWorld);
				vecNormal = XMVector3TransformNormal(vecNormal, matNormal);
			}
			ASSERT(IsFiniteVector(vecPosition) && IsFiniteVector(vecNormal) && XMVectorGetX(XMVector3LengthSq(vecNormal)) > 0.0f);
			XMStoreFloat3(&vertex.f3Pos, vecPosition);
			XMStoreFloat3(&vertex.f3Normal, XMVector3Normalize(vecNormal));
			XMFLOAT2* pf2Uvs[] {&vertex.f2Uv, &vertex.f2Uv1, &vertex.f2Uv2, &vertex.f2Uv3, &vertex.f2Uv4, };
			for (int64_t k = 0; k < 5; ++k)
			{
				if (rMesh.HasTextureCoords(static_cast<unsigned int>(k)))
				{
					ASSERT(rMesh.mNumUVComponents[k] == 2);
					const aiVector3D& rUv = rMesh.mTextureCoords[k][j];
					ASSERT(std::isfinite(rUv.x) && std::isfinite(rUv.y));
					*pf2Uvs[k] = XMFLOAT2(rUv.x, rUv.y);
				}
			}
			vertex.f4Weight0.x = bSkinned ? 0.0f : 1.0f;
			rData.vertices.push_back(vertex);
		}
		std::vector<int64_t> influenceCounts(bSkinned ? rMesh.mNumVertices : 0);
		for (int64_t j = 0; j < rMesh.mNumBones; ++j)
		{
			const aiBone& rBone = *rMesh.mBones[j];
			int64_t iBoneNode = ResolveAssimpNode(rBone.mName, rNodeNames);
			XMMATRIX matInverseBind = AssimpMatrix(rBone.mOffsetMatrix);
			ValidateAssimpMatrix(matInverseBind);
			auto [it, bInserted] = rNodeJoints.try_emplace(iBoneNode, std::ssize(rData.skeletonData.skinJointToNode));
			int64_t iJoint = it->second;
			if (bInserted)
			{
				rData.skeletonData.skinJointToNode.push_back(static_cast<uint16_t>(iBoneNode));
				XMFLOAT4X4 f4x4InverseBind {};
				XMStoreFloat4x4(&f4x4InverseBind, matInverseBind);
				rData.skeletonData.inverseBindMatrices.push_back(f4x4InverseBind);
			}
			else
			{
				// One packed skin cannot give a shared bone different mesh-local inverse binds.
				XMMATRIX matStored = XMLoadFloat4x4(&rData.skeletonData.inverseBindMatrices.at(iJoint));
				for (int64_t k = 0; k < 4; ++k)
				{
					ASSERT(XMVector4Equal(matInverseBind.r[k], matStored.r[k]));
				}
			}
			for (int64_t k = 0; k < rBone.mNumWeights; ++k)
			{
				const aiVertexWeight& rWeight = rBone.mWeights[k];
				ASSERT(rWeight.mVertexId < rMesh.mNumVertices && std::isfinite(rWeight.mWeight) && rWeight.mWeight >= 0.0f);
				if (rWeight.mWeight == 0.0f)
				{
					continue;
				}
				int64_t& riInfluences = influenceCounts.at(rWeight.mVertexId);
				ASSERT(riInfluences < 4);
				common::ModelVertex& rVertex = rData.vertices.at(iStart + rWeight.mVertexId);
				XMStoreFloat4(&rVertex.f4Joint0, XMVectorSetByIndex(XMLoadFloat4(&rVertex.f4Joint0), static_cast<float>(iJoint), static_cast<size_t>(riInfluences)));
				XMStoreFloat4(&rVertex.f4Weight0, XMVectorSetByIndex(XMLoadFloat4(&rVertex.f4Weight0), rWeight.mWeight, static_cast<size_t>(riInfluences)));
				++riInfluences;
			}
		}
		if (bSkinned)
		{
			for (int64_t j = 0; j < rMesh.mNumVertices; ++j)
			{
				common::ModelVertex& rVertex = rData.vertices.at(iStart + j);
				ASSERT(std::abs(XMVectorGetX(XMVector4Dot(XMLoadFloat4(&rVertex.f4Weight0), XMVectorReplicate(1.0f))) - 1.0f) <= 1.0e-5f);
				rVertex.fJoint = rVertex.f4Joint0.x;
			}
		}
		rMaterial.indexBuffer.reserve(static_cast<size_t>(std::ssize(rMaterial.indexBuffer)) + 3 * static_cast<size_t>(rMesh.mNumFaces));
		for (int64_t j = 0; j < rMesh.mNumFaces; ++j)
		{
			const aiFace& rFace = rMesh.mFaces[j];
			ASSERT(rFace.mNumIndices == 3);
			for (int64_t k : {0, fDeterminant < 0.0f ? 2 : 1, fDeterminant < 0.0f ? 1 : 2, })
			{
				ASSERT(rFace.mIndices[k] < rMesh.mNumVertices);
				rMaterial.indexBuffer.push_back(static_cast<uint32_t>(static_cast<uint32_t>(iStart) + rFace.mIndices[k]));
			}
		}
	}
	for (int64_t i = 0; i < rNode.mNumChildren; ++i)
	{
		LoadAssimpNode(rScene, *rNode.mChildren[i], matWorld, rData, rSourceMaterials, rMaterialNodeMap, rNodeNames, rNodeJoints, bHasSkeleton, riNode);
	}
}

AssimpSceneDecoder::AssimpSceneDecoder(const std::filesystem::path& rPath, SceneExportData& rData)
{
	Assimp::Importer importer;
	SceneIOSystem* pIO = new SceneIOSystem(mDependencies);
	importer.SetIOHandler(pIO);
	unsigned int uiFlags = aiProcess_Triangulate | aiProcess_GenNormals;
	if (rPath.extension() == ".dae" || rPath.extension() == ".3ds")
	{
		uiFlags |= aiProcess_FlipUVs;
	}
	const aiScene* pScene = importer.ReadFile(rPath.string(), uiFlags);
	if (pScene == nullptr)
	{
		throw std::runtime_error(std::format("Assimp failed to import '{}': {}", rPath.string(), importer.GetErrorString()));
	}
	if (pScene->mRootNode == nullptr)
	{
		throw std::runtime_error(std::format("Assimp failed to import '{}': {}", rPath.string(), importer.GetErrorString()));
	}
	pIO->ValidateReads();
	bool bHasBones = false;
	for (int64_t i = 0; i < pScene->mNumMeshes; ++i)
	{
		ASSERT(pScene->mMeshes[i]->mNumAnimMeshes == 0);
		bHasBones = bHasBones || pScene->mMeshes[i]->mNumBones != 0;
	}
	std::unordered_set<std::string> animatedNames;
	for (int64_t i = 0; i < pScene->mNumAnimations; ++i)
	{
		const aiAnimation& rAnimation = *pScene->mAnimations[i];
		ASSERT(rAnimation.mNumMeshChannels == 0 && rAnimation.mNumMorphMeshChannels == 0);
		for (int64_t j = 0; j < rAnimation.mNumChannels; ++j)
		{
			const aiNodeAnim& rChannel = *rAnimation.mChannels[j];
			if (rChannel.mNumPositionKeys != 0 || rChannel.mNumRotationKeys != 0 || rChannel.mNumScalingKeys != 0)
			{
				animatedNames.emplace(rChannel.mNodeName.C_Str());
			}
		}
	}
	bool bHasSkeleton = bHasBones || !animatedNames.empty();
	rData.iSourceAnimationCount = pScene->mNumAnimations;
	rData.iSourceMaterialCount = std::max<int64_t>(1, pScene->mNumMaterials);
	ASSERT(rData.iSourceMaterialCount <= common::SceneHeader::kiMaxMaterials);
	rData.materials.resize(static_cast<size_t>(rData.iSourceMaterialCount));
	rData.materialNodeInfos.resize(static_cast<size_t>(rData.iSourceMaterialCount));
	mSourceMaterialShaderDatas.reserve(static_cast<size_t>(rData.iSourceMaterialCount));
	for (int64_t i = 0; i < pScene->mNumMaterials; ++i)
	{
		mSourceMaterialShaderDatas.push_back(LoadAssimpMaterial(*pScene, *pScene->mMaterials[i], rPath, rData, mDependencies));
	}
	if (mSourceMaterialShaderDatas.empty())
	{
		common::MaterialShaderData material;
		material.fMetallicFactor = 0.0f;
		material.f3EmissiveFactor = XMFLOAT3(0.0f, 0.0f, 0.0f);
		mSourceMaterialShaderDatas.push_back(material);
	}
	XMMATRIX matBasis = XMMatrixRotationX(XM_PIDIV2);
	std::unordered_map<std::string, int64_t> nodeNames;
	std::vector<XMMATRIX> worldMatrices;
	if (bHasSkeleton)
	{
		// Basis conversion and drawn-pose normalization never receive absolute source keys.
		rData.skeletonData.nodes.push_back(AssimpModelNode(XMMatrixIdentity(), -1));
		rData.skeletonData.nodes.push_back(AssimpModelNode(matBasis, 0));
		worldMatrices.push_back(XMMatrixIdentity());
		worldMatrices.push_back(matBasis);
		LoadAssimpHierarchy(*pScene->mRootNode, 1, animatedNames, rData, nodeNames, worldMatrices);
		LoadAssimpAnimations(*pScene, nodeNames, rData);
		ASSERT(!rData.animations.empty());
	}
	MaterialNodeMap materialNodeMap;
	std::unordered_map<int64_t, int64_t> nodeJoints;
	int64_t iNode = bHasSkeleton ? 2 : 0;
	LoadAssimpNode(*pScene, *pScene->mRootNode, matBasis, rData, mSourceMaterialShaderDatas, materialNodeMap, nodeNames, nodeJoints, bHasSkeleton, iNode);
	std::vector<XMMATRIX> jointMatrices;
	jointMatrices.reserve(static_cast<size_t>(std::ssize(rData.skeletonData.skinJointToNode)));
	for (int64_t i = 0; i < std::ssize(rData.skeletonData.skinJointToNode); ++i)
	{
		jointMatrices.push_back(XMLoadFloat4x4(&rData.skeletonData.inverseBindMatrices.at(i)) * worldMatrices.at(rData.skeletonData.skinJointToNode.at(i)));
	}
	XMVECTOR vecMin = XMVectorReplicate(std::numeric_limits<float>::infinity());
	XMVECTOR vecMax = XMVectorReplicate(-std::numeric_limits<float>::infinity());
	AccumulateDrawnBounds(rData.vertices, rData.materials, rData.materialNodeInfos, jointMatrices, bHasSkeleton, vecMin, vecMax);
	ASSERT(std::ranges::any_of(rData.materials, [](const Material& rMaterial)
	{
		return !rMaterial.indexBuffer.empty();
	}));
	ASSERT(IsFiniteVector(vecMin) && IsFiniteVector(vecMax));
	float fLongest = BoundsLongestExtent(vecMin, vecMax);
	ASSERT(std::isfinite(fLongest) && fLongest > 0.0f);
	XMVECTOR vecCenter = BoundsCenter(vecMin, vecMax);
	if (bHasSkeleton)
	{
		XMFLOAT3A f3Center {};
		XMStoreFloat3A(&f3Center, vecCenter);
		double fScale = 2.0 / fLongest;
		XMMATRIX matNormalization = XMMatrixScaling(static_cast<float>(fScale), static_cast<float>(fScale), static_cast<float>(fScale)) * XMMatrixTranslation(static_cast<float>(-f3Center.x * fScale), static_cast<float>(-f3Center.y * fScale), static_cast<float>(-f3Center.z * fScale));
		rData.skeletonData.nodes.at(0) = AssimpModelNode(matNormalization, -1);
		for (XMMATRIX& rWorld : worldMatrices)
		{
			rWorld = rWorld * matNormalization;
		}
		for (MaterialNodeInfo& rInfo : rData.materialNodeInfos)
		{
			rInfo.matMeshWorld = rInfo.matMeshWorld * matNormalization;
		}
		rData.skeletonData.skeleton.uiNodeCount = static_cast<uint16_t>(std::ssize(rData.skeletonData.nodes));
		rData.skeletonData.skeleton.uiSkinJointCount = static_cast<uint16_t>(std::ssize(rData.skeletonData.skinJointToNode));
	}
	else
	{
		for (common::ModelVertex& rVertex : rData.vertices)
		{
			XMStoreFloat3(&rVertex.f3Pos, XMVectorScale(XMVectorSubtract(XMLoadFloat3(&rVertex.f3Pos), vecCenter), 2.0f / fLongest));
		}
	}
	rData.materialInfos.resize(static_cast<size_t>(std::ssize(rData.materials)));
	for (int64_t i = 0; i < std::ssize(rData.materialInfos); ++i)
	{
		const MaterialNodeInfo& rInfo = rData.materialNodeInfos.at(i);
		XMMATRIX matNodeWorld = bHasSkeleton && rInfo.iNodeIndex >= 0 ? worldMatrices.at(rInfo.iNodeIndex) : XMMatrixIdentity();
		SetMaterialTransform(rInfo, bHasSkeleton, matNodeWorld, rData.materialInfos.at(i));
	}
	std::ranges::sort(mDependencies);
	std::ranges::subrange<std::vector<std::filesystem::path>::iterator> duplicateRange = std::ranges::unique(mDependencies);
	mDependencies.erase(duplicateRange.begin(), duplicateRange.end());
	std::erase(mDependencies, std::filesystem::weakly_canonical(rPath));
}

void AssimpSceneDecoder::DecodeMaterialShaderDatas(SceneExportData& rData) const
{
	rData.materialShaderDatas.clear();
	rData.materialShaderDatas.reserve(static_cast<size_t>(std::ssize(rData.materialInfos)));
	for (const common::MaterialInfo& rInfo : rData.materialInfos)
	{
		int64_t iOriginal = rInfo.iOriginalMaterialIndex >= 0 ? rInfo.iOriginalMaterialIndex : std::ssize(rData.materialShaderDatas);
		rData.materialShaderDatas.push_back(mSourceMaterialShaderDatas.at(iOriginal));
	}
}
