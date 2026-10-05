#include "HudScreen.h"

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"

#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Fleet.h"
#include "Game.h"

namespace game
{

constexpr float kfHudEdgeMarginFraction = 0.05f;
constexpr float kfHudPanelTopFraction = 0.125f;
constexpr float kfHudPanelMaximumHeightFraction = 0.75f;
constexpr std::chrono::seconds kForceOpenGracePeriod = 2s;

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

	// Force-open the fleet panel when the focused fleet has no presence in any subscribed frame.
	// Iterating all subscribed frames (not just mClientGridCoordinate) tolerates cell-boundary crossings,
	// where the player's snapshot has migrated to a neighbor before mClientGridCoordinate catches up.
	bool bWantsForceOpen = false;
	const char* pcWantReason = "fleet member present";
	int64_t iSubscribedFrameCount = 0;

	if (!(gpGame->ClientPlayerIdentifier().iValue != 0))
	{
		bWantsForceOpen = true;
		pcWantReason = "ClientPlayerId invalid";
	}
	else if (const Fleet* pFleet = gpGame->mFleetSelection.FocusedFleet(); pFleet == nullptr)
	{
		bWantsForceOpen = true;
		pcWantReason = "no focused fleet";
	}
	else
	{
		bool bFoundAny = false;
		for (const auto& [rCoordinate, rFrames] : gpGame->mCoordinateFrames)
		{
			if (rFrames.iSnapshotCount == 0)
			{
				continue;
			}
			++iSubscribedFrameCount;
			const PlayersPostRender& rPlayers = *gpGame->RenderFrame(rCoordinate).postRender.pPlayers;
			for (int64_t i = 0; i < rPlayers.iCount && !bFoundAny; ++i)
			{
				engine::GlobalId globalPlayerId = rPlayers.pGlobalPlayerIds[i];
				for (const FleetMember& rMember : pFleet->members)
				{
					if (rMember.globalPlayerId == globalPlayerId)
					{
						bFoundAny = true;
						break;
					}
				}
			}
			if (bFoundAny)
			{
				break;
			}
		}
		if (!bFoundAny)
		{
			bWantsForceOpen = true;
			pcWantReason = (iSubscribedFrameCount == 0)
				? "no subscribed snapshots"
				: "no fleet members in any subscribed frame";
		}
	}

	// Grace period: only force-open once the want-state has been sustained. Absorbs the brief gap during cell-boundary
	// hand-offs when the player snapshot is momentarily absent from every subscribed frame, plus ClientPlayerIdentifier blips.
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
		LOG(kDefault, kWarning, "HUD auto-unhide reason: {} coord: ({},{}) frames: {}", pcWantReason, gpGame->mClientGridCoordinate.iX, gpGame->mClientGridCoordinate.iY, iSubscribedFrameCount);
	}
	mbPreviousForceOpen = bForceOpen;

	// Right panel content gate: it has nothing useful to show without a focused player in current snapshot.
	std::optional<int64_t> oPlayerIndex;
	{
		auto it = gpGame->mCoordinateFrames.find(gpGame->mClientGridCoordinate);
		if (it != gpGame->mCoordinateFrames.end() && it->second.iSnapshotCount > 0)
		{
			oPlayerIndex = gpGame->ClientPlayerIndex(*gpGame->RenderFrame(gpGame->mClientGridCoordinate).postRender.pPlayers);
		}
	}
	bool bRightHasContent = oPlayerIndex.has_value();

	// With focused-player content, mouse proximity to either anchor opens both panels together.
	ImVec2 vLeftAnchor(rInputOutput.DisplaySize.x * kfHudEdgeMarginFraction, rInputOutput.DisplaySize.y * kfHudPanelTopFraction);
	ImVec2 vRightAnchor(rInputOutput.DisplaySize.x * (1.0f - kfHudEdgeMarginFraction), rInputOutput.DisplaySize.y * kfHudPanelTopFraction);

	// Hover activation zone is a fixed-extent strip (PanelWidth x max-height fraction at the anchor), decoupled from the
	// panels' content-driven live size so hover behavior is unchanged even as the panels visually shrink. Measure
	// PanelWidth() under the same scale+font scopes the panels render with so the strip width matches them exactly.
	ImVec2 vHoverExtent {};
	{
		engine::ScopedMenuScale menuScale;
		engine::ScopedMenuFont menuFont;
		vHoverExtent = ImVec2(PanelWidth(), rInputOutput.DisplaySize.y * kfHudPanelMaximumHeightFraction);
	}
	float fMouseLeft = engine::ComputeMouseOpennessTarget(vHoverExtent, vLeftAnchor, 0.0f);
	float fMouseRight = engine::ComputeMouseOpennessTarget(vHoverExtent, vRightAnchor, 1.0f);
	float fMouseTarget = std::max(fMouseLeft, fMouseRight);

	// Final shared targets. When right has content: both panels see max(force, mouse) — strict sync.
	// When right has no content: left can still auto-un-hide (force only), right stays hidden.
	float fForceTarget = bForceOpen ? 1.0f : 0.0f;
	float fLeftTarget = bRightHasContent ? std::max(fForceTarget, fMouseTarget) : fForceTarget;
	float fRightTarget = bRightHasContent ? std::max(fForceTarget, fMouseTarget) : 0.0f;

	RenderFleetPanel(fLeftTarget);
	RenderFocusedPlayerPanel(fRightTarget);
}

float HudScreen::PanelWidth()
{
	// Fixed row templates measured under the pushed menu font (caller pushes ScopedMenuFont first). Fixed
	// templates — not live content — keep the width stable as fleet members churn; measuring under the font auto-tracks
	// gUiFontScale/UiScale() with no magnifying multiplier.
	const ImGuiStyle& rStyle = ImGui::GetStyle();

	// Member-row template: content-spanning Selectable rows.
	float fMemberRowWidth = ImGui::CalcTextSize("Ship 88 (-888,-888) #8888888888").x;

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
	int64_t iWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
	engine::ScopedMenuFont menuFont;
	// Content-driven height: auto-resize to the fleet list, capped at kfHudPanelMaximumHeightFraction (long lists scroll). Width
	// pinned to PanelWidth() via the matching min/max constraint x.
	float fPanelWidth = PanelWidth();
	float fMaximumHeight = rInputOutput.DisplaySize.y * kfHudPanelMaximumHeightFraction;
	ImGui::SetNextWindowSize(ImVec2(fPanelWidth, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSizeConstraints(ImVec2(fPanelWidth, 0.0f), ImVec2(fPanelWidth, fMaximumHeight));
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
		gpClientSession->UpdateDesiredCoordinates(SubscriptionChangeReason::kFocusPreviousFleet);
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
		gpClientSession->UpdateDesiredCoordinates(SubscriptionChangeReason::kFocusNextFleet);
		LOG(kDefault, kVerbose, "HUD FocusNextFleet NewIndex: {} FleetCount: {}", gpGame->mFleetSelection.miFocusedFleetIndex, iFleetCount);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled((gpGame->mFleetSelection.mCreateFleetToggle.mFlags & engine::NetworkUiControlFlags::kPending) || iFleetCount >= kiMaximumFleetsPerClient);
	if (ImGui::Button("[+]##Fleet"))
	{
		if (gpClientSession != nullptr)
		{
			gpGame->mFleetSelection.mCreateFleetToggle.mFlags.Set(engine::NetworkUiControlFlags::kPending);
			gpClientSession->SendCreateFleetRequest();
			LOG(kDefault, kVerbose, "HUD CreateFleetRequest FleetCount: {}", iFleetCount);
		}
	}
	ImGui::EndDisabled();

	const Fleet* pFleet = gpGame->mFleetSelection.FocusedFleet();

	gpGame->mFleetSelection.mDeleteFleetToggle.Update(iFleetCount);
	bool bCanDelete = pFleet != nullptr && pFleet->members.empty();
	ImGui::SameLine();
	ImGui::BeginDisabled(!bCanDelete || (gpGame->mFleetSelection.mDeleteFleetToggle.mFlags & engine::NetworkUiControlFlags::kPending));
	if (ImGui::Button("[-]##Fleet"))
	{
		if (pFleet != nullptr && gpClientSession != nullptr)
		{
			gpGame->mFleetSelection.mDeleteFleetToggle.mFlags.Set(engine::NetworkUiControlFlags::kPending);
			gpClientSession->SendDeleteFleetRequest(pFleet->guid);
			LOG(kDefault, kVerbose, "HUD DeleteFleetRequest Fleet: ({},{}) FleetCount: {}", pFleet->guid.uiHigh, pFleet->guid.uiLow, iFleetCount);
		}
	}
	ImGui::EndDisabled();

	if (pFleet != nullptr)
	{
		ImGui::Separator();

		gpGame->mFleetSelection.mSpawnIntoFleetToggle.Update(std::ssize(pFleet->members));

		for (auto [i, rMember] : std::views::enumerate(pFleet->members))
		{
			bool bSelected = (rMember.globalPlayerId.iValue != 0) && rMember.globalPlayerId == gpGame->mFleetSelection.mFocusedMemberGlobalId;

			engine::GridCoord memberCoordinate {};
			for (int64_t j = 0; j < std::ssize(gpGame->mClientPlayerIdentifiers); ++j)
			{
				if (gpGame->mClientPlayerIdentifiers.at(j) == rMember.globalPlayerId)
				{
					memberCoordinate = gpGame->mClientPlayerCoordinates.at(j);
					break;
				}
			}

			ImGui::PushID(static_cast<int>(i));
			if (!(rMember.flags & FleetMemberFlags::kIsDead))
			{
				char pcLabel[64];
				std::snprintf(pcLabel, sizeof(pcLabel), "Ship %lld (%d,%d) #%lld", i + 1, memberCoordinate.iX, memberCoordinate.iY, rMember.globalPlayerId.iValue);
				if (ImGui::Selectable(pcLabel, bSelected))
				{
					gpGame->mFleetSelection.SelectPlayerInFleet(rMember.globalPlayerId);
					gpClientSession->UpdateDesiredCoordinates(SubscriptionChangeReason::kSelectPlayer);
				}
			}
			else
			{
				char pcLabel[64];
				std::snprintf(pcLabel, sizeof(pcLabel), "Ship %lld [DEAD] #%lld", i + 1, rMember.globalPlayerId.iValue);
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
				if (ImGui::Selectable(pcLabel, false))
				{
					if (gpClientSession != nullptr)
					{
						gpClientSession->SendRespawnInFleetRequest(pFleet->guid, rMember.globalPlayerId);
						LOG(kDefault, kVerbose, "HUD RespawnInFleet Fleet: ({},{}) Member: {}", pFleet->guid.uiHigh, pFleet->guid.uiLow, rMember.globalPlayerId.iValue);
					}
				}
				ImGui::PopStyleColor();
			}
			ImGui::PopID();
		}

		ImGui::BeginDisabled((gpGame->mFleetSelection.mSpawnIntoFleetToggle.mFlags & engine::NetworkUiControlFlags::kPending) || std::ssize(pFleet->members) >= kiMaximumFleetMembers);
		if (ImGui::Button("[+]##Player"))
		{
			if (gpClientSession != nullptr)
			{
				gpGame->mFleetSelection.mSpawnIntoFleetToggle.mFlags.Set(engine::NetworkUiControlFlags::kPending);
				gpClientSession->SendSpawnIntoFleetRequest(pFleet->guid);
				LOG(kDefault, kVerbose, "HUD SpawnIntoFleet Fleet: ({},{})", pFleet->guid.uiHigh, pFleet->guid.uiLow);
			}
		}
		ImGui::EndDisabled();

		ImGui::Separator();
		gpGame->mFleetSelection.mNavigationDelayControl.Update(NavigationDelayKey {.fleetGuid = pFleet->guid, .fNavigationDelay = pFleet->navigationDelaySeconds.count()});
		ImGui::BeginDisabled((gpGame->mFleetSelection.mNavigationDelayControl.mFlags & engine::NetworkUiControlFlags::kPending));
		static float sfNavigationDelayEditValue = 0.0f;
		static bool sbNavigationDelaySliderWasActive = false;
		// Reload only while the slider is inactive, so it owns the value for the whole interaction and a release
		// sends exactly the value the user let go on
		if (!sbNavigationDelaySliderWasActive)
		{
			sfNavigationDelayEditValue = pFleet->navigationDelaySeconds.count();
		}
		// Reserve the trailing label's width — AlwaysAutoResize windows default the item width to the full content
		// width, which would push the label past the clip edge
		ImGui::SetNextItemWidth(-(ImGui::CalcTextSize("Nav Delay").x + ImGui::GetStyle().ItemInnerSpacing.x));
		// Ctrl+Click typed input can be non-finite or out of range and the server rejects such a delay, so the
		// release below reverts it instead of clamping it; checking only on release leaves the text box alone while typing
		ImGui::SliderFloat("Nav Delay", &sfNavigationDelayEditValue, 0.0f, 60.0f, "%.3f");
		sbNavigationDelaySliderWasActive = ImGui::IsItemActive();
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			if (!PlayersPostRender::IsNavigationDelayInRange(sfNavigationDelayEditValue))
			{
				sfNavigationDelayEditValue = pFleet->navigationDelaySeconds.count();
			}
			// An unchanged synchronized delay leaves this fleet's request pending.
			if (gpClientSession != nullptr && sfNavigationDelayEditValue != pFleet->navigationDelaySeconds.count())
			{
				gpGame->mFleetSelection.mNavigationDelayControl.mFlags.Set(engine::NetworkUiControlFlags::kPending);
				gpClientSession->SendFleetNavigationDelayRequest(pFleet->guid, sfNavigationDelayEditValue);
			}
		}
		ImGui::EndDisabled();
	}

	ImGui::End();
}

void HudScreen::RenderFocusedPlayerPanel(float fTarget)
{
	ImGuiIO& rInputOutput = ImGui::GetIO();
	engine::ScopedMenuScale menuScale;

	std::optional<int64_t> oPlayerIndex = std::nullopt;
	{
		auto it = gpGame->mCoordinateFrames.find(gpGame->mClientGridCoordinate);
		if (it != gpGame->mCoordinateFrames.end() && it->second.iSnapshotCount > 0)
		{
			oPlayerIndex = gpGame->ClientPlayerIndex(*gpGame->RenderFrame(gpGame->mClientGridCoordinate).postRender.pPlayers);
		}
	}

	ImVec2 vAnchor(rInputOutput.DisplaySize.x * (1.0f - kfHudEdgeMarginFraction), rInputOutput.DisplaySize.y * kfHudPanelTopFraction);
	float fEdgeX = engine::UpdateSlideAndGetEdgeX(mFocusedPlayerSlide, vAnchor, 1.0f, fTarget);
	int64_t iWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	engine::ScopedMenuFont menuFont;
	// Match the left FleetPanel's size exactly (symmetry): same PanelWidth(), height forced to the left panel's live height
	// captured earlier this frame (RenderFleetPanel runs first). First frame (vLastSize.y still zero): fall back to the cap.
	float fLeftHeight = (mFleetSlide.vLastSize.y > 0.0f) ? mFleetSlide.vLastSize.y : (rInputOutput.DisplaySize.y * kfHudPanelMaximumHeightFraction);
	ImGui::SetNextWindowSize(ImVec2(PanelWidth(), fLeftHeight), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(fEdgeX, vAnchor.y), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
	ImGui::Begin("FocusedPlayerPanel", nullptr, static_cast<ImGuiWindowFlags>(iWindowFlags));
	mFocusedPlayerSlide.vLastSize = ImGui::GetWindowSize();
	engine::gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRectangle occlusion
	ImVec2 vPanelPosition = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	engine::DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPosition, ImVec2(vPanelPosition.x + vPanelSize.x, vPanelPosition.y + vPanelSize.y));

	if (oPlayerIndex.has_value())
	{
		PlayersPostRender& rPlayers = *gpGame->RenderFrame(gpGame->mClientGridCoordinate).postRender.pPlayers;
		bool bUseMissiles = static_cast<bool>(rPlayers.pFlags[*oPlayerIndex] & PlayerFlags::kUseMissiles);
		gpGame->mWeaponModeToggle.Update(WeaponModeKey {.playerIdentifier = gpGame->ClientPlayerIdentifier(), .bUseMissiles = bUseMissiles});

		const char* pcLabel = bUseMissiles ? "Missiles" : "Blasters";
		const ImGuiStyle& rStyle = ImGui::GetStyle();
		ImVec2 vLabelSize = ImGui::CalcTextSize(pcLabel);
		ImVec2 vButtonSize(vLabelSize.x + 2.0f * rStyle.FramePadding.x, vLabelSize.y + 2.0f * rStyle.FramePadding.y);
		ImVec2 vAvailable = ImGui::GetContentRegionAvail();
		ImVec2 vCursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(vCursor.x + std::max(0.0f, 0.5f * (vAvailable.x - vButtonSize.x)), vCursor.y + std::max(0.0f, 0.5f * (vAvailable.y - vButtonSize.y))));

		ImGui::BeginDisabled((gpGame->mWeaponModeToggle.mFlags & engine::NetworkUiControlFlags::kPending));
		if (ImGui::Button(pcLabel, vButtonSize))
		{
			if (gpClientSession != nullptr && (gpGame->ClientPlayerIdentifier().iValue != 0))
			{
				gpGame->mWeaponModeToggle.mFlags.Set(engine::NetworkUiControlFlags::kPending);
				float fNavigationDelay = rPlayers.pfNavigationDelays[*oPlayerIndex];
				gpClientSession->SendUpdatePlayerRequest(gpGame->ClientPlayerIdentifier().iValue, !bUseMissiles, fNavigationDelay);
			}
		}
		ImGui::EndDisabled();
	}

	ImGui::End();
}

} // namespace game

#endif // BT_CLIENT
