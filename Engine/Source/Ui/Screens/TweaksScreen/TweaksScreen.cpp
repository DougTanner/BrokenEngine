#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Agent/AgentUiRegistry.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

namespace engine
{

// Order-sensitive fold of the registered stable keys: the per-key CRCs are hashed as one byte run, so a
// reordering, a rename, or a count change all produce a different value.
static common::crc_t ComputeLayoutCrc()
{
	common::crc_t pCrcKeys[kiMaxTweakSections] {};
	for (int64_t i = 0; i < TweaksScreen::msiSectionCount; ++i)
	{
		pCrcKeys[i] = common::Crc(TweaksScreen::mspSectionDescriptions[i].stableKey);
	}
	return common::Crc(std::span<const common::crc_t>(pCrcKeys, static_cast<size_t>(TweaksScreen::msiSectionCount)));
}

constexpr float kfUiScale = 1.5f;

void TweaksScreen::RegisterSection(int64_t& riSection, const TweakSectionDescription& rDescription)
{
	ASSERT(riSection == kiInvalidTweakSection);
	ASSERT(msiSectionCount < kiMaxTweakSections);
	riSection = msiSectionCount;
	mspSectionDescriptions[msiSectionCount] = rDescription;
	++msiSectionCount;
}

common::Flags<TweakSectionFlags> TweaksScreen::AllSectionFlags()
{
	return static_cast<TweakSectionFlags>((1ui32 << msiSectionCount) - 1ui32);
}

void RegisterEngineTweakSections()
{
	TweaksScreen::RegisterSection(giTweakSectionPbr, {.displayName = "Pbr", .stableKey = "Pbr", .pRender = RenderPhysicallyBasedRenderingSection});
	TweaksScreen::RegisterSection(giTweakSectionTerrain, {.displayName = "Terrain", .stableKey = "Terrain", .pRender = RenderTerrainSection});
	TweaksScreen::RegisterSection(giTweakSectionWater, {.displayName = "Water", .stableKey = "Water", .pRender = RenderWaterSection});
	TweaksScreen::RegisterSection(giTweakSectionLighting, {.displayName = "Lighting", .stableKey = "Lighting", .pRender = RenderLightingSection});
	TweaksScreen::RegisterSection(giTweakSectionShadow, {.displayName = "Shadow", .stableKey = "Shadow", .pRender = RenderShadowSection});
	TweaksScreen::RegisterSection(giTweakSectionSunMoon, {.displayName = "Sun/Moon", .stableKey = "SunMoon", .pRender = RenderSunMoonSection});
	TweaksScreen::RegisterSection(giTweakSectionMiscellaneous, {.displayName = "Misc", .stableKey = "Misc", .pRender = RenderMiscellaneousSection});
	TweaksScreen::RegisterSection(giTweakSectionSound, {.displayName = "Sound", .stableKey = "Sound", .pRender = RenderSoundSection});
	TweaksScreen::RegisterSection(giTweakSectionSmoke, {.displayName = "Smoke", .stableKey = "Smoke", .pRender = RenderSmokeSection});
	TweaksScreen::RegisterSection(giTweakSectionWind, {.displayName = "Wind", .stableKey = "Wind", .pRender = RenderWindSection});
	TweaksScreen::RegisterSection(giTweakSectionHexShield, {.displayName = "Hex Shield", .stableKey = "HexShield", .pRender = RenderHexShieldSection});
}

TweaksScreen::TweaksScreen()
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
		mpWindowPositions[i] = ImVec2((kfStartX + static_cast<float>(i) * kfOffsetX) * fUiScale, 0.0f);
	}
}

void TweaksScreen::RenderWaveCountRadioButtons(Wrapper& rCountWrapper)
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

void TweaksScreen::ChevronIndexSelector(std::string_view label, Wrapper& rWrapper, std::span<const std::string_view> names)
{
	// Match WrapperSlider's alpha-fade-while-dragging behavior so this widget keeps its layout slot when another slider is active.
	bool bAnotherSliderActive = !mActiveSlider.empty();
	if (bAnotherSliderActive)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}

	int64_t iCount = std::ssize(names);
	int64_t iIndex = std::clamp(rWrapper.GetIndex(), 0i64, iCount - 1);

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

bool TweaksScreen::BeginSubtab(const char* pcLabel, int64_t iSection, int64_t iTab)
{
	bool bApplySavedSubtab = (mApplySubtab & SectionFlag(iSection)) && mpiActiveSubtab[iSection] == iTab;
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
		mpiActiveSubtab[iSection] = static_cast<int8_t>(iTab);
	}
	return true;
}

void TweaksScreen::WrapperSlider(std::string_view label, Wrapper& rWrapper, int64_t iSection, float fWidthMultiplier)
{
	// Render non-active sliders with alpha=0 to preserve layout
	bool bIsActiveSlider = (mActiveSlider.empty() || label == mActiveSlider);
	if (!bIsActiveSlider)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}

	if (iSection >= 0)
	{
		ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * fWidthMultiplier);
	}

	float fValue = rWrapper.mfCurrent;

	char pcImGuiLabel[128] {};
	std::snprintf(pcImGuiLabel, sizeof(pcImGuiLabel), "%.*s", static_cast<int>(label.size()), label.data());

	if (!rWrapper.mAllowed.empty())
	{
		int64_t iIndex = rWrapper.GetIndex();
		int64_t iMaxIndex = std::ssize(rWrapper.mAllowed) - 1i64;
		int iSliderIndex = static_cast<int>(iIndex);
		char pcValue[32] {};
		std::snprintf(pcValue, sizeof(pcValue), "%.6g", static_cast<double>(rWrapper.mAllowed.at(iIndex)));
		if (ImGui::SliderInt(pcImGuiLabel, &iSliderIndex, 0, static_cast<int>(iMaxIndex), pcValue, ImGuiSliderFlags_NoInput))
		{
			rWrapper.SetIndex(iSliderIndex);
		}
	}
	else if (ImGui::SliderFloat(pcImGuiLabel, &fValue, rWrapper.mfMin, rWrapper.mfMax, "%.6f"))
	{
		rWrapper.Set(fValue);
	}
	if (ImGui::IsItemActive())
	{
		mActiveSlider = label;
		miActiveSliderSection = iSection;
	}

	if (gpAgentUiRegistry != nullptr)
	{
		// Same formats as the slider's own text, applied to the post-change value.
		if (!rWrapper.mAllowed.empty())
		{
			gpAgentUiRegistry->RecordItemValue(ImGui::GetID(pcImGuiLabel), "%.6g", static_cast<double>(rWrapper.mAllowed.at(rWrapper.GetIndex())));
		}
		else
		{
			gpAgentUiRegistry->RecordItemValue(ImGui::GetID(pcImGuiLabel), "%.6f", static_cast<double>(rWrapper.mfCurrent));
		}
	}

	if (!bIsActiveSlider)
	{
		ImGui::PopStyleVar();
	}
}

void TweaksScreen::Render()
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

void TweaksScreen::RenderToggleBar()
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

	// Render toggle buttons, and the Sun Angle slider unless it is the one dragged, with alpha=0 when a slider is active to preserve layout
	if (bSliderActive)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}

	// The bar's window background is transparent, so each label needs a button frame behind it to stay readable.
	for (int64_t i = 0; i < msiSectionCount; ++i)
	{
		if (i > 0)
		{
			ImGui::SameLine();
		}
		bool bVisible = mSectionVisible & SectionFlag(i);
		if (bVisible)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		}
		if (ImGui::Button(mspSectionDescriptions[i].displayName.data(), ImVec2(fButtonWidth, 0.0f)))
		{
			mSectionVisible.Toggle(SectionFlag(i));
		}
		if (bVisible)
		{
			ImGui::PopStyleColor(3);
		}
	}

	if (bSliderActive)
	{
		ImGui::PopStyleVar();
	}

	static constexpr char kpcSunAngleLabel[] = "##Sun Angle";
	bool bHideSunAngle = bSliderActive && mActiveSlider != kpcSunAngleLabel;
	if (bHideSunAngle)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y * 2.0f));
	ImGui::SetNextItemWidth(fContentWidth);
	ImGui::SliderFloat(kpcSunAngleLabel, &gpCamera->mfSunAngle, 0.0f, XM_2PI, "%.6f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::IsItemActive())
	{
		mActiveSlider = kpcSunAngleLabel;
		miActiveSliderSection = -1;
	}
	ImGui::PopStyleVar();
	if (bHideSunAngle)
	{
		ImGui::PopStyleVar();
	}

	mfToggleBarBottom = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;

	ImGui::PopFont();
	ImGui::End();

	ImGui::PopStyleColor(2);
}

void TweaksScreen::WrapperSeparatorText(std::string_view label)
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

void TweaksScreen::SaveState(TweakSectionState& rState) const
{
	// Clear first so unregistered slots and puiPadding are written as zeros: two saves of unchanged settings must be byte-identical.
	rState = {};

	rState.visible = mSectionVisible;
	rState.collapsed = mSectionCollapsed;
	for (int64_t i = 0; i < kiMaxTweakSections; ++i)
	{
		rState.pfWindowPositionX[i] = mpWindowPositions[i].x;
		rState.pfWindowPositionY[i] = mpWindowPositions[i].y;
	}
	std::copy(std::begin(mpiActiveSubtab), std::end(mpiActiveSubtab), std::begin(rState.piActiveSubtab));
	rState.crcLayout = ComputeLayoutCrc();
}

void TweaksScreen::LoadState(const TweakSectionState& rState)
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
		mpWindowPositions[i] = ImVec2(rState.pfWindowPositionX[i], rState.pfWindowPositionY[i]);
	}
	std::copy(std::begin(rState.piActiveSubtab), std::end(rState.piActiveSubtab), std::begin(mpiActiveSubtab));
	mApplySubtab = AllSectionFlags();
}

void TweaksScreen::RenderSectionWindow(int64_t iSection)
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
	ImVec2 f2InitialPosition = (mpWindowPositions[iSection].y > 0.0f) ? mpWindowPositions[iSection] : ImVec2 {kfStartX * UiScale(), mfToggleBarBottom};
	ImGui::SetNextWindowPos(f2InitialPosition, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(mSectionCollapsed & SectionFlag(iSection), ImGuiCond_FirstUseEver);
	// Cap the height to the space below the toggle bar so a window dragged up against it still ends on screen.
	static constexpr float kfBottomPadding = 10.0f;
	ImGuiIO& rInputOutput = ImGui::GetIO();
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(rInputOutput.DisplaySize.x, rInputOutput.DisplaySize.y - mfToggleBarBottom - kfBottomPadding * UiScale()));
	bool bSectionVisible = (mSectionVisible & SectionFlag(iSection)); // ImGui::Begin writes the close-button [x] state back through this bool*
	ImGui::Begin(mspSectionDescriptions[iSection].displayName.data(), bHasActiveSlider ? nullptr : &bSectionVisible, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kfUiScale);
	mpWindowPositions[iSection] = ImGui::GetWindowPos();
	mSectionCollapsed.Set(SectionFlag(iSection), ImGui::IsWindowCollapsed());
	if (!bHasActiveSlider)
	{
		mSectionVisible.Set(SectionFlag(iSection), bSectionVisible);
	}

	mspSectionDescriptions[iSection].pRender(*this);

	ImGui::PopFont();
	ImGui::End();

	if (bHasActiveSlider)
	{
		ImGui::PopStyleColor(4);
	}
}

} // namespace engine

#endif // BT_CLIENT
