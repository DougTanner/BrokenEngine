#include "ModelViewerApplication.h"

#include "File/PackChunks.h"
#include "Frame/TerrainCollisionUtils.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/SunMoonWrappersBase.h"

#include "Graphics/Camera.h"
#include "Profile/ProfileRows.h"
#include "Ui/Screens/TweaksScreen/Tweaks.h"
#include "Game.h"

namespace game
{

constexpr int64_t kiDynamicBufferInitialCapacity = 1;
constexpr float kfFlightRadius = 1.5f;
constexpr float kfDefaultFlightHeight = 6.0f;

bool ModelViewerApplication::WorldVisible() const
{
	return (mFlags & ViewerFlags::kFlying) || mbWorldBackground;
}

void ModelViewerApplication::ResetDisplayTime()
{
	mDisplayTimer.Reset();
	engine::gpGraphics->mRenderFrameTimer.Reset();
	engine::gpProfileManager->DiscardSkippedFrameSamples();
}

void ModelViewerApplication::ToggleFlight()
{
	mFlags.Set(ViewerFlags::kFlying, !(mFlags & ViewerFlags::kFlying));
	if (mFlags & ViewerFlags::kFlying)
	{
		// Each flight starts at the engine's initial zoom, as a new Sandbox client does.
		engine::gpCamera->RestoreFraming(engine::CameraBase::kfCameraFramingInitial);
		engine::gpCamera->mbZooming = false;
	}
	miDragButton = -1;
	ResetDisplayTime();
}

void ModelViewerApplication::TogglePause()
{
	mFlags.Toggle(ViewerFlags::kPaused);
}

void ModelViewerApplication::SelectModel(int64_t iSelection)
{
	if (iSelection < 0 || iSelection >= std::ssize(mPreviews))
	{
		return;
	}
	if (iSelection == miSelected)
	{
		return;
	}
	miSelected = iSelection;
	miSelectedAnimation = -1;
	mAnimationTime = 0s;
	mFlags.Set(ViewerFlags::kPlaying, false);
	LoadModelMetadata();
}

struct LightingPreset
{
	static constexpr int64_t kiVersion = 4;
	std::array<float, 19> values {};
};

static_assert(sizeof(LightingPreset) == 76, "LightingPreset::kiVersion must be bumped with its layout or participant order");
static_assert(std::is_trivially_copyable_v<LightingPreset>);

constexpr std::array<const char*, 3> kLightingPresetPaths {"ModelViewerPresetW.bin", "ModelViewerPresetE.bin", "ModelViewerPresetR.bin"};

ModelViewerApplication::ModelViewerApplication()
: common::Singleton<ModelViewerApplication>(gpModelViewerApplication, this)
{
	mSeedRandomEngine.TimeSeed();
	// Strict comparison over the CRC-sorted list keeps the lowest CRC on a tie.
	float fTallestHeight = -std::numeric_limits<float>::infinity();
	for (common::crc_t islandCrc : engine::gpIslandTerrain->mIslandCrcsSorted)
	{
		float fPeakHeight = engine::gpIslandTerrain->mIslands.at(islandCrc).fPeakHeightMeters;
		if (fPeakHeight > fTallestHeight)
		{
			fTallestHeight = fPeakHeight;
			mTallestIslandCrc = islandCrc;
		}
	}
	GenerateIslands();
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
	if (!mPreviews.empty())
	{
		LoadModelMetadata();
	}
	SelectLightingPreset(0);
}

static std::filesystem::path FindExecutableCheckout()
{
	std::wstring executable(32'768, L'\0');
	int64_t iLength = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
	if (iLength == 0 || std::cmp_equal(iLength, executable.size()))
	{
		throw std::runtime_error("Cannot resolve ModelViewer executable path");
	}
	executable.resize(static_cast<std::size_t>(iLength));
	std::filesystem::path path = std::filesystem::canonical(std::filesystem::path(executable).parent_path());
	while (true)
	{
		std::filesystem::path gitPath = path / ".git";
		if (std::filesystem::is_regular_file(gitPath) || std::filesystem::is_directory(gitPath))
		{
			return path;
		}
		if (path == path.parent_path())
		{
			throw std::runtime_error("ModelViewer executable is outside a checkout");
		}
		path = path.parent_path();
	}
}

void ModelViewerApplication::GetAlignedPreviewBounds(FXMMATRIX matAlignment, XMFLOAT3A& rf3Center, float& rfRadius) const
{
	const Preview& rPreview = mPreviews.at(miSelected);
	XMStoreFloat3A(&rf3Center, XMVector3TransformCoord(XMLoadFloat3A(&rPreview.f3Center), matAlignment));
	float fMaximumScale = std::max({XMVectorGetX(XMVector3Length(matAlignment.r[0])), XMVectorGetX(XMVector3Length(matAlignment.r[1])), XMVectorGetX(XMVector3Length(matAlignment.r[2]))});
	rfRadius = rPreview.fRadius * fMaximumScale;
}

void ModelViewerApplication::ValidateModelAlignment()
{
	mFlags.Set(ViewerFlags::kMetadataValid, false);
	mModelAlignmentError.clear();
	for (const XMFLOAT3A* pf3Value : {&mf3ModelScale, &mf3ModelRotation, &mf3ModelOffset})
	{
		if (!std::isfinite(pf3Value->x) || !std::isfinite(pf3Value->y) || !std::isfinite(pf3Value->z))
		{
			mModelAlignmentError = "Alignment components must be finite";
			return;
		}
	}
	if (mf3ModelScale.x <= 0.0f || mf3ModelScale.y <= 0.0f || mf3ModelScale.z <= 0.0f)
	{
		mModelAlignmentError = "Scale XYZ must be positive";
		return;
	}
	common::ModelHeader modelHeader
	{
		.f3RotationRadians = {XMConvertToRadians(mf3ModelRotation.x), XMConvertToRadians(mf3ModelRotation.y), XMConvertToRadians(mf3ModelRotation.z)},
		.f3Scale = mf3ModelScale,
		.f3Offset = mf3ModelOffset,
	};
	XMMATRIX matAlignment = common::GetModelAlignment(modelHeader);
	XMVECTOR vecDeterminant;
	XMMATRIX matNormal = XMMatrixTranspose(XMMatrixInverse(&vecDeterminant, matAlignment));
	float fDeterminant = XMVectorGetX(vecDeterminant);
	if (XMMatrixIsNaN(matAlignment) || XMMatrixIsInfinite(matAlignment) || XMMatrixIsNaN(matNormal) || XMMatrixIsInfinite(matNormal) || !std::isfinite(fDeterminant) || fDeterminant == 0.0f)
	{
		mModelAlignmentError = "Derived alignment and normal matrix must be finite and invertible";
		return;
	}
	XMFLOAT3A f3Center {};
	float fRadius = 0.0f;
	GetAlignedPreviewBounds(matAlignment, f3Center, fRadius);
	if (!std::isfinite(f3Center.x) || !std::isfinite(f3Center.y) || !std::isfinite(f3Center.z) || !std::isfinite(fRadius) || fRadius <= 0.0f)
	{
		mModelAlignmentError = "Derived preview bounds must be finite with a positive radius";
		return;
	}
	mMatModelAlignment = matAlignment;
	miFittedSelection = -1;
	mFlags.Set(ViewerFlags::kMetadataValid);
}

void ModelViewerApplication::LoadModelMetadata()
{
	// Heap: selection loads checkout paths, JSON and status strings.
	ScopedSuppressAllocationTracking suppress;
	mFlags.Set(ViewerFlags::kMetadataLoaded, false);
	mf3ModelScale = {1.0f, 1.0f, 1.0f};
	mf3ModelRotation = {};
	mf3ModelOffset = {};
	meModelHitboxLevel = common::ModelHitboxLevel::kSphere;
	mfOutlineInset = common::ModelMetadata {}.fOutlineInsetMeters;
	mFlags.Set(ViewerFlags::kNavigationObstacle, false);
	mOutlineVertices.clear();
	mModelOutlineError.clear();
	mHardpoints.clear();
	meSelectedHardpointKind = common::ModelHardpointKind::kMissileTube;
	mFlags.Set(ViewerFlags::kResetHardpointTab);
	ValidateModelAlignment();
	mModelMetadataPath.clear();
	try
	{
		std::filesystem::path checkoutPath = FindExecutableCheckout();
		const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunks = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
		const common::SceneHeader& rScene = rChunks.at(mPreviews.at(miSelected).crc).pHeader->sceneHeader;
		std::filesystem::path sourceRelativePath(rChunks.at(rScene.modelCrc).pHeader->pcPath);
		sourceRelativePath.replace_extension();
		int64_t iMatches = 0;
		std::filesystem::path sourcePath;
		for (const std::filesystem::path& rData : {checkoutPath / "Engine/Data", checkoutPath / "Projects/BrokenEngineSandbox/Data"})
		{
			std::filesystem::path candidatePath = rData / sourceRelativePath;
			if (std::filesystem::is_regular_file(candidatePath))
			{
				++iMatches;
				sourcePath = candidatePath;
			}
		}
		if (iMatches != 1)
		{
			throw std::runtime_error(std::format("Expected one source for {} under {}; found {}", sourceRelativePath.string(), checkoutPath.string(), iMatches));
		}
		mModelMetadataPath = sourcePath;
		mModelMetadataPath += ".model.json";
		if (std::filesystem::exists(mModelMetadataPath))
		{
			common::ModelMetadata metadata = common::ReadModelMetadata(mModelMetadataPath);
			mf3ModelScale = metadata.f3Scale;
			mf3ModelRotation = metadata.f3RotationDegrees;
			mf3ModelOffset = metadata.f3Offset;
			meModelHitboxLevel = metadata.eHitboxLevel;
			mfOutlineInset = metadata.fOutlineInsetMeters;
			mFlags.Set(ViewerFlags::kNavigationObstacle, metadata.bNavigationObstacle);
			mHardpoints = std::move(metadata.hardpoints);
			ValidateModelAlignment();
			if (!(mFlags & ViewerFlags::kMetadataValid))
			{
				throw std::runtime_error(mModelAlignmentError);
			}
			mModelMetadataStatus = "Loaded source metadata";
		}
		else
		{
			mModelMetadataStatus = "No sidecar: identity defaults; Save creates source metadata";
		}
		mFlags.Set(ViewerFlags::kMetadataLoaded);
	}
	catch (const std::exception& rException)
	{
		mf3ModelScale = {1.0f, 1.0f, 1.0f};
		mf3ModelRotation = {};
		mf3ModelOffset = {};
		meModelHitboxLevel = common::ModelHitboxLevel::kSphere;
		mfOutlineInset = common::ModelMetadata {}.fOutlineInsetMeters;
		mFlags.Set(ViewerFlags::kNavigationObstacle, false);
		mHardpoints.clear();
		ValidateModelAlignment();
		mModelMetadataStatus = std::format("Load failed: {}", rException.what());
	}
	UpdateModelOutline();
}

const common::ModelOutline& ModelViewerApplication::GetSelectedModelOutline() const
{
	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const common::SceneHeader& rSceneHeader = rChunkMap.at(mPreviews.at(miSelected).crc).pHeader->sceneHeader;
	const engine::EagerChunk& rModelChunk = rChunkMap.at(rSceneHeader.modelCrc);
	return *reinterpret_cast<const common::ModelOutline*>(rModelChunk.pData + common::ModelHeader::OutlineOffset(rModelChunk.pHeader->iSize));
}

void ModelViewerApplication::UpdateModelOutline()
{
	const common::ModelOutline& rOutline = GetSelectedModelOutline();
	// A pack that is not kOutline holds the convex hull in its outline array, so a preview from it would look real.
	if (rOutline.eHitboxLevel != common::ModelHitboxLevel::kOutline)
	{
		return;
	}
	ASSERT(rOutline.iOutlineVertexCount >= 3 && rOutline.iOutlineVertexCount <= common::kiModelOutlineVertexMaximum);
	std::vector<XMFLOAT2> vertices = common::OffsetPolygon(std::span<const XMFLOAT2>(rOutline.pf2OutlineVertices, static_cast<size_t>(rOutline.iOutlineVertexCount)), rOutline.fOutlineInsetMeters - mfOutlineInset, common::kfModelOutlineSimplifyEpsilonMeters);
	if (std::ssize(vertices) < 3 || std::ssize(vertices) > common::kiModelOutlineVertexMaximum)
	{
		mModelOutlineError = std::format("Outline has {} vertices; it needs 3 to {}", std::ssize(vertices), common::kiModelOutlineVertexMaximum);
		return;
	}
	if (!common::PointInPolygon({0.0f, 0.0f}, vertices))
	{
		mModelOutlineError = "Outline does not contain the model origin";
		return;
	}
	mOutlineVertices = std::move(vertices);
	mModelOutlineError.clear();
}

void ModelViewerApplication::SaveModelMetadata()
{
	// Heap: explicit source Save formats and writes the sidecar.
	ScopedSuppressAllocationTracking suppress;
	// The tracked sidecars' layout: fixed key order, tab indentation and shortest round-trip floats.
	std::string content = std::format("{{\n\t\"version\": 6,\n\t\"rotationDegrees\": [{}, {}, {}],\n\t\"scale\": [{}, {}, {}],\n\t\"offset\": [{}, {}, {}],\n\t\"hitbox\": \"{}\",\n\t\"outlineInset\": {},\n\t\"navigationObstacle\": {},\n\t\"hardpoints\": [", mf3ModelRotation.x, mf3ModelRotation.y, mf3ModelRotation.z, mf3ModelScale.x, mf3ModelScale.y, mf3ModelScale.z, mf3ModelOffset.x, mf3ModelOffset.y, mf3ModelOffset.z, common::kpcModelHitboxLevelNames[static_cast<size_t>(meModelHitboxLevel)], mfOutlineInset, mFlags & ViewerFlags::kNavigationObstacle);
	for (const auto& [i, rHardpoint] : std::views::enumerate(std::as_const(mHardpoints)))
	{
		std::format_to(std::back_inserter(content), "{}\n\t\t{{\"kind\": \"{}\", \"position\": [{}, {}, {}], \"direction\": [{}, {}, {}]}}", i == 0 ? "" : ",", common::kpcModelHardpointKindNames[static_cast<size_t>(rHardpoint.eKind)], rHardpoint.f3Position.x, rHardpoint.f3Position.y, rHardpoint.f3Position.z, rHardpoint.f3Direction.x, rHardpoint.f3Direction.y, rHardpoint.f3Direction.z);
	}
	content += mHardpoints.empty() ? "]\n}\n" : "\n\t]\n}\n";
	std::filesystem::path stagingPath = mModelMetadataPath;
	stagingPath += ".tmp";
	std::ofstream stream(stagingPath, std::ios::binary);
	stream.write(content.data(), std::ssize(content));
	stream.close();
	std::error_code errorCode;
	if (!stream.fail())
	{
		std::filesystem::rename(stagingPath, mModelMetadataPath, errorCode);
	}
	if (stream.fail() || errorCode)
	{
		std::error_code removeErrorCode;
		std::filesystem::remove(stagingPath, removeErrorCode);
		mModelMetadataStatus = std::format("Save failed: {}", errorCode ? errorCode.message() : "cannot write staging file");
		return;
	}
	mModelMetadataStatus = "Saved source metadata. Game packs update on the next data build.";
}

struct LightingDescriptor
{
	engine::Wrapper* pWrapper = nullptr;
	const char* pcLabel = nullptr;
	const char* pcHeading = nullptr;
};

constexpr std::array<LightingDescriptor, 19> kLightingDescriptors
{
	LightingDescriptor {.pWrapper = &engine::gSunMoonMinimumAmbient, .pcLabel = "Sun Moon Minimum Ambient", .pcHeading = "Ambient and environment"},
	LightingDescriptor {.pWrapper = &engine::gSunMoonAmbientMultiplier, .pcLabel = "Sun Moon Ambient Multiplier", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrIblAmbient, .pcLabel = "Pbr Ibl Ambient", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrIblDiffuse, .pcLabel = "Pbr Ibl Diffuse", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrIblDiffusePower, .pcLabel = "Pbr Ibl Diffuse Power", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrIblSpecular, .pcLabel = "Pbr Ibl Specular", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrIblSpecularPower, .pcLabel = "Pbr Ibl Specular Power", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrCubemapLodPower, .pcLabel = "Pbr Cubemap Lod Power", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrCubemapLodOffset, .pcLabel = "Pbr Cubemap Lod Offset", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrBrdfDiffuse, .pcLabel = "Pbr Brdf Diffuse", .pcHeading = "BRDF"},
	LightingDescriptor {.pWrapper = &engine::gPbrBrdfDiffusePower, .pcLabel = "Pbr Brdf Diffuse Power", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrBrdfSpecular, .pcLabel = "Pbr Brdf Specular", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrBrdfSpecularPower, .pcLabel = "Pbr Brdf Specular Power", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gPbrEmissive, .pcLabel = "Pbr Emissive", .pcHeading = "Emissive"},
	LightingDescriptor {.pWrapper = &engine::gPbrExposure, .pcLabel = "Pbr Exposure", .pcHeading = "HDR color grading"},
	LightingDescriptor {.pWrapper = &engine::gPbrGamma, .pcLabel = "Pbr Gamma", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gColorGradingSaturation, .pcLabel = "Color Grading Saturation", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gColorGradingContrast, .pcLabel = "Color Grading Contrast", .pcHeading = nullptr},
	LightingDescriptor {.pWrapper = &engine::gColorGradingTemperature, .pcLabel = "Color Grading Temperature", .pcHeading = nullptr},
};

static std::array<float, 19> GetLightingDefaults()
{
	std::array<float, 19> values {};
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

void ModelViewerApplication::ApplyLightingValues(const std::array<float, 19>& rValues)
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

void ModelViewerApplication::SaveLightingPreset(const std::array<float, 19>& rValues)
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
	std::array<float, 19> values = GetLightingDefaults();
	ApplyLightingValues(values);
	SaveLightingPreset(values);
}

// Visits the rest position of every drawn index in draw order, so each consecutive triple is one drawn triangle.
template <typename VISITOR>
void ModelViewerApplication::ForEachRestPosition(const Preview& rPreview, VISITOR&& rVisitor) const
{
	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunks = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const engine::EagerChunk& rScene = rChunks.at(rPreview.crc);
	const common::SceneHeader& rSceneHeader = rScene.pHeader->sceneHeader;
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
			rVisitor(vecPosition);
		}
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
	XMVECTOR vecMin = XMVectorReplicate(std::numeric_limits<float>::max());
	XMVECTOR vecMax = XMVectorReplicate(-std::numeric_limits<float>::max());
	int64_t iDrawnVertices = 0;
	ForEachRestPosition(rPreview, [&vecMin, &vecMax, &iDrawnVertices](FXMVECTOR vecPosition)
	{
		vecMin = XMVectorMin(vecMin, vecPosition);
		vecMax = XMVectorMax(vecMax, vecPosition);
		++iDrawnVertices;
	});
	XMVECTOR vecCenter = XMVectorScale(XMVectorAdd(vecMin, vecMax), 0.5f);
	XMStoreFloat3A(&rPreview.f3Center, vecCenter);
	rPreview.fRadius = XMVectorGetX(XMVector3Length(XMVectorSubtract(vecMax, vecCenter)));
	if (iDrawnVertices == 0 || !std::isfinite(rPreview.fRadius) || rPreview.fRadius <= 0.0f)
	{
		throw std::ios_base::failure("ModelViewer empty bounds");
	}
	LOG(kGraphics, kInfo, "ModelViewer rest preview {}: materials {}, joints {}, center ({}, {}, {}), radius {}", rPreview.pcPath, rSceneHeader.uiMaterialCount, std::ssize(rPreview.jointMatrices), rPreview.f3Center.x, rPreview.f3Center.y, rPreview.f3Center.z, rPreview.fRadius);
}

void ModelViewerApplication::PickHardpoint()
{
	const ImGuiViewport* pViewport = ImGui::GetMainViewport();
	const Camera* pCamera = static_cast<const Camera*>(engine::gpCamera);
	const Preview& rPreview = mPreviews.at(miSelected);
	XMMATRIX matWorld = mMatModelAlignment;
	const ImVec2& rMousePosition = ImGui::GetIO().MousePos;
	XMVECTOR vecOrigin = XMVector3Unproject(XMVectorSet(rMousePosition.x, rMousePosition.y, 0.0f, 1.0f), pViewport->Pos.x, pViewport->Pos.y, pViewport->Size.x, pViewport->Size.y, 0.0f, 1.0f, pCamera->mMatModelPerspective, pCamera->mMatModelView, matWorld);
	XMVECTOR vecFar = XMVector3Unproject(XMVectorSet(rMousePosition.x, rMousePosition.y, 1.0f, 1.0f), pViewport->Pos.x, pViewport->Pos.y, pViewport->Size.x, pViewport->Size.y, 0.0f, 1.0f, pCamera->mMatModelPerspective, pCamera->mMatModelView, matWorld);
	XMVECTOR vecDirection = XMVector3Normalize(XMVectorSubtract(vecFar, vecOrigin));
	XMVECTOR pVecTriangle[3] {};
	int64_t iCorner = 0;
	float fNearestDistance = std::numeric_limits<float>::max();
	XMVECTOR vecNearestCross = XMVectorZero();
	ForEachRestPosition(rPreview, [&](FXMVECTOR vecPosition)
	{
		pVecTriangle[iCorner] = vecPosition;
		iCorner = (iCorner + 1) % 3;
		if (iCorner != 0)
		{
			return;
		}
		// Counter-clockwise winding is the front face the kCullBack model pipelines draw.
		XMVECTOR vecCross = XMVector3Cross(XMVectorSubtract(pVecTriangle[1], pVecTriangle[0]), XMVectorSubtract(pVecTriangle[2], pVecTriangle[0]));
		float fDistance = 0.0f;
		if (XMVectorGetX(XMVector3Dot(vecCross, vecDirection)) < 0.0f && TriangleTests::Intersects(vecOrigin, vecDirection, pVecTriangle[0], pVecTriangle[1], pVecTriangle[2], fDistance) && fDistance < fNearestDistance)
		{
			fNearestDistance = fDistance;
			vecNearestCross = vecCross;
		}
	});
	if (fNearestDistance == std::numeric_limits<float>::max())
	{
		return;
	}
	common::ModelHardpoint& rHardpoint = mHardpoints.emplace_back();
	rHardpoint.eKind = meSelectedHardpointKind;
	XMStoreFloat3A(&rHardpoint.f3Position, XMVectorMultiplyAdd(XMVectorReplicate(fNearestDistance), vecDirection, vecOrigin));
	XMStoreFloat3A(&rHardpoint.f3Direction, XMVector3Normalize(vecNearestCross));
	if (mFlags & ViewerFlags::kMetadataLoaded)
	{
		mModelMetadataStatus = "Unsaved source edits";
	}
}

void ModelViewerApplication::GenerateIslands()
{
	// Heap: island generation builds placement and hull vectors on seed changes.
	ScopedSuppressAllocationTracking suppress;
	// Steps the seed forward until the cell's centred placements hold the tallest template and another placement to
	// alternate with, and each one's peak lies inside the flight sweep's inset, where the body can arrive. It ends by
	// probability, not guarantee: no seed is certain to centre the tallest template in the cell.
	while (true)
	{
		engine::GenerateIslandChain(mIslandSeed, mIslandCell.islands);
		std::span<const engine::IslandPlacement> centredIslands = engine::CentredIslands(mIslandCell.islands);
		auto it = std::ranges::find(centredIslands, mTallestIslandCrc, &engine::IslandPlacement::islandCrc);
		bool bPeaksReachable = std::ranges::all_of(centredIslands, [](const engine::IslandPlacement& rPlacement)
		{
			XMFLOAT2 f2Peak = engine::gpIslandTerrain->PlacementPeakPosition(rPlacement);
			return common::IsInsideHex(XMLoadFloat2(&f2Peak), engine::kCellDirections, kfFlightRadius + 0.01f);
		});
		if (std::ssize(centredIslands) > 1 && it != centredIslands.end() && bPeaksReachable)
		{
			miTallestPlacement = it - centredIslands.begin();
			break;
		}
		mIslandSeed = engine::GridCoord::FromKey(mIslandSeed.ToKey() + 1);
	}
	engine::gpIslandTerrain->BuildElevationGrid(mIslandCell.islands, mIslandCell.elevationGrid);
	const XMFLOAT2& rf2Island = mIslandCell.islands.at(0).f2WorldPosition;
	float fBestScore = std::numeric_limits<float>::max();
	float fLowestHeight = engine::gpFlightPlanes->Lowest().fHeightMeters;
	for (float fY = -engine::kfCellHalfHeight; fY < engine::kfCellHalfHeight; fY += 8.0f)
	{
		for (float fX = -engine::kfCellHalfWidth; fX < engine::kfCellHalfWidth; fX += 8.0f)
		{
			XMVECTOR vecCandidate = XMVectorSet(fX, fY, fLowestHeight, 1.0f);
			if (!DiscClear(vecCandidate, 0.0f))
			{
				continue;
			}
			float fDistance = std::hypot(fX - rf2Island.x, fY - rf2Island.y);
			float fScore = std::abs(fDistance - 140.0f);
			if (fScore < fBestScore)
			{
				fBestScore = fScore;
				mVecWaterAnchor = vecCandidate;
			}
		}
	}
	if (fBestScore == std::numeric_limits<float>::max())
	{
		throw std::runtime_error("ModelViewer generated cell has no clear-water flight anchor");
	}
	ResetFlight();
}

bool ModelViewerApplication::DiscClear(FXMVECTOR vecPosition, float fHeight) const
{
	if (!common::IsInsideHex(vecPosition, engine::kCellDirections, kfFlightRadius + 0.01f))
	{
		return false;
	}
	engine::NavData navigation;
	engine::DiscTerrainResult result = engine::ResolveDiscAgainstTerrain(mIslandCell, navigation, vecPosition, vecPosition, kfFlightRadius, fHeight, false);
	return !(result.flags & engine::DiscTerrainFlags::kStartOverlap);
}

void ModelViewerApplication::ResetFlight()
{
	mfFlightHeight = kfDefaultFlightHeight;
	mFlightBody = {.vecPosition = XMVectorSetZ(mVecWaterAnchor, mfFlightHeight)};
	miDestinationPlacement = -1;
	SelectDestination();
	mDisplayTimer.Reset();
}

void ModelViewerApplication::SelectDestination()
{
	if (miDestinationPlacement == miTallestPlacement)
	{
		// Random is inclusive, so size - 2 draws one of the other placements, skipping past the tallest.
		miDestinationPlacement = common::Random(std::ssize(engine::CentredIslands(mIslandCell.islands)) - 2, mSeedRandomEngine);
		if (miDestinationPlacement >= miTallestPlacement)
		{
			++miDestinationPlacement;
		}
	}
	else
	{
		miDestinationPlacement = miTallestPlacement;
	}
	XMFLOAT2 f2Destination = engine::gpIslandTerrain->PlacementPeakPosition(mIslandCell.islands.at(miDestinationPlacement));
	mFlightBody.vecDestination = XMVectorSet(f2Destination.x, f2Destination.y, 0.0f, 1.0f);
}

void ModelViewerApplication::UpdateFlight(float fDeltaTime)
{
	FlightBody& rBody = mFlightBody;
	XMVECTOR vecToDestination = XMVectorSetZ(XMVectorSubtract(rBody.vecDestination, rBody.vecPosition), 0.0f);
	if (XMVectorGetX(XMVector3LengthSq(vecToDestination)) < 20.0f * 20.0f)
	{
		SelectDestination();
		vecToDestination = XMVectorSetZ(XMVectorSubtract(rBody.vecDestination, rBody.vecPosition), 0.0f);
	}
	if (XMVectorGetX(XMVector3LengthSq(vecToDestination)) > 0.0f)
	{
		// Turning in proportion to the heading error keeps small errors from chattering; gain 6 critically damps the 6/s
		// response and decay.
		float fHeadingError = std::atan2(XMVectorGetZ(XMVector3Cross(rBody.vecDirection, vecToDestination)), XMVectorGetX(XMVector3Dot(rBody.vecDirection, vecToDestination)));
		float fWantedTurn = std::clamp(6.0f * fHeadingError, -4.0f, 4.0f);
		rBody.fDeltaRotation = std::lerp(fWantedTurn, rBody.fDeltaRotation, common::ExponentialDecay(6.0f, fDeltaTime));
	}
	rBody.fDeltaRotation *= common::ExponentialDecay(6.0f, fDeltaTime);
	float fHeight = XMVectorGetZ(rBody.vecPosition);
	float fNextHeight = fHeight + std::clamp(mfFlightHeight - fHeight, -25.0f * fDeltaTime, 25.0f * fDeltaTime);
	rBody.fClimbTilt = fDeltaTime > 0.0f ? 0.35f * (fNextHeight - fHeight) / (25.0f * fDeltaTime) : rBody.fClimbTilt;
	rBody.vecDirection = common::RotateDirectionAboutZ(rBody.vecDirection, fDeltaTime * rBody.fDeltaRotation);
	rBody.vecVelocity = engine::ApplyMovement<true>(rBody.vecVelocity, rBody.vecDirection, fDeltaTime, 10.0f, 0.25f, 40.0f, 4.0f);
	XMVECTOR vecNext = XMVectorSetZ(XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime), rBody.vecVelocity, rBody.vecPosition), fNextHeight);
	// Sweep the disc against the inset cell so every accepted segment stays in the original cell.
	static constexpr common::HexDirections kFlightCell = common::MakeHexDirections(engine::kfCellApothem - kfFlightRadius - 0.01f);
	if (std::optional<float> optionalExit = common::TraceHexExit(rBody.vecPosition, vecNext, kFlightCell))
	{
		int64_t iEdge = common::HexExitDirection(vecNext, kFlightCell);
		XMVECTOR vecNormal = XMVectorNegate(XMVector2Normalize(XMLoadFloat2(&kFlightCell.pEntries[iEdge].f2CenterOffset)));
		float fDistance = XMVectorGetX(XMVector2Length(XMVectorSubtract(vecNext, rBody.vecPosition)));
		vecNext = XMVectorSetZ(XMVectorLerp(rBody.vecPosition, vecNext, std::max(0.0f, *optionalExit - 0.001f / fDistance)), fNextHeight);
		rBody.vecVelocity = XMVector3Reflect(rBody.vecVelocity, vecNormal);
		rBody.vecDirection = common::ReflectDirection(rBody.vecDirection, vecNormal);
	}
	rBody.vecPosition = XMVectorSetW(vecNext, 1.0f);
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
		if (i != miSelected)
		{
			engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(rPreview.crc, iCommandBuffer, 0);
			continue;
		}
		shaders::ModelLayout& rLayout = *engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(rPreview.crc, engine::kBufferMain, iCommandBuffer).pData;
		rLayout = {};
		const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
		const common::SceneHeader& rSceneHeader = rChunkMap.at(rPreview.crc).pHeader->sceneHeader;
		const engine::EagerChunk& rModelChunk = rChunkMap.at(rSceneHeader.modelCrc);
		engine::ModelShadowBounds shadowBounds = engine::ComputeModelShadowBounds(*reinterpret_cast<const common::ModelOutline*>(rModelChunk.pData + common::ModelHeader::OutlineOffset(rModelChunk.pHeader->iSize)));
		XMMATRIX matPackedAlignment = common::GetModelAlignment(rModelChunk.pHeader->modelHeader);
		XMMATRIX matAlignmentDelta = XMMatrixInverse(nullptr, matPackedAlignment) * mMatModelAlignment;
		XMMATRIX matInstanceTransform = XMMatrixIdentity();
		XMVECTOR vecPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		if (mFlags & ViewerFlags::kFlying)
		{
			XMMATRIX matFlightRotation = XMMatrixRotationX(-0.2f * mFlightBody.fDeltaRotation) * XMMatrixRotationY(-mFlightBody.fClimbTilt) * common::RotationMatrixFromDirection(mFlightBody.vecDirection, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
			matInstanceTransform = matFlightRotation * XMMatrixTranslationFromVector(mFlightBody.vecPosition);
			vecPosition = mFlightBody.vecPosition;
			XMStoreFloat4(&rLayout.f4PositionLayer, mFlightBody.vecPosition);
			rLayout.f4PositionLayer.w = static_cast<float>(engine::gpFlightPlanes->NearestPlaneIndex(XMVectorGetZ(mFlightBody.vecPosition)));
		}
		XMMATRIX matTransform = mMatModelAlignment * matInstanceTransform;
		rLayout.f2ShadowWorldXRange = {engine::ComputeModelShadowRange(shadowBounds, matAlignmentDelta * matInstanceTransform, vecPosition)};
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rLayout.f3x4Transform[0]), matTransform);
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));
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
		engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(rPreview.crc, iCommandBuffer, 1);
		if (!(mFlags & ViewerFlags::kFlying))
		{
			engine::gpPipelineManager->mDynamicPipelines.mpModelPipelineMaps[engine::kDynamicModelPipelineModelShadow].at(rPreview.crc)->WriteIndirectBuffer(iCommandBuffer, 0);
		}
	}
}

bool ModelViewerApplication::Render()
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerFrameUpdate);
	bool bDrawOverWorld = !(mFlags & ViewerFlags::kFlying);
	if ((engine::gpGraphics->mFlags & engine::GraphicsFlags::kDrawModelsOverWorld) != bDrawOverWorld)
	{
		engine::gpGraphics->mFlags.Set(engine::GraphicsFlags::kDrawModelsOverWorld, bDrawOverWorld);
		engine::gpGraphics->meDestroyType = std::max(engine::gpGraphics->meDestroyType, engine::DestroyType::kCommandBuffers);
	}
	VkClearColorValue& rVkHdrClearColor = engine::gpGraphics->mHdrVkClearColorValue;
	if (rVkHdrClearColor.float32[0] != mfBackgroundGray)
	{
		// Alpha keeps its initial ungraded marker.
		rVkHdrClearColor.float32[0] = mfBackgroundGray;
		rVkHdrClearColor.float32[1] = mfBackgroundGray;
		rVkHdrClearColor.float32[2] = mfBackgroundGray;
		engine::gpGraphics->meDestroyType = std::max(engine::gpGraphics->meDestroyType, engine::DestroyType::kCommandBuffers);
	}
	engine::gpGraphics->Refresh();
	if (engine::gpGraphics->mbSwapchainRecreateDeferred || engine::gpGraphics->meDestroyType != engine::DestroyType::kNone)
	{
		bool bAcquire = engine::gpGraphics->mbSwapchainRecreateDeferred || engine::gpGraphics->meDestroyType >= engine::DestroyType::kSwapchain;
		engine::gpGraphics->Create();
		ResetDisplayTime();
		if (engine::gpGraphics->mbSwapchainRecreateDeferred)
		{
			engine::gpTextureUploadManager->SignalFrame();
			return false;
		}
		if (bAcquire)
		{
			engine::gpSwapchainManager->AcquireNextImage();
			ResetDisplayTime();
		}
		if (engine::gpGraphics->meDestroyType >= engine::DestroyType::kSwapchain)
		{
			engine::gpTextureUploadManager->SignalFrame();
			return false;
		}
	}
	float fDeltaTime = std::chrono::duration<float>(mDisplayTimer.GetDeltaNs(true)).count();
	Camera* pCamera = static_cast<Camera*>(engine::gpCamera);
	if (!mPreviews.empty() && miFittedSelection != miSelected)
	{
		XMFLOAT3A f3Center {};
		float fRadius = 0.0f;
		GetAlignedPreviewBounds(mMatModelAlignment, f3Center, fRadius);
		pCamera->Fit(f3Center, fRadius);
		miFittedSelection = miSelected;
	}
	if (mFlags & ViewerFlags::kFlying)
	{
		if (!(mFlags & ViewerFlags::kPaused))
		{
			UpdateFlight(fDeltaTime);
		}
		mCameraFrame.iTick = miFrame;
		mCameraFrame.renderBasis = engine::MakeRenderBasis(engine::kOriginCoordinate, engine::kOriginCoordinate);
		pCamera->UpdateFlight(mCameraFrame, mFlightBody.vecPosition, fDeltaTime);
	}
	else
	{
		pCamera->mVecPosition = mVecWaterAnchor;
		pCamera->Update();
	}
	const engine::CellStaticData* pIslandCell = &mIslandCell;
	engine::gpIslands->UpdateActiveIslands(std::span<const engine::CellStaticData* const>(&pIslandCell, WorldVisible() ? 1 : 0));
	FrameInterpolate& rFrame = mRenderInterpolates.at(engine::kOriginCoordinate);
	rFrame.iFrame = miFrame++;
	std::chrono::nanoseconds frameTime = mTimer.GetDeltaNs(true);
	if (!(mFlags & ViewerFlags::kPaused))
	{
		mElapsedTime += frameTime;
	}
	std::chrono::duration<float> elapsedTime(mElapsedTime);
	engine::gpGraphics->RenderGlobal(elapsedTime);
	if ((mFlags & ViewerFlags::kPlaying) && !(mFlags & ViewerFlags::kPaused))
	{
		const engine::AnimationData& rAnimation = engine::gAnimationDataMap.at(mPreviews.at(miSelected).crc);
		float fDuration = rAnimation.mpAnimations[miSelectedAnimation].fDuration;
		std::chrono::duration<float> deltaTime(engine::gpGraphics->mRenderFrameDeltaNanoseconds);
		mAnimationTime = fDuration > 0.0f ? std::chrono::duration<float>(std::fmod((mAnimationTime + deltaTime).count(), fDuration)) : 0s;
	}
	engine::gpGraphics->RenderMainPresentAcquire(engine::gpSwapchainManager->miFramebufferIndex, mRenderInterpolates, mActiveCoordinates, engine::kOriginCoordinate, elapsedTime);
	if (engine::gpGraphics->mFlags & engine::GraphicsFlags::kRecreated)
	{
		ResetDisplayTime();
	}
	return true;
}

void ModelViewerApplication::RenderUi()
{
	// Heap: ImGui retains window and draw storage between frames and rebuilds it after recreation, and an outline inset
	// edit rebuilds the previewed outline.
	ScopedSuppressAllocationTracking suppress;
	const ImGuiViewport* pViewport = ImGui::GetMainViewport();
	Camera* pCamera = static_cast<Camera*>(engine::gpCamera);
	// True only on a frame whose drawn right window selects the Edit Hardpoints tab in orbit with Rest pose: the one
	// gate for picking and hardpoint markers, whose entries are rest-pose coordinates drawn through the accepted alignment.
	bool bHardpointsActive = false;
	// True only on a frame whose drawn right window selects the Collision tab in orbit with Rest pose on a model with a
	// packed outline: the one gate for the inset slider and the outline overlay, whose lines are in the rest model's
	// aligned space.
	bool bOutlineActive = false;
	if (!engine::gbShowImGui)
	{
		float fScale = engine::UiScale();
		float fMargin = 16.0f * fScale;
		float fSunTrackWidth = 0.5f * pViewport->Size.x;
		float fSunWindowWidth = fSunTrackWidth + 2.0f * ImGui::GetStyle().WindowPadding.x;
		ImGuiWindowFlags iWindowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
		ImGui::SetNextWindowPos(ImVec2(pViewport->Pos.x + 0.5f * (pViewport->Size.x - fSunWindowWidth), pViewport->Pos.y + fMargin));
		ImGui::SetNextWindowSize(ImVec2(fSunWindowWidth, 0.0f));
		if (ImGui::Begin("Sun angle", nullptr, iWindowFlags | ImGuiWindowFlags_NoTitleBar))
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
		float fPanelsBottom = pViewport->Pos.y + pViewport->Size.y - fMargin;
		float fPanelHeight = std::max(1.0f, fPanelsBottom - fPanelsTop);
		float fPanelCenter = pViewport->Pos.y + 0.5f * pViewport->Size.y;
		float fCenteredPanelHeight = std::max(1.0f, 2.0f * std::min(fPanelCenter - fPanelsTop, fPanelsBottom - fPanelCenter));
		float fCatalogWidth = std::min(460.0f * fScale, 0.4f * pViewport->Size.x);
		float fAvailableWidth = std::max(1.0f, pViewport->Size.x - 2.0f * fMargin);
		float fLightingTrackWidth = 2.0f * std::max(1.0f, std::min(340.0f * fScale, 0.4f * pViewport->Size.x) - 2.0f * ImGui::GetStyle().WindowPadding.x);
		float fLightingWidth = fLightingTrackWidth + 2.0f * ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ScrollbarSize;
		bool bStackPanels = fCatalogWidth + fLightingWidth + 3.0f * fMargin > pViewport->Size.x || fPanelsTop >= fPanelCenter;
		float fCatalogHeight = bStackPanels ? std::max(1.0f, (fPanelHeight - fMargin) * 0.5f) : fCenteredPanelHeight;
		fCatalogWidth = bStackPanels ? fAvailableWidth : fCatalogWidth;
		fLightingWidth = std::min(fLightingWidth, fAvailableWidth);
		ImGuiWindowFlags iPanelWindowFlags = iWindowFlags | (bStackPanels ? 0 : ImGuiWindowFlags_AlwaysAutoResize);
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
			ImGui::BeginDisabled(mFlags & ViewerFlags::kFlying);
			ImGui::Checkbox("World background", &mbWorldBackground);
			ImGui::EndDisabled();
			ImGui::BeginDisabled(WorldVisible());
			ImGui::TextUnformatted("Background gray");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			// Reversed range puts white at the left.
			ImGui::SliderFloat("##Background gray", &mfBackgroundGray, 1.0f, 0.0f, "%.3f", ImGuiSliderFlags_NoInput);
			ImGui::EndDisabled();
			ImGui::Separator();
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
			if (engine::WrapperToggle("Multisampling", &engine::gMultisampling) && !engine::gMultisampling.Get<bool>())
			{
				engine::gSampleShading.Set(false);
			}
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
			engine::WrapperToggle("Sample Shading", &engine::gSampleShading);
			ImGui::BeginDisabled(!engine::gSampleShading.Get<bool>());
			QualitySlider("Minimum Sample Shading", engine::gMinimumSampleShading);
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			engine::WrapperToggle("Anisotropy", &engine::gAnisotropy);
			ImGui::BeginDisabled(!engine::gAnisotropy.Get<bool>());
			QualitySlider("Maximum Anisotropy", engine::gMaximumAnisotropy);
			ImGui::EndDisabled();
			ImGui::PopStyleVar();
			ImGui::Separator();
			ImGui::SeparatorText("Islands");
			ImGui::TextUnformatted((mFlags & ViewerFlags::kFlying) ? "TAB: orbit preview" : "TAB: autonomous flight");
			ImGui::TextUnformatted((mFlags & ViewerFlags::kPaused) ? "SPACE: resume" : "SPACE: pause");
			if (ImGui::Button("Reroll"))
			{
				mIslandSeed = engine::GridCoord::FromKey(common::RandomNext(mSeedRandomEngine));
				GenerateIslands();
			}
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
					SelectModel(i);
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
		ImGui::SetNextWindowSize(ImVec2(fLightingWidth, fLightingHeight));
		char pcLightingTitle[32];
		std::snprintf(pcLightingTitle, sizeof(pcLightingTitle), "Preset: %c###Lighting", "WER"[miSelectedLightingPreset]);
		if (ImGui::Begin(pcLightingTitle, nullptr, iWindowFlags | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			auto SourceMetadataControls = [this]()
			{
				ImGui::TextWrapped("Source: %ls", mModelMetadataPath.c_str());
				ImGui::BeginDisabled(!(mFlags & ViewerFlags::kMetadataLoaded) || !(mFlags & ViewerFlags::kMetadataValid) || !mModelOutlineError.empty());
				if (ImGui::Button("Save"))
				{
					SaveModelMetadata();
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::TextWrapped("%s", mModelMetadataStatus.c_str());
				if (!mModelAlignmentError.empty())
				{
					ImGui::TextWrapped("%s", mModelAlignmentError.c_str());
				}
				if (!mModelOutlineError.empty())
				{
					ImGui::TextWrapped("%s", mModelOutlineError.c_str());
				}
				ImGui::TextWrapped("Save changes this checkout's source. Games use it after their next data build.");
			};
			if (ImGui::BeginTabBar("Panels", ImGuiTabBarFlags_AutoSelectNewTabs))
			{
				if (ImGui::BeginTabItem("Lighting"))
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
							std::array<float, 19> values {};
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
					ImGui::EndTabItem();
				}
				if ((mFlags & ViewerFlags::kFlying) && ImGui::BeginTabItem("Flight"))
				{
					auto SliderRows = [fScale](const char* pcLabel, std::span<float* const> values, std::span<const char* const> rowLabels, float fMin, float fMax)
					{
						bool bChanged = false;
						ImGui::PushID(pcLabel);
						ImGui::SeparatorText(pcLabel);
						for (const auto& [i, pfValue] : std::views::enumerate(values))
						{
							ImGui::PushID(rowLabels[i]);
							ImGui::TextUnformatted(rowLabels[i]);
							ImGui::SameLine();
							float fInputWidth = 65.0f * fScale;
							ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x - fInputWidth - ImGui::GetStyle().ItemSpacing.x));
							float fValue = std::isfinite(*pfValue) ? *pfValue : fMin;
							if (ImGui::SliderFloat("##Slider", &fValue, fMin, fMax, "%.3f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoRoundToFormat))
							{
								*pfValue = fValue;
								bChanged = true;
							}
							ImGui::SameLine();
							ImGui::SetNextItemWidth(fInputWidth);
							bChanged |= ImGui::InputFloat("##Exact", pfValue, 0.0f, 0.0f, "%.9g");
							ImGui::PopID();
						}
						ImGui::PopID();
						return bChanged;
					};
					const char* ppcAxes[] {"X", "Y", "Z"};
					bool bAlignmentChanged = SliderRows("Scale", std::array {&mf3ModelScale.x, &mf3ModelScale.y, &mf3ModelScale.z}, ppcAxes, 0.01f, 10.0f);
					bAlignmentChanged |= SliderRows("Rotation (degrees)", std::array {&mf3ModelRotation.x, &mf3ModelRotation.y, &mf3ModelRotation.z}, ppcAxes, -180.0f, 180.0f);
					bAlignmentChanged |= SliderRows("Offset", std::array {&mf3ModelOffset.x, &mf3ModelOffset.y, &mf3ModelOffset.z}, ppcAxes, -1.0f, 1.0f);
					if (bAlignmentChanged)
					{
						ValidateModelAlignment();
						if (mFlags & ViewerFlags::kMetadataLoaded)
						{
							mModelMetadataStatus = "Unsaved source edits";
						}
					}
					// ImGui parses inf and nan; NaN would make the body position NaN and inf would make it climb without bound, so keep the last value.
					float fFlightHeight = mfFlightHeight;
					if (SliderRows("Height (m)", std::array {&fFlightHeight}, std::array {""}, 0.0f, 100.0f) && std::isfinite(fFlightHeight))
					{
						mfFlightHeight = fFlightHeight;
					}
					SourceMetadataControls();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Edit Hardpoints"))
				{
					bHardpointsActive = !(mFlags & ViewerFlags::kFlying) && miSelectedAnimation < 0;
					if (ImGui::BeginTabBar("Hardpoint categories"))
					{
						auto EditHardpoints = [&](common::ModelHardpointKind eKind)
						{
							meSelectedHardpointKind = eKind;
							ImGui::PushID(static_cast<int>(eKind));
							ImGui::BeginDisabled(!bHardpointsActive);
							bool bPicking = mFlags & ViewerFlags::kPicking;
							if (ImGui::Checkbox("Pick", &bPicking))
							{
								mFlags.Set(ViewerFlags::kPicking, bPicking);
							}
							ImGui::EndDisabled();
							int64_t iDeleted = -1;
							for (const auto& [i, rHardpoint] : std::views::enumerate(std::as_const(mHardpoints)))
							{
								if (rHardpoint.eKind != eKind)
								{
									continue;
								}
								ImGui::PushID(static_cast<int>(i));
								if (ImGui::SmallButton("X"))
								{
									iDeleted = i;
								}
								ImGui::SameLine();
								ImGui::Text("(%.3f, %.3f, %.3f) dir (%.3f, %.3f, %.3f)", rHardpoint.f3Position.x, rHardpoint.f3Position.y, rHardpoint.f3Position.z, rHardpoint.f3Direction.x, rHardpoint.f3Direction.y, rHardpoint.f3Direction.z);
								ImGui::PopID();
							}
							if (iDeleted >= 0)
							{
								mHardpoints.erase(mHardpoints.begin() + iDeleted);
								if (mFlags & ViewerFlags::kMetadataLoaded)
								{
									mModelMetadataStatus = "Unsaved source edits";
								}
							}
							ImGui::PopID();
						};
						if (ImGui::BeginTabItem("Missile tubes", nullptr, (mFlags & ViewerFlags::kResetHardpointTab) ? ImGuiTabItemFlags_SetSelected : 0))
						{
							EditHardpoints(common::ModelHardpointKind::kMissileTube);
							mFlags.Set(ViewerFlags::kResetHardpointTab, false);
							ImGui::EndTabItem();
						}
						if (ImGui::BeginTabItem("Fighter launch bays"))
						{
							if (!(mFlags & ViewerFlags::kResetHardpointTab))
							{
								EditHardpoints(common::ModelHardpointKind::kFighterLaunchBay);
							}
							ImGui::EndTabItem();
						}
						bHardpointsActive = bHardpointsActive && !(mFlags & ViewerFlags::kResetHardpointTab);
						ImGui::EndTabBar();
					}
					SourceMetadataControls();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Collision"))
				{
					bool bPackedOutline = !mPreviews.empty() && GetSelectedModelOutline().eHitboxLevel == common::ModelHitboxLevel::kOutline;
					bOutlineActive = bPackedOutline && !(mFlags & ViewerFlags::kFlying) && miSelectedAnimation < 0;
					ImGui::BeginDisabled(!bOutlineActive);
					// ImGui parses inf and nan; keep the last value, as Height (m) does.
					float fOutlineInset = mfOutlineInset;
					if (ImGui::SliderFloat("Inset (m)", &fOutlineInset, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp) && std::isfinite(fOutlineInset))
					{
						mfOutlineInset = fOutlineInset;
						if (mFlags & ViewerFlags::kMetadataLoaded)
						{
							mModelMetadataStatus = "Unsaved source edits";
						}
						UpdateModelOutline();
					}
					ImGui::EndDisabled();
					if (bPackedOutline)
					{
						ImGui::Text("Outline vertices: %lld", static_cast<long long>(std::ssize(mOutlineVertices)));
					}
					else
					{
						ImGui::TextWrapped("This model has no packed outline. A data build applies a hitbox change.");
					}
					SourceMetadataControls();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());
		}
		ImGui::End();
		if (bHardpointsActive && !mPreviews.empty())
		{
			// Picked hardpoints need no near-plane clipping: the orbit eye stays over 1.4 fitted radii from the aligned center,
			// and a picked surface position plus its unit direction scaled by 0.1 rest radius lies within 1.1 fitted radii.
			const Preview& rPreview = mPreviews.at(miSelected);
			XMMATRIX matWorld = mMatModelAlignment;
			ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
			for (const common::ModelHardpoint& rHardpoint : mHardpoints)
			{
				ImU32 uiMarkerColor = rHardpoint.eKind == common::ModelHardpointKind::kMissileTube ? IM_COL32(255, 96, 0, 255) : IM_COL32(0, 224, 255, 255);
				XMVECTOR vecPosition = XMVectorSetW(XMLoadFloat3A(&rHardpoint.f3Position), 1.0f);
				XMVECTOR vecEnd = XMVectorMultiplyAdd(XMLoadFloat3A(&rHardpoint.f3Direction), XMVectorReplicate(0.1f * rPreview.fRadius), vecPosition);
				XMVECTOR vecScreenPosition = XMVector3Project(vecPosition, pViewport->Pos.x, pViewport->Pos.y, pViewport->Size.x, pViewport->Size.y, 0.0f, 1.0f, pCamera->mMatModelPerspective, pCamera->mMatModelView, matWorld);
				XMVECTOR vecScreenEnd = XMVector3Project(vecEnd, pViewport->Pos.x, pViewport->Pos.y, pViewport->Size.x, pViewport->Size.y, 0.0f, 1.0f, pCamera->mMatModelPerspective, pCamera->mMatModelView, matWorld);
				ImVec2 screenPosition(XMVectorGetX(vecScreenPosition), XMVectorGetY(vecScreenPosition));
				pDrawList->AddLine(screenPosition, ImVec2(XMVectorGetX(vecScreenEnd), XMVectorGetY(vecScreenEnd)), uiMarkerColor, 2.0f * fScale);
				pDrawList->AddCircleFilled(screenPosition, 4.0f * fScale, uiMarkerColor);
			}
		}
		if (bOutlineActive)
		{
			// These points need no near-plane clipping for the reason the hardpoint markers do: they lie within one fitted
			// radius of the aligned center. Outline vertices are already aligned, so their world matrix is identity.
			const common::ModelOutline& rOutline = GetSelectedModelOutline();
			float fOutlineZ = 0.5f * (rOutline.fMinimumZ + rOutline.fMaximumZ);
			ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
			auto DrawOutline = [&](std::span<const XMFLOAT2> vertices, ImU32 uiColor, float fThickness)
			{
				ImVec2 pScreenPoints[common::kiModelOutlineVertexMaximum] {};
				for (const auto& [i, rVertex] : std::views::enumerate(vertices))
				{
					XMVECTOR vecScreen = XMVector3Project(XMVectorSet(rVertex.x, rVertex.y, fOutlineZ, 1.0f), pViewport->Pos.x, pViewport->Pos.y, pViewport->Size.x, pViewport->Size.y, 0.0f, 1.0f, pCamera->mMatModelPerspective, pCamera->mMatModelView, XMMatrixIdentity());
					pScreenPoints[i] = ImVec2(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen));
				}
				pDrawList->AddPolyline(pScreenPoints, static_cast<int>(std::ssize(vertices)), uiColor, fThickness, ImDrawFlags_Closed);
			};
			// Magenta is what the game holds; green is the slider's approximation of the next bake, drawn on top.
			DrawOutline(std::span<const XMFLOAT2>(rOutline.pf2OutlineVertices, static_cast<size_t>(rOutline.iOutlineVertexCount)), IM_COL32(255, 0, 255, 255), 4.0f * fScale);
			DrawOutline(mOutlineVertices, IM_COL32(0, 255, 0, 255), 1.5f * fScale);
		}
	}
	// Rewritten every frame because a surface-tier recreation replaces the manager and its text areas.
	engine::gpImGuiManager->UpdateTextArea(engine::kTextDebug, (mFlags & ViewerFlags::kPaused) ? "PAUSED" : "");
	engine::gpImGuiManager->RenderTextAreas();
	const ImGuiIO& rInputOutput = ImGui::GetIO();
	bool bCaptureMouse = rInputOutput.WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
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
			// A picking click starts no drag, so neither the click nor holding it afterwards orbits.
			if (bHardpointsActive && (mFlags & ViewerFlags::kPicking) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				PickHardpoint();
			}
			else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
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
					if (miDragButton == ImGuiMouseButton_Left && !(mFlags & ViewerFlags::kFlying))
					{
						pCamera->mArcBall.beginDrag(anchor);
						pCamera->mArcBall.drag(anchor + increment);
					}
					else if (miDragButton == ImGuiMouseButton_Right)
					{
						pCamera->RotateSun(anchor, anchor + increment);
					}
				}
			}
		}
	}
	int64_t iScrollDelta = 0;
	if (!bCaptureMouse && !mPreviews.empty())
	{
		if (mFlags & ViewerFlags::kFlying)
		{
			// ImGui reports wheel notches; the engine framing zoom consumes raw Win32 wheel units, as Sandbox's Input does.
			iScrollDelta = std::lround(rInputOutput.MouseWheel * WHEEL_DELTA);
		}
		else
		{
			pCamera->Zoom(rInputOutput.MouseWheel);
		}
	}
	pCamera->mCameraInput.iScrollDelta = iScrollDelta;
	engine::gpGraphics->Refresh();
}

void RenderLightingTweakExtensions([[maybe_unused]] engine::TweaksScreen& rScreen, [[maybe_unused]] int64_t iSection, [[maybe_unused]] int64_t iFirstSubtab)
{
}

void RenderSmokeTweakExtensions([[maybe_unused]] engine::TweaksScreen& rScreen, [[maybe_unused]] int64_t iSection, [[maybe_unused]] int64_t iFirstSubtab)
{
}

void RenderWindTweakExtensions([[maybe_unused]] engine::TweaksScreen& rScreen, [[maybe_unused]] int64_t iSection, [[maybe_unused]] int64_t iFirstSubtab)
{
}

void RenderSoundTweakExtensions([[maybe_unused]] engine::TweaksScreen& rScreen)
{
}

} // namespace game

namespace engine
{

void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, [[maybe_unused]] const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, std::chrono::duration<float> currentTime)
{
	engine::gpBufferManager->ResetSkinningAllocations(iCommandBuffer);
	engine::RenderLightingMain(iCommandBuffer);
	if (game::gpModelViewerApplication->WorldVisible())
	{
		engine::RenderWaterMain(iCommandBuffer, currentTime);
	}
	else
	{
		engine::gpPipelineManager->mpPipelines[kPipelineWater].WriteIndirectBuffer(iCommandBuffer, 0);
	}
	engine::RenderLightingSpreadIndirect(iCommandBuffer);
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(engine::gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ViewProjection[0]), XMMatrixTranspose(engine::gpCamera->mMatView * engine::gpCamera->mMatProjection));
	XMStoreFloat4(&rMainLayout.f4EyePosition, engine::gpCamera->mVecEyePosition);
	XMStoreFloat4(&rMainLayout.f4WaterEyePosition, engine::gpCamera->mVecWaterEyePosition);
	XMStoreFloat4(&rMainLayout.f4ToEyeNormal, XMVectorSetW(engine::gpCamera->mVecToEyeNormal, engine::gpCamera->mbOrthographicProjection ? 1.0f : 0.0f));
	game::Camera* pCamera = static_cast<game::Camera*>(engine::gpCamera);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ModelViewProjection[0]), XMMatrixTranspose(pCamera->mMatModelView * pCamera->mMatModelPerspective));
	XMStoreFloat4(&rMainLayout.f4ModelEyePosition, pCamera->mVecModelEyePosition);
	rMainLayout.iFrameNumber = static_cast<int32_t>(rRenderInterpolates.at(cameraCoordinate).iFrame);
	rMainLayout.iRenderNumber = static_cast<int32_t>(engine::gpGraphics->miFrameCounter);
	if (engine::gpGraphics->mFlags & engine::GraphicsFlags::kDrawModelsOverWorld)
	{
		rMainLayout.fPhysicallyBasedRenderingLighting = 0.0f;
		rMainLayout.fPhysicallyBasedRenderingLightingSpecular = 0.0f;
		rMainLayout.fPhysicallyBasedRenderingSmoke = 0.0f;
		rMainLayout.fPhysicallyBasedRenderingShadowFloor = 1.0f;
	}
	game::gpModelViewerApplication->RenderModel(iCommandBuffer);
}

// The viewer panel keeps its state in ModelViewerApplication, so creating and destroying the UI screens has nothing to do.
void CreateUiScreens()
{
}

void RenderUiScreens()
{
	game::gpModelViewerApplication->RenderUi();
}

void DestroyUiScreens()
{
}

} // namespace engine
