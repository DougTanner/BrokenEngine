#include "HudScreen.h"

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Fleet.h"
#include "Game.h"

namespace game
{

constexpr float kfHudEdgeMarginFraction = 0.05f;
constexpr float kfHudPanelTopFraction = 0.2f;
constexpr float kfHudPanelHeightFraction = 0.6f;
constexpr std::chrono::seconds kForceOpenGracePeriod = 2s;

// Controls that send a simulation-affecting request are disabled while the server plays a replay
static bool IsServerReplaying()
{
	return engine::gpClient != nullptr && (engine::gpClient->mStateFlags & engine::Client::ClientStateFlags::kServerReplaying);
}

void HudScreen::Render()
{
	// Ensure tick prefix for the auto-unhide log — Render() runs outside ClientUpdate's LogTickScope.
	std::optional<common::LogTickScope> optionalTickScope;
	if (common::gpThreadLocal->miLogTickCounter < 0)
	{
		optionalTickScope.emplace(gpGame->miTickCounter);
	}

	if (gpGame->meUiState != engine::UiState::kNone)
	{
		return;
	}

	// Force-open the fleet panel when the focused fleet has no presence in any subscribed cell.
	// Iterating all subscribed cells (not just mClientGridCoordinate) tolerates cell-boundary crossings,
	// where the owned entity's snapshot has migrated to a neighbor before mClientGridCoordinate catches up.
	bool bWantsForceOpen = false;
	const char* pcWantReason = "fleet member present";
	int64_t iSubscribedCellCount = 0;

	if (!(gpGame->ClientOwnedEntityIdentifier().iValue != 0))
	{
		bWantsForceOpen = true;
		pcWantReason = "ClientOwnedEntityId invalid";
	}
	else if (const Fleet* pFleet = gpGame->mFleetSelection.FocusedFleet(); pFleet == nullptr)
	{
		bWantsForceOpen = true;
		pcWantReason = "no focused fleet";
	}
	else
	{
		bool bFoundAny = false;
		for (const auto& [rCoordinate, rCell] : gpGame->mCells)
		{
			if (rCell.iSnapshotCount == 0)
			{
				continue;
			}
			++iSubscribedCellCount;
			ForEachShipType(gpGame->RenderFrame(rCoordinate), [&]([[maybe_unused]] ShipType eType, [[maybe_unused]] const auto& rInterpolate, const auto& rPostRender)
			{
				for (int64_t i = 0; i < rPostRender.iCount && !bFoundAny; ++i)
				{
					engine::GlobalId globalId = rPostRender.pGlobalIds[i];
					bFoundAny = std::ranges::any_of(pFleet->members, [globalId](const FleetMember& rMember)
					{
						return rMember.globalId == globalId;
					});
				}
			});
			if (bFoundAny)
			{
				break;
			}
		}
		if (!bFoundAny)
		{
			bWantsForceOpen = true;
			pcWantReason = (iSubscribedCellCount == 0)
				? "no subscribed snapshots"
				: "no fleet members in any subscribed cell";
		}
	}

	// Grace period: only force-open once the want-state has been sustained. Absorbs the brief gap during cell-boundary
	// hand-offs when the owned-entity snapshot is momentarily absent from every subscribed cell, plus ClientOwnedEntityIdentifier blips.
	ImGuiIO& rInputOutput = ImGui::GetIO();
	if (bWantsForceOpen)
	{
		mTimeWantingForceOpen += std::chrono::duration<float>(rInputOutput.DeltaTime);
	}
	else
	{
		mTimeWantingForceOpen = 0s;
	}
	bool bForceOpen = (mTimeWantingForceOpen >= kForceOpenGracePeriod);

	// Durable log on rising edge of the genuine auto-un-hide trigger — fires once per recovery event.
	// kWarning clears both the compile floor (keLogLevelDefault, kDebug) and the runtime default threshold (kInfo).
	if (bForceOpen && !mbPreviousForceOpen)
	{
		LOG(kDefault, kWarning, "HUD auto-unhide reason: {} coord: ({},{}) cells: {}", pcWantReason, gpGame->mClientGridCoordinate.iQ, gpGame->mClientGridCoordinate.iR, iSubscribedCellCount);
	}
	mbPreviousForceOpen = bForceOpen;

	// Right panel content gate: the focused member id, because the render frame trails a newly focused member by
	// several ticks.
	bool bRightHasContent = gpGame->HasFocusedClientEntity();

	// With focused owned-entity content, mouse proximity to either anchor opens both panels together.
	ImVec2 vLeftAnchor(rInputOutput.DisplaySize.x * kfHudEdgeMarginFraction, rInputOutput.DisplaySize.y * kfHudPanelTopFraction);
	ImVec2 vRightAnchor(rInputOutput.DisplaySize.x * (1.0f - kfHudEdgeMarginFraction), rInputOutput.DisplaySize.y * kfHudPanelTopFraction);
	float fMouseLeft = engine::ComputeMouseOpennessTarget(mFleetSlide, vLeftAnchor, -1.0f);
	float fMouseRight = engine::ComputeMouseOpennessTarget(mFocusedOwnedEntitySlide, vRightAnchor, 1.0f);
	float fMouseTarget = std::max(fMouseLeft, fMouseRight);

	// Final shared targets. When right has content: both panels see max(force, mouse) — strict sync.
	// When right has no content: left can still auto-un-hide (force only), right stays hidden.
	float fForceTarget = bForceOpen ? 1.0f : 0.0f;
	float fLeftTarget = bRightHasContent ? std::max(fForceTarget, fMouseTarget) : fForceTarget;
	float fRightTarget = bRightHasContent ? std::max(fForceTarget, fMouseTarget) : 0.0f;

	RenderFleetPanel(fLeftTarget);
	RenderFocusedOwnedEntityPanel(fRightTarget);
}

float HudScreen::PanelWidth()
{
	// Fixed row templates measured under the pushed menu font (caller pushes ScopedMenuFont first). Fixed
	// templates — not live content — keep the width stable as fleet members churn; measuring under the font auto-tracks
	// gUiFontScale/UiScale() with no magnifying multiplier.
	const ImGuiStyle& rStyle = ImGui::GetStyle();

	// Member-row template: content-spanning Selectable rows, with the longest ship type name.
	float fMemberRowWidth = ImGui::CalcTextSize("Battleship 88 (-888,-888) #8888888888").x;

	// Nav-row template: [<] 88/88 [>] [+] [-]. Padding-dominated — at low gUiFontScale text shrinks but the per-button
	// FramePadding and per-joint ItemSpacing don't, so measure them explicitly: 4 buttons × 2 edges (8× FramePadding.x),
	// 4 SameLine joints (4× ItemSpacing.x). Otherwise the row can exceed the text-only width and clip trailing buttons.
	float fNavigationTextWidth = ImGui::CalcTextSize("[<]88/88[>][+][-]").x;
	float fNavigationRowWidth = fNavigationTextWidth + 8.0f * rStyle.FramePadding.x + 4.0f * rStyle.ItemSpacing.x;

	// ScrollbarSize added unconditionally (not gated on list length) so the width stays frame-to-frame stable when a
	// vertical scrollbar appears on long fleet lists — otherwise it would clip the exact-fit member rows.
	return std::max(fMemberRowWidth, fNavigationRowWidth) + rStyle.WindowPadding.x * 2.0f + rStyle.ScrollbarSize;
}

void HudScreen::RenderFleetPanel(float fTarget)
{
	ImGuiIO& rInputOutput = ImGui::GetIO();
	engine::ScopedMenuScale menuScale;

	ImVec2 vAnchor(rInputOutput.DisplaySize.x * kfHudEdgeMarginFraction, rInputOutput.DisplaySize.y * kfHudPanelTopFraction);
	float fEdgeX = engine::UpdateSlideAndGetEdgeX(mFleetSlide, vAnchor, -1.0f, fTarget);
	int64_t iWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	engine::ScopedMenuFont menuFont;
	// Fixed size shared with the right panel: the fleet list fills from the top and scrolls when long
	ImGui::SetNextWindowSize(ImVec2(PanelWidth(), rInputOutput.DisplaySize.y * kfHudPanelHeightFraction), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(fEdgeX, vAnchor.y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::Begin("FleetPanel", nullptr, static_cast<ImGuiWindowFlags>(iWindowFlags));
	mFleetSlide.vLastSize = ImGui::GetWindowSize();
	engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRectangle occlusion
	ImVec2 vPanelPosition = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	engine::DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPosition, ImVec2(vPanelPosition.x + vPanelSize.x, vPanelPosition.y + vPanelSize.y));

	int64_t iFleetCount = std::ssize(gpGame->mFleetSelection.mClientFleets);

	// Update fleet toggle: clears pending when fleet count changes
	gpGame->mFleetSelection.mCreateFleetToggle.Update(iFleetCount);

	ImGui::BeginDisabled(gpGame->mFleetSelection.miFocusedFleetIndex <= 0);
	if (ImGui::Button("[<]"))
	{
		gpGame->mFleetSelection.FocusPreviousFleet();
		gpClientSession->mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kFocusPreviousFleet));
		LOG(kDefault, kVerbose, "HUD FocusPrevFleet NewIndex: {} FleetCount: {}", gpGame->mFleetSelection.miFocusedFleetIndex, iFleetCount);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (iFleetCount > 0)
	{
		ImGui::Text("%lld/%lld", gpGame->mFleetSelection.miFocusedFleetIndex + 1, iFleetCount);
	}
	else
	{
		ImGui::Text("0/0");
	}
	ImGui::SameLine();

	ImGui::BeginDisabled(gpGame->mFleetSelection.miFocusedFleetIndex >= std::ssize(gpGame->mFleetSelection.mClientFleets) - 1);
	if (ImGui::Button("[>]"))
	{
		gpGame->mFleetSelection.FocusNextFleet();
		gpClientSession->mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kFocusNextFleet));
		LOG(kDefault, kVerbose, "HUD FocusNextFleet NewIndex: {} FleetCount: {}", gpGame->mFleetSelection.miFocusedFleetIndex, iFleetCount);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled((gpGame->mFleetSelection.mCreateFleetToggle.mFlags & engine::NetworkUiControlFlags::kPending) || iFleetCount >= kiMaximumFleetsPerClient || IsServerReplaying());
	if (ImGui::Button("[+]##Fleet"))
	{
		gpGame->mFleetSelection.mCreateFleetToggle.mFlags.Set(engine::NetworkUiControlFlags::kPending);
		gpClientSession->SendCreateFleetRequest();
		LOG(kDefault, kVerbose, "HUD CreateFleetRequest FleetCount: {}", iFleetCount);
	}
	ImGui::EndDisabled();

	const Fleet* pFleet = gpGame->mFleetSelection.FocusedFleet();
	if (pFleet != nullptr)
	{
		ImGui::Separator();

		gpGame->mFleetSelection.mSpawnIntoFleetToggle.Update(std::ssize(pFleet->members));

		// Above the member list, so a spawn's new row never moves these buttons
		struct SpawnButton
		{
			const char* pcLabel = nullptr;
			ShipType eType {};
		};
		static constexpr SpawnButton kpSpawnButtons[] {{.pcLabel = "[+F]##Fighter", .eType = ShipType::kFighter}, {.pcLabel = "[+O]##Omni", .eType = ShipType::kOmni}};
		ImGui::BeginDisabled((gpGame->mFleetSelection.mSpawnIntoFleetToggle.mFlags & engine::NetworkUiControlFlags::kPending) || std::ssize(pFleet->members) >= kiMaximumFleetMembers || IsServerReplaying());
		for (const auto& [i, rButton] : std::views::enumerate(kpSpawnButtons))
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			if (ImGui::Button(rButton.pcLabel))
			{
				gpGame->mFleetSelection.mSpawnIntoFleetToggle.mFlags.Set(engine::NetworkUiControlFlags::kPending);
				gpClientSession->SendSpawnIntoFleetRequest(pFleet->guid, rButton.eType);
				LOG(kDefault, kVerbose, "HUD SpawnIntoFleet Fleet: ({},{}) Type: {}", pFleet->guid.uiHigh, pFleet->guid.uiLow, ShipTypeName(rButton.eType));
			}
		}
		ImGui::EndDisabled();

		for (const auto& [i, rMember] : std::views::enumerate(pFleet->members))
		{
			bool bSelected = (rMember.globalId.iValue != 0) && rMember.globalId == gpGame->mFleetSelection.mFocusedMemberGlobalId;

			engine::GridCoord memberCoordinate {};
			for (int64_t j = 0; j < std::ssize(gpGame->mClientOwnedEntityIdentifiers); ++j)
			{
				if (gpGame->mClientOwnedEntityIdentifiers.at(j) == rMember.globalId)
				{
					memberCoordinate = gpGame->mClientOwnedEntityCoordinates.at(j);
					break;
				}
			}

			ImGui::PushID(static_cast<int>(i));
			if (!(rMember.flags & FleetMemberFlags::kIsDead))
			{
				char pcLabel[64];
				std::snprintf(pcLabel, sizeof(pcLabel), "%s %lld (%d,%d) #%lld", ShipTypeName(rMember.eType), i + 1, memberCoordinate.iQ, memberCoordinate.iR, rMember.globalId.iValue);
				if (ImGui::Selectable(pcLabel, bSelected))
				{
					gpGame->mFleetSelection.SelectOwnedEntityInFleet(rMember.globalId);
					gpClientSession->mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kSelectOwnedEntity));
				}
			}
			else
			{
				char pcLabel[64];
				std::snprintf(pcLabel, sizeof(pcLabel), "%s %lld [DEAD] #%lld", ShipTypeName(rMember.eType), i + 1, rMember.globalId.iValue);
				ImGui::TextDisabled("%s", pcLabel);
			}
			ImGui::PopID();
		}
	}

	ImGui::End();
}

void HudScreen::RenderFocusedOwnedEntityPanel(float fTarget)
{
	ImGuiIO& rInputOutput = ImGui::GetIO();
	engine::ScopedMenuScale menuScale;

	ImVec2 vAnchor(rInputOutput.DisplaySize.x * (1.0f - kfHudEdgeMarginFraction), rInputOutput.DisplaySize.y * kfHudPanelTopFraction);
	float fEdgeX = engine::UpdateSlideAndGetEdgeX(mFocusedOwnedEntitySlide, vAnchor, 1.0f, fTarget);
	int64_t iWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	engine::ScopedMenuFont menuFont;
	ImGui::SetNextWindowSize(ImVec2(PanelWidth(), rInputOutput.DisplaySize.y * kfHudPanelHeightFraction), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(fEdgeX, vAnchor.y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
	ImGui::Begin("FocusedOwnedEntityPanel", nullptr, static_cast<ImGuiWindowFlags>(iWindowFlags));
	mFocusedOwnedEntitySlide.vLastSize = ImGui::GetWindowSize();
	engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRectangle occlusion
	ImVec2 vPanelPosition = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	engine::DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPosition, ImVec2(vPanelPosition.x + vPanelSize.x, vPanelPosition.y + vPanelSize.y));

	const Fleet* pFleet = gpGame->mFleetSelection.FocusedFleet();
	if (pFleet != nullptr && gpGame->ClientOwnedEntityIdentifier() == pFleet->flagshipGlobalId)
	{
		gpGame->mFleetSelection.mNavigationDelayControl.Update(NavigationDelayKey {.fleetGuid = pFleet->guid, .fNavigationDelay = pFleet->navigationDelaySeconds.count()});
		ImGui::BeginDisabled((gpGame->mFleetSelection.mNavigationDelayControl.mFlags & engine::NetworkUiControlFlags::kPending) || IsServerReplaying());
		static float sfNavigationDelayEditValue = 0.0f;
		static bool sbNavigationDelaySliderWasActive = false;
		// Reload only while the slider is inactive, so it owns the value for the whole interaction and a release
		// sends exactly the value the user let go on
		if (!sbNavigationDelaySliderWasActive)
		{
			sfNavigationDelayEditValue = pFleet->navigationDelaySeconds.count();
		}
		// A negative width sizes the slider to the content's right edge minus the trailing label's width, so the label
		// stays inside the clip edge
		ImGui::SetNextItemWidth(-(ImGui::CalcTextSize("Nav Delay").x + ImGui::GetStyle().ItemInnerSpacing.x));
		// Ctrl+Click typed input can be non-finite or out of range and the server rejects such a delay, so the
		// release below reverts it instead of clamping it; checking only on release leaves the text box alone while typing
		ImGui::SliderFloat("Nav Delay", &sfNavigationDelayEditValue, 0.0f, 60.0f, "%.3f");
		sbNavigationDelaySliderWasActive = ImGui::IsItemActive();
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			if (!IsNavigationDelayInRange(sfNavigationDelayEditValue))
			{
				sfNavigationDelayEditValue = pFleet->navigationDelaySeconds.count();
			}
			// An unchanged synchronized delay leaves this fleet's request pending.
			if (sfNavigationDelayEditValue != pFleet->navigationDelaySeconds.count())
			{
				gpGame->mFleetSelection.mNavigationDelayControl.mFlags.Set(engine::NetworkUiControlFlags::kPending);
				gpClientSession->SendFleetNavigationDelayRequest(pFleet->guid, sfNavigationDelayEditValue);
			}
		}
		ImGui::EndDisabled();
	}

	ImGui::End();
}

} // namespace game

#endif // BT_CLIENT
