#include "ModelViewerApplication.h"

#include "File/PackChunks.h"
#include "Graphics/Camera.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

constexpr int64_t kiDynamicBufferInitialCapacity = 1;

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
	for (Preview& rPreview : mPreviews)
	{
		PreparePreview(rPreview);
	}
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
			engine::SkinningReservation reservation = engine::gAnimationDataMap.at(rPreview.crc).ReserveSkinning(iCommandBuffer, 1);
			for (int64_t j = 0; j < reservation.iMaterialCount; ++j)
			{
				common::MeshData meshData = rPreview.meshData.at(j);
				meshData.uiJointMatrixOffset += static_cast<uint32_t>(reservation.iJointBase);
				reservation.pMeshData[reservation.iMeshDataBase + j] = meshData;
			}
			std::copy(rPreview.jointMatrices.begin(), rPreview.jointMatrices.end(), reservation.pJointMatrices + reservation.iJointBase);
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
	engine::gpGraphics->RenderMainPresentAcquire(engine::gpSwapchainManager->miFramebufferIndex, mRenderInterpolates, mActiveCoordinates, engine::kOriginCoordinate, elapsedTime);
	return true;
}

void ModelViewerApplication::RenderUi()
{
	// Heap: ImGui retains window and draw storage between frames and rebuilds it after recreation.
	ScopedSuppressAllocationTracking suppress;
	float fScale = engine::UiScale();
	ImGui::SetNextWindowPos(ImVec2(32.0f * fScale, 32.0f * fScale), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(460.0f * fScale, 520.0f * fScale), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Model Viewer"))
	{
		ImGui::TextUnformatted("Left drag: orbit | Wheel: zoom");
		ImGui::SliderFloat("Sun angle", &engine::gpCamera->mfSunAngle, 0.0f, XM_2PI, "%.2f rad", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Separator();
		ImGui::Text("Scenes: %lld", static_cast<long long>(std::ssize(mPreviews)));
		ImGui::Separator();
		if (mPreviews.empty())
		{
			ImGui::TextWrapped("No selectable model. No renderable scenes were found.");
		}
		for (int64_t i = 0; i < std::ssize(mPreviews); ++i)
		{
			if (ImGui::Selectable(mPreviews.at(i).pcPath, i == miSelected))
			{
				miSelected = i;
			}
		}
		ImGui::Spacing();
		ImGui::Text("Framebuffer: %u x %u", engine::gpGraphics->mFramebufferVkExtent2D.width, engine::gpGraphics->mFramebufferVkExtent2D.height);
		ImGui::Text("Rendered frames: %lld", static_cast<long long>(miFrame));
		if (ImGui::CollapsingHeader("Data directory"))
		{
			ImGui::TextWrapped("%ls", engine::gLaunchOptions.dataDirectory.c_str());
		}
		engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());
	}
	ImGui::End();
	const ImGuiIO& rInputOutput = ImGui::GetIO();
	bool bCaptureMouse = rInputOutput.WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
	Camera* pCamera = static_cast<Camera*>(engine::gpCamera);
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || bCaptureMouse || mPreviews.empty())
	{
		mbOrbiting = false;
	}
	else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		mbOrbiting = true;
	}
	else if (mbOrbiting)
	{
		pCamera->Orbit(-0.01f * rInputOutput.MouseDelta.x, 0.01f * rInputOutput.MouseDelta.y);
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

void RenderViewerFrame(int64_t iCommandBuffer, const FrameInterpolate& rFrame)
{
	engine::gpBufferManager->ResetSkinningAllocations(iCommandBuffer);
	engine::RenderLightingMain(iCommandBuffer);
	engine::RenderLightingSpreadIndirect(iCommandBuffer);
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(engine::gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ViewProjection[0]), XMMatrixTranspose(engine::gpCamera->mMatView * engine::gpCamera->mMatPerspective));
	XMStoreFloat4(&rMainLayout.f4EyePosition, engine::gpCamera->mVecEyePosition);
	XMStoreFloat4(&rMainLayout.f4ToEyeNormal, engine::gpCamera->mVecToEyeNormal);
	rMainLayout.iFrameNumber = static_cast<int32_t>(rFrame.iFrame);
	rMainLayout.iRenderNumber = static_cast<int32_t>(engine::gpGraphics->miFrameCounter);
	rMainLayout.fPhysicallyBasedRenderingLighting = 0.0f;
	rMainLayout.fPhysicallyBasedRenderingLightingSpecular = 0.0f;
	rMainLayout.fPhysicallyBasedRenderingSmoke = 0.0f;
	rMainLayout.fPhysicallyBasedRenderingShadowFloor = 1.0f;
	gpModelViewerApplication->RenderModel(iCommandBuffer);
}

} // namespace game
