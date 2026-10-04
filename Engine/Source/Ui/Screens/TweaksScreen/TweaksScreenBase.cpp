#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "TweaksSliderMap.h"

#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace engine
{

// Order-sensitive fold of the registered stable keys: the per-key CRCs are hashed as one byte run, so a
// reordering, a rename, or a count change all produce a different value.
static common::crc_t ComputeLayoutCrc()
{
	common::crc_t crcKeys[kiMaxTweakSections] {};
	for (int64_t i = 0; i < TweaksScreenBase::msiSectionCount; ++i)
	{
		crcKeys[i] = common::Crc(TweaksScreenBase::msSectionDescriptions[i].stableKey);
	}
	return common::Crc(std::span<const common::crc_t>(crcKeys, static_cast<size_t>(TweaksScreenBase::msiSectionCount)));
}

constexpr float kfUiScale = 1.5f;

void TweaksScreenBase::RegisterSection(int64_t& riSection, const TweakSectionDescription& rDescription)
{
	ASSERT(riSection == kiInvalidTweakSection);
	ASSERT(msiSectionCount < kiMaxTweakSections);
	riSection = msiSectionCount;
	msSectionDescriptions[msiSectionCount] = rDescription;
	++msiSectionCount;
}

common::Flags<TweakSectionFlags> TweaksScreenBase::AllSectionFlags()
{
	return static_cast<TweakSectionFlags>((1u << msiSectionCount) - 1u);
}

void RegisterEngineTweakSections()
{
	TweaksScreenBase::RegisterSection(giTweakSectionPbr, {.displayName = "Pbr", .stableKey = "Pbr", .pRender = RenderPhysicallyBasedRenderingSection});
	TweaksScreenBase::RegisterSection(giTweakSectionTerrain, {.displayName = "Terrain", .stableKey = "Terrain", .pRender = RenderTerrainSection});
	TweaksScreenBase::RegisterSection(giTweakSectionWater, {.displayName = "Water", .stableKey = "Water", .pRender = RenderWaterSection});
	TweaksScreenBase::RegisterSection(giTweakSectionLighting, {.displayName = "Lighting", .stableKey = "Lighting", .pRender = RenderLightingSection});
	TweaksScreenBase::RegisterSection(giTweakSectionShadow, {.displayName = "Shadow", .stableKey = "Shadow", .pRender = RenderShadowSection});
	TweaksScreenBase::RegisterSection(giTweakSectionSunMoon, {.displayName = "Sun/Moon", .stableKey = "SunMoon", .pRender = RenderSunMoonSection});
	TweaksScreenBase::RegisterSection(giTweakSectionMiscellaneous, {.displayName = "Misc", .stableKey = "Misc", .pRender = RenderMiscellaneousSection});
	TweaksScreenBase::RegisterSection(giTweakSectionSound, {.displayName = "Sound", .stableKey = "Sound", .pRender = RenderSoundSection});
	TweaksScreenBase::RegisterSection(giTweakSectionSmoke, {.displayName = "Smoke", .stableKey = "Smoke", .pRender = RenderSmokeSection});
	TweaksScreenBase::RegisterSection(giTweakSectionWind, {.displayName = "Wind", .stableKey = "Wind", .pRender = RenderWindSection});
}

void RenderTweakExtensionSubtabs(TweaksScreenBase& rScreen, int64_t iSection, int8_t iFirstSubtab)
{
	for (int8_t i = iFirstSubtab; const TweakExtensionHook& rHook : game::kTweakExtensionHooks)
	{
		if (*rHook.piSection != iSection)
		{
			continue;
		}

		if (rHook.pcLabel == nullptr)
		{
			continue;
		}

		if (rScreen.BeginSubtab(rHook.pcLabel, iSection, i))
		{
			rHook.pRender(rScreen);
			ImGui::EndTabItem();
		}
		++i;
	}
}

void RenderTweakExtensionInline(TweaksScreenBase& rScreen, int64_t iSection)
{
	for (const TweakExtensionHook& rHook : game::kTweakExtensionHooks)
	{
		if (*rHook.piSection == iSection && rHook.pcLabel == nullptr)
		{
			rHook.pRender(rScreen);
		}
	}
}

TweaksScreenBase::TweaksScreenBase()
{
	// Every section must already be registered; msiSectionCount below is the final count. Registration runs
	// from Main.cpp rather than here because device loss recreates Graphics, and so this object, in place -
	// registering here would double-register.

	// Zero Y defers vertical positioning to mfToggleBarBottom; X offsets stagger the windows.
	static constexpr float kfStartX = 10.0f;
	static constexpr float kfOffsetX = 30.0f;
	float fUiScale = UiScale();

	for (int64_t i = 0; i < msiSectionCount; ++i)
	{
		mWindowPositions[i] = ImVec2((kfStartX + static_cast<float>(i) * kfOffsetX) * fUiScale, 0.0f);
	}
}

void TweaksScreenBase::RenderWaveCountRadioButtons(Wrapper& rCountWrapper)
{
	if (!mActiveSlider.empty())
	{
		return;
	}

	ImGui::Text("Wave Count");
	ImGui::SameLine();
	int64_t iCurrent = rCountWrapper.Get<int64_t>();
	for (const std::pair<const char*, int64_t>& rPair : std::initializer_list<std::pair<const char*, int64_t>> {{"15", 15}, {"31", 31}, {"63", 63}, {"127", 127}, {"255", 255}})
	{
		if (ImGui::RadioButton(rPair.first, iCurrent == rPair.second))
		{
			rCountWrapper.Set(rPair.second);
		}
		ImGui::SameLine();
	}
	ImGui::NewLine();
}

void TweaksScreenBase::ChevronIndexSelector(std::string_view label, Wrapper& rWrapper, std::span<const std::string_view> names)
{
	// Match WrapperSlider's alpha-fade-while-dragging behavior so this widget keeps its layout slot when another slider is active.
	bool bAnotherSliderActive = !mActiveSlider.empty();
	if (bAnotherSliderActive)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}

	int64_t iCount = std::ssize(names);
	int64_t iIndex = std::clamp(rWrapper.GetIndex(), static_cast<int64_t>(0), iCount - 1);

	// Render << and >> adjacent first, then the label — keeps button positions fixed when the displayed name changes width.
	char pcIdentifier[128];
	std::snprintf(pcIdentifier, sizeof(pcIdentifier), "<<##%.*s_prev", static_cast<int>(label.size()), label.data());
	if (ImGui::Button(pcIdentifier) && !bAnotherSliderActive)
	{
		rWrapper.SetIndex((iIndex + iCount - 1) % iCount);
	}
	ImGui::SameLine();
	std::snprintf(pcIdentifier, sizeof(pcIdentifier), ">>##%.*s_next", static_cast<int>(label.size()), label.data());
	if (ImGui::Button(pcIdentifier) && !bAnotherSliderActive)
	{
		rWrapper.SetIndex((iIndex + 1) % iCount);
	}
	ImGui::SameLine();
	std::string_view name = names[iIndex];
	ImGui::Text("%.*s [%lld/%lld] %.*s", static_cast<int>(label.size()), label.data(), iIndex + 1, iCount, static_cast<int>(name.size()), name.data());

	if (bAnotherSliderActive)
	{
		ImGui::PopStyleVar();
	}
}

bool TweaksScreenBase::BeginSubtab(const char* pcLabel, int64_t iSection, int8_t iTab)
{
	bool bApplySavedSubtab = (mApplySubtab & SectionFlag(iSection)) && miActiveSubtab[iSection] == iTab;
	if (!ImGui::BeginTabItem(pcLabel, nullptr, bApplySavedSubtab ? ImGuiTabItemFlags_SetSelected : 0))
	{
		return false;
	}

	if (bApplySavedSubtab)
	{
		mApplySubtab.Set(SectionFlag(iSection), false);
	}
	if (!(mApplySubtab & SectionFlag(iSection)))
	{
		miActiveSubtab[iSection] = iTab;
	}
	return true;
}

void TweaksScreenBase::WrapperSlider(std::string_view label, int64_t iSection, float fWidthMultiplier, std::string_view mapKey)
{
	if (mapKey.empty())
	{
		mapKey = label;
	}

	if constexpr (kbDebugInput)
	{
		if (mbAuditMode)
		{
			// Audit hash-set insertions allocate; exclude them from allocation tracking.
			ScopedSuppressAllocationTracking suppress;
			std::unordered_map<std::string_view, Wrapper*>& rSliderMap = TweaksSliderMap::Get();
			if (rSliderMap.contains(mapKey))
			{
				mAuditTouched.insert(mapKey);
			}
			else
			{
				mAuditMissed.insert(mapKey);
			}
			return;
		}
	}

	std::unordered_map<std::string_view, Wrapper*>& rSliderMap = TweaksSliderMap::Get();
	auto it = rSliderMap.find(mapKey);
	if (it == rSliderMap.end())
	{
		return;
	}

	// Render non-active sliders with alpha=0 to preserve layout
	bool bIsActiveSlider = (mActiveSlider.empty() || mapKey == mActiveSlider);
	if (!bIsActiveSlider)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}

	if (iSection >= 0)
	{
		ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * fWidthMultiplier);
	}

	Wrapper* pWrapper = it->second;
	float fValue = pWrapper->mfCurrent;

	// When mapKey differs from label, append ##mapKey for unique ImGui ID
	char pcImGuiLabel[128] {};
	if (mapKey != label)
	{
		std::snprintf(pcImGuiLabel, sizeof(pcImGuiLabel), "%.*s##%.*s", static_cast<int>(label.size()), label.data(), static_cast<int>(mapKey.size()), mapKey.data());
	}
	else
	{
		std::snprintf(pcImGuiLabel, sizeof(pcImGuiLabel), "%.*s", static_cast<int>(label.size()), label.data());
	}

	if (ImGui::SliderFloat(pcImGuiLabel, &fValue, pWrapper->mfMin, pWrapper->mfMax, "%.6f"))
	{
		pWrapper->Set(fValue);
	}
	if (ImGui::IsItemActive())
	{
		mActiveSlider = mapKey;
		miActiveSliderSection = iSection;
	}

	if (!bIsActiveSlider)
	{
		ImGui::PopStyleVar();
	}
}

void TweaksScreenBase::Render()
{
	if constexpr (kbDebugInput)
	{
		ImGuiIO& rInputOutput = ImGui::GetIO();

		if (!rInputOutput.MouseDown[0])
		{
			mActiveSlider = {};
		}

		ImGuiStyle& rStyle = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(rStyle.FramePadding.x * kfUiScale, rStyle.FramePadding.y * kfUiScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(rStyle.ItemSpacing.x * kfUiScale, rStyle.ItemSpacing.y * kfUiScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(rStyle.ItemInnerSpacing.x * kfUiScale, rStyle.ItemInnerSpacing.y * kfUiScale));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(rStyle.WindowPadding.x * kfUiScale, rStyle.WindowPadding.y * kfUiScale));

		if (miAuditFrame >= 0)
		{
			RunSliderAuditFrame();
		}

		RenderToggleBar();

		// Render visible section windows (only the one with active slider when dragging)
		for (int64_t i = 0; i < msiSectionCount; ++i)
		{
			if (!mActiveSlider.empty())
			{
				if (i == miActiveSliderSection)
				{
					RenderSectionWindow(i);
				}
			}
			else if (mSectionVisible & SectionFlag(i))
			{
				RenderSectionWindow(i);
			}
		}

		ImGui::PopStyleVar(4);
	}
}

void TweaksScreenBase::RenderToggleBar()
{
	// Capture state at start (mActiveSlider can change during WrapperSlider)
	bool bSliderActive = !mActiveSlider.empty();

	ImGuiIO& rInputOutput = ImGui::GetIO();

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	ImGui::SetNextWindowPos(ImVec2(0.0f, 10.0f * UiScale()), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(rInputOutput.DisplaySize.x, 0.0f));
	ImGui::Begin("Tweaks", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kfUiScale);

	float fContentWidth = rInputOutput.DisplaySize.x - ImGui::GetStyle().WindowPadding.x * 2.0f;

	float fButtonWidth = (fContentWidth - ImGui::GetStyle().ItemSpacing.x * (msiSectionCount - 1)) / msiSectionCount;

	// Render toggle buttons with alpha=0 when slider is active to preserve layout
	if (bSliderActive)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
	for (int64_t i = 0; i < msiSectionCount; ++i)
	{
		if (i > 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::Selectable(msSectionDescriptions[i].displayName.data(), mSectionVisible & SectionFlag(i), 0, ImVec2(fButtonWidth, 0.0f)))
		{
			mSectionVisible.Toggle(SectionFlag(i));
		}
	}
	ImGui::PopStyleVar();

	if (bSliderActive)
	{
		ImGui::PopStyleVar();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y * 2.0f));
	ImGui::SetNextItemWidth(fContentWidth);
	float fSunAngle = gSunAngleOverride.mfCurrent;
	if (ImGui::SliderFloat("##Sun Angle", &fSunAngle, gSunAngleOverride.mfMin, gSunAngleOverride.mfMax, "%.6f"))
	{
		gSunAngleOverride.Set(fSunAngle);
	}
	if (ImGui::IsItemActive())
	{
		mActiveSlider = "##Sun Angle";
		miActiveSliderSection = -1;
	}
	ImGui::PopStyleVar();

	mfToggleBarBottom = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;

	ImGui::PopFont();
	ImGui::End();

	ImGui::PopStyleColor(2);
}

void TweaksScreenBase::WrapperSeparatorText(std::string_view label)
{
	if (!mActiveSlider.empty())
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}
	ImGui::SeparatorText(label.data());
	if (!mActiveSlider.empty())
	{
		ImGui::PopStyleVar();
	}
}

void TweaksScreenBase::SaveState(TweakSectionState& rState) const
{
	// Clear first so unregistered slots and uiPadding are written as zeros: two saves of unchanged settings must be byte-identical.
	rState = {};

	rState.visible = mSectionVisible;
	rState.collapsed = mSectionCollapsed;
	for (int64_t i = 0; i < kiMaxTweakSections; ++i)
	{
		rState.fWindowPositionX[i] = mWindowPositions[i].x;
		rState.fWindowPositionY[i] = mWindowPositions[i].y;
	}
	std::memcpy(rState.iActiveSubtab, miActiveSubtab, sizeof(miActiveSubtab));
	rState.crcLayout = ComputeLayoutCrc();
}

void TweaksScreenBase::LoadState(const TweakSectionState& rState)
{
	if (rState.crcLayout != ComputeLayoutCrc())
	{
		// The registered section set changed since the save, so its dense indices no longer name the same
		// sections. Discard the layout and keep the constructor defaults.
		LOG(kDefault, kWarning, "LoadTweaks section layout changed, using default layout");
		return;
	}

	mSectionVisible = rState.visible;
	mSectionCollapsed = rState.collapsed;
	for (int64_t i = 0; i < kiMaxTweakSections; ++i)
	{
		mWindowPositions[i] = ImVec2(rState.fWindowPositionX[i], rState.fWindowPositionY[i]);
	}
	std::memcpy(miActiveSubtab, rState.iActiveSubtab, sizeof(miActiveSubtab));
	mApplySubtab = AllSectionFlags();
}

void TweaksScreenBase::RenderSectionWindow(int64_t iSection)
{
	bool bHasActiveSlider = (!mActiveSlider.empty() && miActiveSliderSection == iSection);

	if (bHasActiveSlider)
	{
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	}

	static constexpr float kfStartX = 10.0f;
	ImVec2 f2InitialPosition = (mWindowPositions[iSection].y > 0.0f) ? mWindowPositions[iSection] : ImVec2 {kfStartX * UiScale(), mfToggleBarBottom};
	ImGui::SetNextWindowPos(f2InitialPosition, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(mSectionCollapsed & SectionFlag(iSection), ImGuiCond_FirstUseEver);
	bool bSectionVisible = (mSectionVisible & SectionFlag(iSection)); // ImGui::Begin writes the close-button [x] state back through this bool*
	ImGui::Begin(msSectionDescriptions[iSection].displayName.data(), bHasActiveSlider ? nullptr : &bSectionVisible, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kfUiScale);
	mWindowPositions[iSection] = ImGui::GetWindowPos();
	mSectionCollapsed.Set(SectionFlag(iSection), ImGui::IsWindowCollapsed());
	if (!bHasActiveSlider)
	{
		mSectionVisible.Set(SectionFlag(iSection), bSectionVisible);
	}

	msSectionDescriptions[iSection].pRender(*this);

	ImGui::PopFont();
	ImGui::End();

	if (bHasActiveSlider)
	{
		ImGui::PopStyleColor(4);
	}
}

void TweaksScreenBase::RunSliderAuditFrame()
{
	if constexpr (kbDebugInput)
	{
		// Lighting has 5 subtabs (Write/Combine/Read/Visible/Lighting); ImGui applies selection on the following tab-bar frame, so frames 0-4 queue tabs and frame 5 renders Lighting.
		static constexpr int64_t kiAuditFrameCount = 6;

		if (miAuditFrame == 0)
		{
			std::memcpy(miPreAuditSubtab, miActiveSubtab, sizeof(miActiveSubtab));
			ScopedSuppressAllocationTracking suppress;
			size_t uiSliderCount = TweaksSliderMap::Get().size();
			mAuditTouched.reserve(uiSliderCount);
			mAuditMissed.reserve(8); // typical drift is small; reserve nominal to avoid 1-element bucket churn
		}

		for (int64_t i = 0; i < msiSectionCount; ++i)
		{
			miActiveSubtab[i] = miAuditFrame;
			mApplySubtab.Set(SectionFlag(i));
		}

		mbAuditMode = true;

		// Synthetic offscreen window: BeginTabBar / WrapperSeparatorText / etc. need an active window, but we don't want anything visible or interactive.
		ImGui::SetNextWindowPos(ImVec2(-10'000.0f, -10'000.0f));
		ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f));
		static constexpr ImGuiWindowFlags kiAuditFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (ImGui::Begin("##slider-audit", nullptr, kiAuditFlags))
		{
			for (int64_t i = 0; i < msiSectionCount; ++i)
			{
				msSectionDescriptions[i].pRender(*this);
			}
		}
		ImGui::End();

		mbAuditMode = false;

		// Restore EVERY frame (not just on completion): the actual UI render later in the same Render() call must draw the user's saved tab, not the audit-cycled one.
		std::memcpy(miActiveSubtab, miPreAuditSubtab, sizeof(miActiveSubtab));
		mApplySubtab = AllSectionFlags();

		++miAuditFrame;
		if (miAuditFrame >= kiAuditFrameCount)
		{
			// Heap: TweaksSliderMap iteration touches its hash buckets; mirror the registration-side suppression.
			ScopedSuppressAllocationTracking suppress;
			for (const auto& [key, pWrapper] : TweaksSliderMap::Get())
			{
				if (!mAuditTouched.contains(key))
				{
					LOG(kDefault, kWarning, "TweaksSliderMap: orphan key '{}'", key);
				}
			}
			for (std::string_view missedKey : mAuditMissed)
			{
				LOG(kDefault, kWarning, "TweaksSliderMap: missed key '{}'", missedKey);
			}
			miAuditFrame = -1; // sentinel: audit complete
		}
	}
}

} // namespace engine

#endif // BT_CLIENT
