#include "ModelViewerApplication.h"

#include "File/PackChunks.h"
#include "Graphics/Camera.h"
#include "Profile/ProfileRows.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/SunMoonWrappersBase.h"
#include "Game.h"

namespace game
{

constexpr int64_t kiDynamicBufferInitialCapacity = 1;

struct LightingPreset
{
	static constexpr int64_t kiVersion = 2;
	std::array<float, 21> values {};
};

static_assert(sizeof(LightingPreset) == 84, "LightingPreset::kiVersion must be bumped with its layout or participant order");
static_assert(std::is_trivially_copyable_v<LightingPreset>);

constexpr std::array<const char*, 3> kLightingPresetPaths {"ModelViewerPresetW.bin", "ModelViewerPresetE.bin", "ModelViewerPresetR.bin"};

ModelViewerApplication::ModelViewerApplication()
: common::Singleton<ModelViewerApplication>(gpModelViewerApplication, this)
{
	mRenderInterpolates.try_emplace(engine::kOriginCoordinate);
	mActiveCoordinates.push_back(engine::kOriginCoordinate);
	engine::LoadAnimationDataFromEagerChunks();
	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunks = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	for (const auto& [rCrc, rChunk] : rChunks)
	{
		if (!(rChunk.pHeader->flags & common::ChunkFlags::kScene))
		{
			continue;
		}
		if (rChunk.pHeader->sceneHeader.uiMaterialCount == 0)
		{
			continue;
		}
		Preview& rPreview = mPreviews.emplace_back();
		rPreview.crc = rCrc;
		rPreview.pcPath = rChunk.pHeader->pcPath;
	}
	std::sort(mPreviews.begin(), mPreviews.end(), [](const Preview& rLeft, const Preview& rRight)
	{
		return std::strcmp(rLeft.pcPath, rRight.pcPath) < 0;
	});
	if (!engine::gLaunchOptions.scenePath.empty())
	{
		auto it = std::find_if(mPreviews.begin(), mPreviews.end(), [](const Preview& rPreview)
		{
			return engine::gLaunchOptions.scenePath == rPreview.pcPath;
		});
		if (it == mPreviews.end())
		{
			throw std::runtime_error(std::format("ModelViewer packed scene not found: {}", engine::gLaunchOptions.scenePath));
		}
		miSelected = std::distance(mPreviews.begin(), it);
	}
	for (Preview& rPreview : mPreviews)
	{
		PreparePreview(rPreview);
	}
	SelectLightingPreset(0);
}

struct LightingDescriptor
{
	engine::Wrapper* pWrapper = nullptr;
	const char* pcLabel = nullptr;
	const char* pcHeading = nullptr;
};

constexpr std::array<LightingDescriptor, 21> kLightingDescriptors
{
	LightingDescriptor { .pWrapper = &engine::gSunMoonMinimumAmbient, .pcLabel = "Minimum Ambient", .pcHeading = "Ambient and environment", },
	LightingDescriptor { .pWrapper = &engine::gSunMoonAmbientMultiplier, .pcLabel = "Ambient Multiplier", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingImageBasedLightingAmbient, .pcLabel = "IBL Ambient", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingImageBasedLightingDiffuse, .pcLabel = "IBL Diffuse", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingImageBasedLightingDiffusePower, .pcLabel = "IBL Diffuse Power", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingImageBasedLightingSpecular, .pcLabel = "IBL Specular", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingImageBasedLightingSpecularPower, .pcLabel = "IBL Specular Power", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend, .pcLabel = "IBL Ambient Color Blend", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingCubemapLevelOfDetailPower, .pcLabel = "Cubemap Lod Power", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingCubemapLevelOfDetailOffset, .pcLabel = "Cubemap Lod Offset", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse, .pcLabel = "BRDF Diffuse", .pcHeading = "BRDF", },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower, .pcLabel = "BRDF Diffuse Power", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular, .pcLabel = "BRDF Specular", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower, .pcLabel = "BRDF Specular Power", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias, .pcLabel = "Model Data Mip Bias", .pcHeading = "Material sampling and emissive", },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingEmissive, .pcLabel = "Emissive", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingExposure, .pcLabel = "Exposure", .pcHeading = "HDR color grading", },
	LightingDescriptor { .pWrapper = &engine::gPhysicallyBasedRenderingGamma, .pcLabel = "Gamma", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gColorGradingSaturation, .pcLabel = "Saturation", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gColorGradingContrast, .pcLabel = "Contrast", .pcHeading = nullptr, },
	LightingDescriptor { .pWrapper = &engine::gColorGradingTemperature, .pcLabel = "Temperature", .pcHeading = nullptr, },
};

static std::array<float, 21> GetLightingDefaults()
{
	std::array<float, 21> values {};
	for (const auto& [i, rDescriptor] : std::views::enumerate(kLightingDescriptors))
	{
		values.at(i) = rDescriptor.pWrapper->mfDefault;
	}
	return values;
}

void ModelViewerApplication::UpdateDefaultComparison(bool bHeld)
{
	if (bHeld == (mFlags & ViewerFlags::kComparingDefaults))
	{
		return;
	}
	if (bHeld)
	{
		for (const auto& [i, rDescriptor] : std::views::enumerate(kLightingDescriptors))
		{
			mLightingTweaks.at(i) = rDescriptor.pWrapper->mfCurrent;
		}
	}
	for (const auto& [i, rDescriptor] : std::views::enumerate(kLightingDescriptors))
	{
		rDescriptor.pWrapper->Set(bHeld ? rDescriptor.pWrapper->mfDefault : mLightingTweaks.at(i));
	}
	mFlags.Set(ViewerFlags::kComparingDefaults, bHeld);
}

void ModelViewerApplication::ApplyLightingValues(const std::array<float, 21>& rValues)
{
	// End slider interactions with the previous lighting state.
	++muiLightingRevision;
	if (mFlags & ViewerFlags::kComparingDefaults)
	{
		mLightingTweaks = rValues;
		return;
	}
	for (const auto& [i, rDescriptor] : std::views::enumerate(kLightingDescriptors))
	{
		rDescriptor.pWrapper->Set(rValues.at(i));
	}
}

void ModelViewerApplication::SaveLightingPreset(const std::array<float, 21>& rValues)
{
	// Heap: preset file I/O allocates paths and streams.
	ScopedSuppressAllocationTracking suppress;
	LightingPreset preset {.values = rValues};
	const char* pcPath = kLightingPresetPaths.at(miSelectedLightingPreset);
	if (!engine::WriteVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, pcPath, preset))
	{
		LOG(kLoading, kError, "ModelViewer preset save failed: {}", pcPath);
	}
}

void ModelViewerApplication::SelectLightingPreset(int64_t iSlot)
{
	// Heap: preset file I/O allocates paths and streams.
	ScopedSuppressAllocationTracking suppress;
	LightingPreset preset {};
	const char* pcPath = kLightingPresetPaths.at(iSlot);
	bool bRead = engine::ReadVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, pcPath, preset);
	if (bRead)
	{
		for (const auto& [i, rDescriptor] : std::views::enumerate(kLightingDescriptors))
		{
			if (!rDescriptor.pWrapper->IsInRange(preset.values.at(i)))
			{
				LOG(kLoading, kWarning, "{} rejected: invalid slider {}", pcPath, i);
				bRead = false;
				break;
			}
		}
	}
	miSelectedLightingPreset = iSlot;
	ApplyLightingValues(bRead ? preset.values : GetLightingDefaults());
}

void ModelViewerApplication::ResetLightingPreset()
{
	std::array<float, 21> values = GetLightingDefaults();
	ApplyLightingValues(values);
	SaveLightingPreset(values);
}

void ModelViewerApplication::PreparePreview(Preview& rPreview)
{
	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunks = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const engine::EagerChunk& rScene = rChunks.at(rPreview.crc);
	const common::SceneHeader& rSceneHeader = rScene.pHeader->sceneHeader;
	if (std::cmp_greater(rSceneHeader.uiTextureCount, common::SceneHeader::kiMaxTextures) || std::cmp_greater(rSceneHeader.uiMaterialCount, common::SceneHeader::kiMaxMaterials))
	{
		throw std::ios_base::failure("ModelViewer scene counts");
	}
	if (rSceneHeader.bHasAnimation)
	{
		const engine::AnimationData& rAnimation = engine::gAnimationDataMap.at(rPreview.crc);
		auto matrices = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMMATRIX*>(rAnimation.mHeader.skeleton.uiNodeCount * static_cast<int64_t>(sizeof(XMMATRIX)));
		for (int64_t i = 0; std::cmp_less(i, rAnimation.mHeader.skeleton.uiNodeCount); ++i)
		{
			int64_t iParent = rAnimation.mpNodes[i].iParentIndex;
			matrices.mpData[i] = iParent < 0 ? rAnimation.mpBindPoseLocalMatrices[i] : rAnimation.mpBindPoseLocalMatrices[i] * matrices.mpData[iParent];
		}
		rPreview.meshData.resize(rSceneHeader.uiMaterialCount);
		rPreview.jointMatrices.resize(rAnimation.SkinnedMaterialCount(rSceneHeader.uiMaterialCount) * rAnimation.mHeader.skeleton.uiSkinJointCount);
		int64_t iJointOffset = 0;
		for (int64_t i = 0; std::cmp_less(i, rSceneHeader.uiMaterialCount); ++i)
		{
			rAnimation.EvaluateMaterial(i, matrices.mpData, &rPreview.meshData.at(i), rPreview.jointMatrices.data(), iJointOffset);
			iJointOffset += rPreview.meshData.at(i).uiJointCount;
		}
	}
	const engine::EagerChunk& rModel = rChunks.at(rSceneHeader.modelCrc);
	const common::ModelHeader& rModelHeader = rModel.pHeader->modelHeader;
	int64_t iIndexSize = common::ModelHeader::UsesU16Indices(rModelHeader.iVertexCount) ? sizeof(uint16_t) : sizeof(uint32_t);
	if (!(rModel.pHeader->flags & common::ChunkFlags::kModel) || !std::in_range<uint32_t>(rModelHeader.iIndexCount) || rModelHeader.iVertexCount <= 0 || std::cmp_not_equal(rModelHeader.iStride, sizeof(common::ModelVertex)))
	{
		throw std::ios_base::failure("ModelViewer model layout");
	}
	int64_t iVerticesOffset = common::ModelHeader::VerticesOffset(rModelHeader.iIndexCount, iIndexSize);
	if (iVerticesOffset > rModel.iDataSize || rModelHeader.iVertexCount > (rModel.iDataSize - iVerticesOffset) / rModelHeader.iStride)
	{
		throw std::ios_base::failure("ModelViewer model extent");
	}
	int64_t iStartsOffset = common::SceneHeader::IndexStartsOffset(rSceneHeader.uiTextureCount);
	if (iStartsOffset + rSceneHeader.uiMaterialCount * static_cast<int64_t>(sizeof(uint32_t)) > rScene.iDataSize)
	{
		throw std::ios_base::failure("ModelViewer material extent");
	}
	const uint32_t* puiStarts = reinterpret_cast<const uint32_t*>(rScene.pData + iStartsOffset);
	XMVECTOR vecMin = XMVectorReplicate(std::numeric_limits<float>::max());
	XMVECTOR vecMax = XMVectorReplicate(-std::numeric_limits<float>::max());
	int64_t iDrawnVertices = 0;
	for (int64_t i = 0; std::cmp_less(i, rSceneHeader.uiMaterialCount); ++i)
	{
		int64_t iEnd = std::cmp_equal(i + 1, rSceneHeader.uiMaterialCount) ? rModelHeader.iIndexCount : puiStarts[i + 1];
		if (std::cmp_greater(puiStarts[i], iEnd) || iEnd > rModelHeader.iIndexCount)
		{
			throw std::ios_base::failure("ModelViewer material indices");
		}
		for (int64_t j = puiStarts[i]; j < iEnd; ++j)
		{
			uint32_t uiIndex = std::cmp_equal(iIndexSize, sizeof(uint16_t)) ? reinterpret_cast<const uint16_t*>(rModel.pData)[j] : reinterpret_cast<const uint32_t*>(rModel.pData)[j];
			if (std::cmp_greater_equal(uiIndex, rModelHeader.iVertexCount))
			{
				throw std::ios_base::failure("ModelViewer vertex index");
			}
			common::ModelVertex vertex;
			std::memcpy(&vertex, rModel.pData + iVerticesOffset + uiIndex * rModelHeader.iStride, sizeof(vertex));
			XMVECTOR vecPosition = XMVectorSetW(XMLoadFloat3(&vertex.f3Pos), 1.0f);
			if (rSceneHeader.bHasAnimation)
			{
				const common::MeshData& rMesh = rPreview.meshData.at(i);
				if (rMesh.uiJointCount > 0)
				{
					const float* pfJoints = &vertex.f4Joint0.x;
					const float* pfWeights = &vertex.f4Weight0.x;
					XMVECTOR pVecRows[3] = {XMVectorZero(), XMVectorZero(), XMVectorZero()};
					for (int64_t k = 0; k < 4; ++k)
					{
						if (!std::isfinite(pfWeights[k]))
						{
							throw std::ios_base::failure("ModelViewer vertex skin");
						}
						if (pfWeights[k] == 0.0f)
						{
							continue;
						}
						if (!std::isfinite(pfJoints[k]) || pfJoints[k] < 0.0f || pfJoints[k] >= rMesh.uiJointCount || std::floor(pfJoints[k]) != pfJoints[k])
						{
							throw std::ios_base::failure("ModelViewer vertex skin");
						}
						const common::JointMatrix& rJoint = rPreview.jointMatrices.at(rMesh.uiJointMatrixOffset + static_cast<uint32_t>(pfJoints[k]));
						for (int64_t iRow = 0; iRow < 3; ++iRow)
						{
							pVecRows[iRow] = XMVectorAdd(pVecRows[iRow], XMVectorScale(XMLoadFloat4(&rJoint.pf4Rows[iRow]), pfWeights[k]));
						}
					}
					// Compact joint rows store translation in w; restore the row-vector translation row.
					XMMATRIX matSkin(XMVectorSetW(pVecRows[0], 0.0f), XMVectorSetW(pVecRows[1], 0.0f), XMVectorSetW(pVecRows[2], 0.0f), XMVectorSet(XMVectorGetW(pVecRows[0]), XMVectorGetW(pVecRows[1]), XMVectorGetW(pVecRows[2]), 1.0f));
					vecPosition = XMVector4Transform(vecPosition, matSkin);
				}
				vecPosition = XMVector4Transform(vecPosition, XMLoadFloat4x4(&rMesh.matrix));
			}
			if (XMVector3IsNaN(vecPosition) || XMVector3IsInfinite(vecPosition))
			{
				throw std::ios_base::failure("ModelViewer bounds position");
			}
			vecMin = XMVectorMin(vecMin, vecPosition);
			vecMax = XMVectorMax(vecMax, vecPosition);
			++iDrawnVertices;
		}
	}
	XMVECTOR vecCenter = XMVectorScale(XMVectorAdd(vecMin, vecMax), 0.5f);
	XMStoreFloat3A(&rPreview.f3Center, vecCenter);
	rPreview.fRadius = XMVectorGetX(XMVector3Length(XMVectorSubtract(vecMax, vecCenter)));
	if (iDrawnVertices == 0 || !std::isfinite(rPreview.fRadius) || rPreview.fRadius <= 0.0f)
	{
		throw std::ios_base::failure("ModelViewer empty bounds");
	}
	LOG(kGraphics, kInfo, "ModelViewer rest preview {}: materials {}, joints {}, center ({}, {}, {}), radius {}", rPreview.pcPath, rSceneHeader.uiMaterialCount, std::ssize(rPreview.jointMatrices), rPreview.f3Center.x, rPreview.f3Center.y, rPreview.f3Center.z, rPreview.fRadius);
}

void ModelViewerApplication::CreateGraphicsResources()
{
	for (const Preview& rPreview : mPreviews)
	{
		engine::Buffer* pBuffers = engine::gpBufferManager->CreateDynamicBuffer(rPreview.crc, engine::kBufferMain, rPreview.pcPath, sizeof(shaders::ModelLayout), kiDynamicBufferInitialCapacity);
		engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(rPreview.crc, rPreview.pcPath, rPreview.crc, pBuffers);
	}
}

void FrameInterpolate::GraphicsResources()
{
	gpModelViewerApplication->CreateGraphicsResources();
}

void ModelViewerApplication::RenderModel(int64_t iCommandBuffer)
{
	for (int64_t i = 0; i < std::ssize(mPreviews); ++i)
	{
		const Preview& rPreview = mPreviews.at(i);
		engine::ModelPipeline* pPipeline = engine::gpPipelineManager->mDynamicPipelines.mpModelPipelineMaps[engine::kDynamicModelPipelineModel].at(rPreview.crc);
		if (i != miSelected)
		{
			pPipeline->WriteIndirectBuffer(iCommandBuffer, 0);
			continue;
		}
		shaders::ModelLayout& rLayout = *engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(rPreview.crc, engine::kBufferMain, iCommandBuffer).pData;
		rLayout = {};
		rLayout.f4Position = {0.0f, 0.0f, 0.0f, 1.0f};
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rLayout.f3x4Transform[0]), XMMatrixIdentity());
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rLayout.f3x4TransformNormal[0]), XMMatrixIdentity());
		if (!rPreview.meshData.empty())
		{
			const engine::AnimationData& rAnimation = engine::gAnimationDataMap.at(rPreview.crc);
			engine::SkinningReservation reservation = rAnimation.ReserveSkinning(iCommandBuffer, 1);
			if (miSelectedAnimation >= 0)
			{
				rAnimation.EvaluateAnimation(miSelectedAnimation, mAnimationTime.count(), std::span(reservation.pMeshData + reservation.iMeshDataBase, static_cast<size_t>(reservation.iMaterialCount)), reservation.pJointMatrices, reservation.iJointBase);
			}
			else
			{
				for (int64_t j = 0; j < reservation.iMaterialCount; ++j)
				{
					common::MeshData meshData = rPreview.meshData.at(j);
					meshData.uiJointMatrixOffset += static_cast<uint32_t>(reservation.iJointBase);
					reservation.pMeshData[reservation.iMeshDataBase + j] = meshData;
				}
				std::copy(rPreview.jointMatrices.begin(), rPreview.jointMatrices.end(), reservation.pJointMatrices + reservation.iJointBase);
			}
			rLayout.uiMeshDataBase = static_cast<uint32_t>(reservation.iMeshDataBase);
			rLayout.uiMaterialCount = static_cast<uint32_t>(reservation.iMaterialCount);
		}
		pPipeline->WriteIndirectBuffer(iCommandBuffer, 1);
	}
}

bool ModelViewerApplication::Render()
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerFrameUpdate);
	if (engine::gpGraphics->mbSwapchainRecreateDeferred || engine::gpGraphics->meDestroyType >= engine::DestroyType::kSwapchain)
	{
		engine::gpGraphics->Create();
		if (engine::gpGraphics->mbSwapchainRecreateDeferred)
		{
			engine::gpTextureUploadManager->SignalFrame();
			return false;
		}
		engine::gpProfileManager->DiscardSkippedFrameSamples();
		engine::gpSwapchainManager->AcquireNextImage();
		engine::gpGraphics->mRenderFrameTimer.Reset();
	}
	Camera* pCamera = static_cast<Camera*>(engine::gpCamera);
	if (!mPreviews.empty() && miFittedSelection != miSelected)
	{
		pCamera->Fit(mPreviews.at(miSelected).f3Center, mPreviews.at(miSelected).fRadius);
		miFittedSelection = miSelected;
	}
	pCamera->Update();
	FrameInterpolate& rFrame = mRenderInterpolates.at(engine::kOriginCoordinate);
	rFrame.iFrame = miFrame++;
	std::chrono::duration<float> elapsedTime(mTimer.GetDeltaNs());
	engine::gpGraphics->RenderGlobal(elapsedTime);
	if (mFlags & ViewerFlags::kPlaying)
	{
		const engine::AnimationData& rAnimation = engine::gAnimationDataMap.at(mPreviews.at(miSelected).crc);
		float fDuration = rAnimation.mpAnimations[miSelectedAnimation].fDuration;
		std::chrono::duration<float> deltaTime(engine::gpGraphics->mRenderFrameDeltaNanoseconds);
		mAnimationTime = fDuration > 0.0f ? std::chrono::duration<float>(std::fmod((mAnimationTime + deltaTime).count(), fDuration)) : 0s;
	}
	engine::gpGraphics->RenderMainPresentAcquire(engine::gpSwapchainManager->miFramebufferIndex, mRenderInterpolates, mActiveCoordinates, engine::kOriginCoordinate, elapsedTime);
	return true;
}

void ModelViewerApplication::RenderUi()
{
	// Heap: ImGui retains window and draw storage between frames and rebuilds it after recreation.
	ScopedSuppressAllocationTracking suppress;
	float fScale = engine::UiScale();
	const ImGuiViewport* pViewport = ImGui::GetMainViewport();
	float fMargin = 16.0f * fScale;
	float fSunTrackWidth = 0.5f * pViewport->Size.x;
	float fSunWindowWidth = fSunTrackWidth + 2.0f * ImGui::GetStyle().WindowPadding.x;
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	ImGui::SetNextWindowPos(ImVec2(pViewport->Pos.x + 0.5f * (pViewport->Size.x - fSunWindowWidth), pViewport->Pos.y + fMargin));
	ImGui::SetNextWindowSize(ImVec2(fSunWindowWidth, 0.0f));
	if (ImGui::Begin("Sun angle", nullptr, windowFlags | ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::TextUnformatted("Sun angle");
		float fNormalFrameHeight = ImGui::GetFrameHeight();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (2.0f * fNormalFrameHeight - ImGui::GetFontSize()) / 2.0f));
		ImGui::SetNextItemWidth(fSunTrackWidth);
		ImGui::SliderFloat("##Sun angle", &engine::gpCamera->mfSunAngle, 0.0f, XM_2PI, "%.2f rad", ImGuiSliderFlags_AlwaysClamp);
		ImGui::PopStyleVar();
		engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());
	}
	float fPanelsTop = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + fMargin;
	ImGui::End();
	float fPanelHeight = std::max(1.0f, pViewport->Pos.y + pViewport->Size.y - fMargin - fPanelsTop);
	float fPanelCenter = pViewport->Pos.y + 0.5f * pViewport->Size.y;
	float fCenteredPanelHeight = std::max(1.0f, 2.0f * std::min(fPanelCenter - fPanelsTop, pViewport->Pos.y + pViewport->Size.y - fMargin - fPanelCenter));
	float fCatalogWidth = std::min(460.0f * fScale, 0.4f * pViewport->Size.x);
	float fAvailableWidth = std::max(1.0f, pViewport->Size.x - 2.0f * fMargin);
	float fLightingTrackWidth = 2.0f * std::max(1.0f, std::min(340.0f * fScale, 0.4f * pViewport->Size.x) - 2.0f * ImGui::GetStyle().WindowPadding.x);
	float fLightingWidth = fLightingTrackWidth + 2.0f * ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ScrollbarSize;
	bool bStackPanels = fCatalogWidth + fLightingWidth + 3.0f * fMargin > pViewport->Size.x || fPanelsTop >= fPanelCenter;
	float fCatalogHeight = bStackPanels ? std::max(1.0f, (fPanelHeight - fMargin) * 0.5f) : fCenteredPanelHeight;
	fCatalogWidth = bStackPanels ? fAvailableWidth : fCatalogWidth;
	fLightingWidth = std::min(fLightingWidth, fAvailableWidth);
	ImGuiWindowFlags iPanelWindowFlags = windowFlags | (bStackPanels ? 0 : ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::SetNextWindowPos(ImVec2(pViewport->Pos.x + fMargin, bStackPanels ? fPanelsTop : fPanelCenter), ImGuiCond_Always, ImVec2(0.0f, bStackPanels ? 0.0f : 0.5f));
	if (bStackPanels)
	{
		ImGui::SetNextWindowSize(ImVec2(fCatalogWidth, fCatalogHeight));
	}
	else
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(fCatalogWidth, 0.0f), ImVec2(fCatalogWidth, fCatalogHeight));
	}
	if (ImGui::Begin("Model Viewer", nullptr, iPanelWindowFlags))
	{
		if (!mPreviews.empty())
		{
			const Preview& rPreview = mPreviews.at(miSelected);
			const engine::AnimationData* pAnimation = rPreview.meshData.empty() ? nullptr : &engine::gAnimationDataMap.at(rPreview.crc);
			const char* pcAnimation = miSelectedAnimation < 0 ? "Rest pose" : pAnimation->mpAnimations[miSelectedAnimation].pcName;
			if (ImGui::BeginCombo("Animation", pcAnimation))
			{
				if (ImGui::Selectable("Rest pose", miSelectedAnimation < 0))
				{
					miSelectedAnimation = -1;
					mAnimationTime = 0s;
					mFlags.Set(ViewerFlags::kPlaying, false);
				}
				if (pAnimation != nullptr)
				{
					for (int64_t i = 0; std::cmp_less(i, pAnimation->mHeader.uiAnimationCount); ++i)
					{
						ImGui::PushID(static_cast<int>(i));
						if (ImGui::Selectable(pAnimation->mpAnimations[i].pcName, i == miSelectedAnimation))
						{
							miSelectedAnimation = i;
							mAnimationTime = 0s;
							mFlags.Set(ViewerFlags::kPlaying);
						}
						ImGui::PopID();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::Separator();
		}
		float fQualityTrackWidth = ImGui::GetContentRegionAvail().x;
		float fNormalFrameHeight = ImGui::GetFrameHeight();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (1.5f * fNormalFrameHeight - ImGui::GetFontSize()) / 2.0f));
		auto QualitySlider = [fQualityTrackWidth](const char* pcLabel, engine::Wrapper& rWrapper)
		{
			ImGui::PushID(pcLabel);
			ImGui::TextUnformatted(pcLabel);
			ImGui::SetNextItemWidth(fQualityTrackWidth);
			float fValue = rWrapper.mfCurrent;
			if (ImGui::SliderFloat("##Value", &fValue, rWrapper.mfMin, rWrapper.mfMax, "%.3f", ImGuiSliderFlags_AlwaysClamp))
			{
				rWrapper.Set(fValue);
			}
			ImGui::PopID();
		};
		ImGui::SeparatorText("Render quality");
		auto QualityToggle = [](const char* pcLabel, engine::Wrapper& rWrapper)
		{
			bool bValue = rWrapper.Get<bool>();
			if (ImGui::Checkbox(pcLabel, &bValue))
			{
				rWrapper.Set(bValue);
			}
		};
		QualityToggle("Multisampling", engine::gMultisampling);
		ImGui::BeginDisabled(!engine::gMultisampling.Get<bool>());
		ImGui::TextUnformatted("Sample count");
		ImGui::SetNextItemWidth(fQualityTrackWidth);
		char pcSampleCount[16];
		std::snprintf(pcSampleCount, sizeof(pcSampleCount), "%dx", engine::gSampleCount.Get<int>());
		if (ImGui::BeginCombo("##Sample count", pcSampleCount))
		{
			for (float fSampleCount : engine::gSampleCount.mAllowed)
			{
				VkSampleCountFlagBits vkSampleCount = static_cast<VkSampleCountFlagBits>(fSampleCount);
				std::snprintf(pcSampleCount, sizeof(pcSampleCount), "%dx", static_cast<int>(vkSampleCount));
				if (ImGui::Selectable(pcSampleCount, vkSampleCount == engine::gSampleCount.Get<VkSampleCountFlagBits>()))
				{
					engine::gSampleCount.Set(vkSampleCount);
				}
			}
			ImGui::EndCombo();
		}
		QualityToggle("Sample Shading", engine::gSampleShading);
		ImGui::BeginDisabled(!engine::gSampleShading.Get<bool>());
		QualitySlider("Min Sample Shading", engine::gMinimumSampleShading);
		ImGui::EndDisabled();
		ImGui::EndDisabled();
		QualityToggle("Anisotropy", engine::gAnisotropy);
		ImGui::BeginDisabled(!engine::gAnisotropy.Get<bool>());
		QualitySlider("Max Anisotropy", engine::gMaximumAnisotropy);
		ImGui::EndDisabled();
		QualitySlider("Mip Lod Bias", engine::gMipmapLevelOfDetailBias);
		ImGui::PopStyleVar();
		ImGui::Separator();
		ImGui::Text("Scenes: %lld", static_cast<long long>(std::ssize(mPreviews)));
		ImGui::Separator();
		if (mPreviews.empty())
		{
			ImGui::TextWrapped("No selectable model. No renderable scenes were found.");
		}
		for (int64_t i = 0; i < std::ssize(mPreviews); ++i)
		{
			if (ImGui::Selectable(mPreviews.at(i).pcPath, i == miSelected) && i != miSelected)
			{
				miSelected = i;
				miSelectedAnimation = -1;
				mAnimationTime = 0s;
				mFlags.Set(ViewerFlags::kPlaying, false);
			}
		}
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Data directory"))
		{
			ImGui::TextWrapped("%ls", engine::gLaunchOptions.dataDirectory.c_str());
		}
		engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());
	}
	ImGui::End();
	float fLightingTop = bStackPanels ? fPanelsTop + fCatalogHeight + fMargin : fPanelCenter;
	float fLightingHeight = bStackPanels ? std::max(1.0f, fPanelHeight - fCatalogHeight - fMargin) : fCenteredPanelHeight;
	ImGui::SetNextWindowPos(ImVec2(pViewport->Pos.x + pViewport->Size.x - fMargin - fLightingWidth, fLightingTop), ImGuiCond_Always, ImVec2(0.0f, bStackPanels ? 0.0f : 0.5f));
	if (bStackPanels)
	{
		ImGui::SetNextWindowSize(ImVec2(fLightingWidth, fLightingHeight));
	}
	else
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(fLightingWidth, 0.0f), ImVec2(fLightingWidth, fLightingHeight));
	}
	char pcLightingTitle[32];
	std::snprintf(pcLightingTitle, sizeof(pcLightingTitle), "Preset: %c###Lighting", "WER"[miSelectedLightingPreset]);
	if (ImGui::Begin(pcLightingTitle, nullptr, iPanelWindowFlags | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		float fNormalFrameHeight = ImGui::GetFrameHeight();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (1.5f * fNormalFrameHeight - ImGui::GetFontSize()) / 2.0f));
		auto LightingSlider = [this, fLightingTrackWidth](const char* pcLabel, engine::Wrapper& rWrapper)
		{
			ImGui::BeginDisabled(mFlags & ViewerFlags::kComparingDefaults);
			ImGui::PushID(static_cast<int>(muiLightingRevision));
			ImGui::PushID(pcLabel);
			ImGui::TextUnformatted(pcLabel);
			ImGui::SetNextItemWidth(fLightingTrackWidth);
			float fValue = rWrapper.mfCurrent;
			if (ImGui::SliderFloat(mFlags & ViewerFlags::kComparingDefaults ? "##ComparisonValue" : "##Value", &fValue, rWrapper.mfMin, rWrapper.mfMax, "%.3f", ImGuiSliderFlags_AlwaysClamp) && !(mFlags & ViewerFlags::kComparingDefaults))
			{
				rWrapper.Set(fValue);
				std::array<float, 21> values {};
				for (const auto& [i, rDescriptor] : std::views::enumerate(kLightingDescriptors))
				{
					values.at(i) = rDescriptor.pWrapper->mfCurrent;
				}
				SaveLightingPreset(values);
			}
			ImGui::PopID();
			ImGui::PopID();
			ImGui::EndDisabled();
		};
		for (const LightingDescriptor& rDescriptor : kLightingDescriptors)
		{
			if (rDescriptor.pcHeading != nullptr)
			{
				ImGui::SeparatorText(rDescriptor.pcHeading);
			}
			LightingSlider(rDescriptor.pcLabel, *rDescriptor.pWrapper);
		}
		ImGui::PopStyleVar();
		engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());
	}
	ImGui::End();
	const ImGuiIO& rInputOutput = ImGui::GetIO();
	bool bCaptureMouse = rInputOutput.WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
	Camera* pCamera = static_cast<Camera*>(engine::gpCamera);
	float fViewportSize = std::min(pViewport->Size.x, pViewport->Size.y);
	if (bCaptureMouse || mPreviews.empty() || !ImGui::IsMousePosValid(&rInputOutput.MousePos) || fViewportSize <= 0.0f)
	{
		miDragButton = -1;
	}
	else
	{
		glm::vec2 mousePosition(2.0f * (rInputOutput.MousePos.x - pViewport->Pos.x - 0.5f * pViewport->Size.x) / fViewportSize, -2.0f * (rInputOutput.MousePos.y - pViewport->Pos.y - 0.5f * pViewport->Size.y) / fViewportSize);
		if (miDragButton >= 0 && !ImGui::IsMouseDown(static_cast<ImGuiMouseButton>(miDragButton)))
		{
			miDragButton = -1;
		}
		else if (miDragButton < 0)
		{
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				miDragButton = ImGui::IsMouseClicked(ImGuiMouseButton_Left) ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
				mPreviousMousePosition = mousePosition;
			}
		}
		else
		{
			glm::vec2 delta = mousePosition - mPreviousMousePosition;
			glm::vec2 anchor = mPreviousMousePosition;
			mPreviousMousePosition = mousePosition;
			float fDeltaLength = glm::length(delta);
			if (fDeltaLength > 0.0f)
			{
				float fAnchorLength = glm::length(anchor);
				if (fAnchorLength > 0.375f)
				{
					anchor *= 0.375f / fAnchorLength;
				}
				int64_t iSteps = std::max(1i64, static_cast<int64_t>(std::ceil(fDeltaLength / 0.25f)));
				glm::vec2 increment = delta / static_cast<float>(iSteps);
				for (int64_t i = 0; i < iSteps; ++i)
				{
					if (miDragButton == ImGuiMouseButton_Left)
					{
						pCamera->mArcBall.beginDrag(anchor);
						pCamera->mArcBall.drag(anchor + increment);
					}
					else
					{
						pCamera->RotateSun(anchor, anchor + increment);
					}
				}
			}
		}
	}
	if (!bCaptureMouse && !mPreviews.empty())
	{
		pCamera->Zoom(rInputOutput.MouseWheel);
	}
}

void RenderViewerUi()
{
	gpModelViewerApplication->RenderUi();
}

} // namespace game

namespace engine
{

void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, [[maybe_unused]] const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, [[maybe_unused]] std::chrono::duration<float> currentTime)
{
	engine::gpBufferManager->ResetSkinningAllocations(iCommandBuffer);
	engine::RenderLightingMain(iCommandBuffer);
	engine::RenderLightingSpreadIndirect(iCommandBuffer);
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(engine::gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ViewProjection[0]), XMMatrixTranspose(engine::gpCamera->mMatView * engine::gpCamera->mMatPerspective));
	XMStoreFloat4(&rMainLayout.f4EyePosition, engine::gpCamera->mVecEyePosition);
	XMStoreFloat4(&rMainLayout.f4ToEyeNormal, engine::gpCamera->mVecToEyeNormal);
	rMainLayout.iFrameNumber = static_cast<int32_t>(rRenderInterpolates.at(cameraCoordinate).iFrame);
	rMainLayout.iRenderNumber = static_cast<int32_t>(engine::gpGraphics->miFrameCounter);
	rMainLayout.fPhysicallyBasedRenderingLighting = 0.0f;
	rMainLayout.fPhysicallyBasedRenderingLightingSpecular = 0.0f;
	rMainLayout.fPhysicallyBasedRenderingSmoke = 0.0f;
	rMainLayout.fPhysicallyBasedRenderingShadowFloor = 1.0f;
	game::gpModelViewerApplication->RenderModel(iCommandBuffer);
}

} // namespace engine
