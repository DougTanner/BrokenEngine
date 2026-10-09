#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/AgentInput.h"

#include "Agent/AgentUiRegistry.h"
#include "Input/RawInputManager.h"

namespace engine
{

constexpr int64_t kiWheelDelta = 120; // Win32 WHEEL_DELTA — one notch of the DirectXTK lifetime scroll accumulator
constexpr int64_t kiMaximumWheelNotches = INT32_MAX / kiWheelDelta;

// Rect stability threshold: max per-corner pixel delta below which two consecutive frames count as settled.
constexpr float kfRectangleStablePixels = 0.5f;

static float MaximumCornerDelta(const XMFLOAT4& rRectangleA, const XMFLOAT4& rRectangleB)
{
	float fDeltaMinimumX = std::fabs(rRectangleA.x - rRectangleB.x);
	float fDeltaMinimumY = std::fabs(rRectangleA.y - rRectangleB.y);
	float fDeltaMaximumX = std::fabs(rRectangleA.z - rRectangleB.z);
	float fDeltaMaximumY = std::fabs(rRectangleA.w - rRectangleB.w);
	return std::max(std::max(fDeltaMinimumX, fDeltaMinimumY), std::max(fDeltaMaximumX, fDeltaMaximumY));
}

AgentInput::AgentInput()
: common::Singleton<AgentInput>(gpAgentInput)
{
}

bool AgentInput::BeginScript(const AgentScript& rScript)
{
	if (mbScriptActive)
	{
		return false;
	}

	mScript = rScript;
	mbScriptActive = true;
	meStatus = AgentScriptStatus::kPending;

	miPhase = 0;
	miPhaseFrame = 0;
	miElapsedFrames = 0;
	miStableCount = 0;
	mbHaveLastRectangle = false;
	mbResolvedDisabled = false;

	// Clear held synthetic key/button/pos state so nothing leaks from a prior script; the scroll accumulator is a
	// lifetime accumulator by design (consumers diff it), so it persists. The ImGui-pos pin is cleared here (not in
	// Finish) so it persists across a script's completion: describe_ui runs after Finish, so it must survive until the
	// next script re-seeds or clears it.
	std::fill(std::begin(mpbSyntheticKeys), std::end(mpbSyntheticKeys), false);
	muiSyntheticMouseButtons = 0;
	mbSyntheticMousePosValid = false;
	mbImGuiMousePositionPinned = false;
	return true;
}

bool AgentInput::WheelNotchesFit(int64_t iNotches) const
{
	if (iNotches < -kiMaximumWheelNotches || iNotches > kiMaximumWheelNotches)
	{
		return false;
	}
	int64_t iSum = miSyntheticScrollAccumulator + iNotches * kiWheelDelta;
	return std::in_range<int>(iSum);
}

void AgentInput::Finish(AgentScriptStatus eStatus)
{
	meStatus = eStatus;

	// Finish clears synthetic keys/buttons and the game mouse position; scroll persists and the ImGui position stays pinned until BeginScript.
	// Mouse-command clicks emit down/up on both sinks; label clicks use ImGui only. Bare mouse-down releases the overlay after two frames but leaves ImGui held until mouse-up.
	std::fill(std::begin(mpbSyntheticKeys), std::end(mpbSyntheticKeys), false);
	muiSyntheticMouseButtons = 0;
	mbSyntheticMousePosValid = false;
}

void AgentInput::IssueImGuiMousePosition(float fX, float fY)
{
	// Remember the pos + pin it so ImGuiManager::Prepare can re-issue it after the Win32 backend (last-writer-wins).
	mbImGuiMousePositionPinned = true;
	mf2ImGuiPinnedPixels[0] = fX;
	mf2ImGuiPinnedPixels[1] = fY;
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGui::GetIO().AddMousePosEvent(fX, fY);
	}
}

void AgentInput::ReissueImGuiMousePosition()
{
	if (mbImGuiMousePositionPinned && ImGui::GetCurrentContext() != nullptr)
	{
		ImGui::GetIO().AddMousePosEvent(mf2ImGuiPinnedPixels[0], mf2ImGuiPinnedPixels[1]);
	}
}

bool AgentInput::StabilizeTarget()
{
	int64_t iIndex = gpAgentUiRegistry->ResolveLabel(mScript.pcLabel, mScript.bHasWindow ? mScript.pcWindow : nullptr);
	if (iIndex == AgentUiRegistry::kiNotFound)
	{
		Finish(AgentScriptStatus::kNotFound);
		return false;
	}
	if (iIndex == AgentUiRegistry::kiAmbiguous)
	{
		Finish(AgentScriptStatus::kAmbiguous);
		return false;
	}

	const AgentUiItem& rItem = gpAgentUiRegistry->mItems[gpAgentUiRegistry->miRead][iIndex];
	XMFLOAT4 f4Rectangle = rItem.f4Rectangle;
	mf2TargetCenter[0] = 0.5f * (f4Rectangle.x + f4Rectangle.z);
	mf2TargetCenter[1] = 0.5f * (f4Rectangle.y + f4Rectangle.w);
	IssueImGuiMousePosition(mf2TargetCenter[0], mf2TargetCenter[1]);

	if (mbHaveLastRectangle && MaximumCornerDelta(f4Rectangle, mf4LastRectangle) < kfRectangleStablePixels)
	{
		++miStableCount;
	}
	else
	{
		miStableCount = 0;
	}
	mf4LastRectangle = f4Rectangle;
	mbHaveLastRectangle = true;

	if (miElapsedFrames >= mScript.iTimeoutFrames)
	{
		Finish(AgentScriptStatus::kTimeout);
		return false;
	}

	if (miStableCount >= 2)
	{
		mbResolvedDisabled = rItem.bDisabled;
		miResolvedStatusFlags = rItem.iStatusFlags;
		// ImGuiItemStatusFlags_Visible is set only inside ItemAdd when the rect overlaps the clip rect (imgui.cpp:11292).
		// Checkbox/menu-item-class widgets emit ITEM_INFO on their clipped early-return too (e.g. Checkbox imgui_widgets.cpp:1247)
		// with StatusFlags lacking Visible — that is how a clipped item is label-resolvable yet reads not-visible. Widgets that
		// emit no clipped-path ITEM_INFO (sliders/drags/buttons/InvisibleButton) never gain a registry label when clipped and
		// resolve as kNotFound instead — except a nav-focused/active clipped widget, which bypasses ItemAdd's clip early-return
		// (imgui.cpp:11251) and still lands here without the Visible bit. This one site covers kClick, kHover, kSetSlider.
		if ((miResolvedStatusFlags & ImGuiItemStatusFlags_Visible) == 0)
		{
			Finish(AgentScriptStatus::kClipped);
			return false;
		}
		return true;
	}
	return false;
}

void AgentInput::AdvanceFrame()
{
	if (!mbScriptActive)
	{
		return;
	}

	// The script stays active through the frame that finished it, so an unfocused RawInputManager::Update still
	// publishes that frame's cleared synthetic state instead of freezing the last overlaid snapshot.
	if (meStatus != AgentScriptStatus::kPending)
	{
		mbScriptActive = false;
		return;
	}

	++miElapsedFrames;

	ImGuiIO* pInputOutput = (ImGui::GetCurrentContext() != nullptr) ? &ImGui::GetIO() : nullptr;

	switch (mScript.eKind)
	{
		case AgentScriptKind::kClick:
		{
			if (miPhase == 0)
			{
				if (!StabilizeTarget())
				{
					return;
				}
				miPhase = 1;
				miPhaseFrame = 0;
				return;
			}
			IssueImGuiMousePosition(mf2TargetCenter[0], mf2TargetCenter[1]);
			if (miPhase == 1)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddMouseButtonEvent(static_cast<int>(mScript.iImGuiMouseButton), true);
				}
				miPhase = 2;
				return;
			}
			if (miPhase == 2)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddMouseButtonEvent(static_cast<int>(mScript.iImGuiMouseButton), false);
				}
				miPhase = 3;
				miPhaseFrame = 0;
				return;
			}
			if (++miPhaseFrame >= 2) // settle
			{
				Finish(AgentScriptStatus::kDone);
			}
			return;
		}

		case AgentScriptKind::kHover:
		{
			if (miPhase == 0)
			{
				if (!StabilizeTarget())
				{
					return;
				}
				miPhase = 1;
				miPhaseFrame = 0;
				return;
			}
			IssueImGuiMousePosition(mf2TargetCenter[0], mf2TargetCenter[1]);
			if (++miPhaseFrame >= mScript.iHoldFrames)
			{
				Finish(AgentScriptStatus::kDone);
			}
			return;
		}

		case AgentScriptKind::kSetSlider:
		{
			if (miPhase == 0)
			{
				if (!StabilizeTarget())
				{
					return;
				}
				// Ctrl+click opens temporary input on inputable widgets and activates non-inputable targets, so validate Inputable first.
				if ((miResolvedStatusFlags & ImGuiItemStatusFlags_Inputable) == 0)
				{
					Finish(AgentScriptStatus::kNotInputable);
					return;
				}
				miPhase = 1;
				miPhaseFrame = 0;
				return;
			}
			IssueImGuiMousePosition(mf2TargetCenter[0], mf2TargetCenter[1]);
			// Ctrl+Click opens ImGui's temp text-input on the slider (value pre-selected); type the value; Enter commits.
			if (miPhase == 1)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiMod_Ctrl, true);
					pInputOutput->AddMouseButtonEvent(0, true);
				}
				miPhase = 2;
				return;
			}
			if (miPhase == 2)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddMouseButtonEvent(0, false);
					// Release Ctrl with the mouse-up: held into phase 3 it trips InputText's ignore_char_inputs, dropping typed chars.
					pInputOutput->AddKeyEvent(ImGuiMod_Ctrl, false);
				}
				miPhase = 3;
				return;
			}
			if (miPhase == 3)
			{
				if (pInputOutput != nullptr)
				{
					for (int64_t i = 0; mScript.pcValueText[i] != '\0'; ++i)
					{
						pInputOutput->AddInputCharacter(static_cast<unsigned int>(static_cast<unsigned char>(mScript.pcValueText[i])));
					}
				}
				miPhase = 4;
				return;
			}
			if (miPhase == 4)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiKey_Enter, true);
				}
				miPhase = 5;
				return;
			}
			if (miPhase == 5)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiKey_Enter, false);
				}
				miPhase = 6;
				miPhaseFrame = 0;
				return;
			}
			if (++miPhaseFrame >= 2) // settle
			{
				Finish(AgentScriptStatus::kDone);
			}
			return;
		}

		case AgentScriptKind::kKey:
		{
			// Overlay sink only: hold the synthetic VK down for iHoldFrames, then release, driving KeyboardPressed edges.
			if (miPhase == 0)
			{
				if (mScript.iVirtualKey > 0 && mScript.iVirtualKey < static_cast<int32_t>(std::size(mpbSyntheticKeys)))
				{
					mpbSyntheticKeys[mScript.iVirtualKey] = true;
				}
				miPhase = 1;
				miPhaseFrame = 0;
				return;
			}
			if (miPhase == 1)
			{
				if (++miPhaseFrame >= mScript.iHoldFrames)
				{
					if (mScript.iVirtualKey > 0 && mScript.iVirtualKey < static_cast<int32_t>(std::size(mpbSyntheticKeys)))
					{
						mpbSyntheticKeys[mScript.iVirtualKey] = false;
					}
					miPhase = 2;
				}
				return;
			}
			Finish(AgentScriptStatus::kDone); // release edge published in phase 1's final frame while still active
			return;
		}

		case AgentScriptKind::kMouse:
		{
			// Raw pixel coords feed both sinks (overlay for game world clicks, ImGui IO for UI). Pos re-pinned each frame.
			if (mScript.bHasCoordinate)
			{
				mbSyntheticMousePosValid = true;
				mf2SyntheticMousePixels[0] = mScript.f2CoordinatePixels[0];
				mf2SyntheticMousePixels[1] = mScript.f2CoordinatePixels[1];
				IssueImGuiMousePosition(mScript.f2CoordinatePixels[0], mScript.f2CoordinatePixels[1]);
			}

			switch (mScript.eMouseAction)
			{
				case AgentMouseAction::kMove:
				{
					if (++miPhaseFrame >= 2)
					{
						Finish(AgentScriptStatus::kDone);
					}
					return;
				}
				case AgentMouseAction::kWheel:
				{
					if (miPhase == 0)
					{
						miSyntheticScrollAccumulator += mScript.iWheelNotches * kiWheelDelta;
						if (pInputOutput != nullptr)
						{
							pInputOutput->AddMouseWheelEvent(0.0f, static_cast<float>(mScript.iWheelNotches));
						}
						miPhase = 1;
					}
					if (++miPhaseFrame >= 2)
					{
						Finish(AgentScriptStatus::kDone);
					}
					return;
				}
				case AgentMouseAction::kDown:
				{
					if (miPhase == 0)
					{
						muiSyntheticMouseButtons |= mScript.uiOverlayMouseButtonBit;
						if (pInputOutput != nullptr)
						{
							pInputOutput->AddMouseButtonEvent(static_cast<int>(mScript.iImGuiMouseButton), true);
						}
						miPhase = 1;
					}
					if (++miPhaseFrame >= 2)
					{
						Finish(AgentScriptStatus::kDone);
					}
					return;
				}
				case AgentMouseAction::kUp:
				{
					if (miPhase == 0)
					{
						muiSyntheticMouseButtons &= ~mScript.uiOverlayMouseButtonBit;
						if (pInputOutput != nullptr)
						{
							pInputOutput->AddMouseButtonEvent(static_cast<int>(mScript.iImGuiMouseButton), false);
						}
						miPhase = 1;
					}
					if (++miPhaseFrame >= 2)
					{
						Finish(AgentScriptStatus::kDone);
					}
					return;
				}
				case AgentMouseAction::kClick:
				{
					if (miPhase == 0)
					{
						muiSyntheticMouseButtons |= mScript.uiOverlayMouseButtonBit;
						if (pInputOutput != nullptr)
						{
							pInputOutput->AddMouseButtonEvent(static_cast<int>(mScript.iImGuiMouseButton), true);
						}
						miPhase = 1;
						miPhaseFrame = 0;
						return;
					}
					if (miPhase == 1)
					{
						muiSyntheticMouseButtons &= ~mScript.uiOverlayMouseButtonBit;
						if (pInputOutput != nullptr)
						{
							pInputOutput->AddMouseButtonEvent(static_cast<int>(mScript.iImGuiMouseButton), false);
						}
						miPhase = 2;
						miPhaseFrame = 0;
						return;
					}
					if (++miPhaseFrame >= 2)
					{
						Finish(AgentScriptStatus::kDone);
					}
					return;
				}
			}
			return;
		}
	}
}

void AgentInput::Overlay(RawInput& rRawInput)
{
	// OR synthetic keyboard bits onto the published snapshot (never clears real bits).
	for (int64_t i = 0; i < kiKeyboardKeyCount; ++i)
	{
		if (mpbSyntheticKeys[i])
		{
			rRawInput.pbKeyboardKeys[i] = true;
		}
	}

	if (muiSyntheticMouseButtons & std::to_underlying(MouseButtons::kMouseButtonLeft))
	{
		rRawInput.mouseButtons.Set(MouseButtons::kMouseButtonLeft, true);
	}
	if (muiSyntheticMouseButtons & std::to_underlying(MouseButtons::kMouseButtonMiddle))
	{
		rRawInput.mouseButtons.Set(MouseButtons::kMouseButtonMiddle, true);
	}
	if (muiSyntheticMouseButtons & std::to_underlying(MouseButtons::kMouseButtonRight))
	{
		rRawInput.mouseButtons.Set(MouseButtons::kMouseButtonRight, true);
	}
	if (muiSyntheticMouseButtons & std::to_underlying(MouseButtons::kMouseButtonExtraOne))
	{
		rRawInput.mouseButtons.Set(MouseButtons::kMouseButtonExtraOne, true);
	}
	if (muiSyntheticMouseButtons & std::to_underlying(MouseButtons::kMouseButtonExtraTwo))
	{
		rRawInput.mouseButtons.Set(MouseButtons::kMouseButtonExtraTwo, true);
	}

	// Overwrite mouse position with the synthetic pixel pos (normalized as the hardware path does).
	if (mbSyntheticMousePosValid)
	{
		rRawInput.f2MousePosition.x = mf2SyntheticMousePixels[0] / static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
		rRawInput.f2MousePosition.y = mf2SyntheticMousePixels[1] / static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	}

	// NOTE: the synthetic scroll accumulator is deliberately NOT added here. iScrollWheelValue is a lifetime
	// accumulator that consumers diff, so its synthetic offset is folded in on every publish in RawInputManager::
	// Update (script active or not) — adding it here too would double-count it while a script runs.
}

} // namespace engine

#endif // defined(BT_CLIENT)
