#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/AgentInput.h"

#include "Agent/AgentUiRegistry.h"
#include "Input/RawInputManager.h"

namespace engine
{

constexpr int64_t kiWheelDelta = 120; // Win32 WHEEL_DELTA — one notch of the DirectXTK lifetime scroll accumulator
constexpr int64_t kiMaximumWheelNotches = INT32_MAX / kiWheelDelta;

AgentInput::AgentInput()
: common::Singleton<AgentInput>(gpAgentInput, this)
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
	mbResolvedDisabled = false;

	// Clear held synthetic keys so nothing leaks from a prior script; the scroll accumulator is a lifetime accumulator
	// by design (consumers diff it), so it persists.
	std::fill(std::begin(mpbSyntheticKeys), std::end(mpbSyntheticKeys), false);
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

	// Finish clears synthetic keys; scroll persists.
	std::fill(std::begin(mpbSyntheticKeys), std::end(mpbSyntheticKeys), false);
}

bool AgentInput::ActivateTarget()
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

	const AgentUiItem& rItem = gpAgentUiRegistry->mpItems[gpAgentUiRegistry->miRead][iIndex];
	mbResolvedDisabled = rItem.bDisabled;
	bool bSetSlider = mScript.eKind == AgentScriptKind::kSetSlider;
	// Prefer-input activation of a non-inputable item would press it instead of opening a text box, so validate first.
	if (bSetSlider && (rItem.iStatusFlags & ImGuiItemStatusFlags_Inputable) == 0)
	{
		Finish(AgentScriptStatus::kNotInputable);
		return false;
	}
	// ImGui::ActivateItemByID applies no disabled filter, and sliders and text fields accept input while disabled.
	if (rItem.bDisabled)
	{
		Finish(AgentScriptStatus::kDone);
		return false;
	}

	// The same remote activation ImGui's own navigation uses: ItemAdd does not clip an activated item, so an
	// offscreen item activates too, and the synthetic mouse never moves.
	ImGuiContext* pContext = ImGui::GetCurrentContext();
	ImGui::ActivateItemByID(rItem.uiIdentifier);
	if (bSetSlider)
	{
		// Opens the slider's temp text-input with its value selected instead of a drag.
		pContext->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
	}
	return true;
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

	ImGuiIO* pInputOutput = (ImGui::GetCurrentContext() != nullptr) ? &ImGui::GetIO() : nullptr;

	switch (mScript.eKind)
	{
		case AgentScriptKind::kClick:
		{
			if (miPhase == 0)
			{
				if (!ActivateTarget())
				{
					return;
				}
				miPhase = 1;
				miPhaseFrame = 0;
				return;
			}
			if (++miPhaseFrame >= 2) // settle
			{
				Finish(AgentScriptStatus::kDone);
			}
			return;
		}

		case AgentScriptKind::kSetSlider:
		{
			// Activation opens the slider's temp text-input (value pre-selected); type the value; Enter commits.
			if (miPhase == 0)
			{
				if (!ActivateTarget())
				{
					return;
				}
				miPhase = 1;
				return;
			}
			if (miPhase == 1)
			{
				if (pInputOutput != nullptr)
				{
					for (int64_t i = 0; mScript.pcValueText[i] != '\0'; ++i)
					{
						pInputOutput->AddInputCharacter(static_cast<unsigned int>(static_cast<unsigned char>(mScript.pcValueText[i])));
					}
				}
				miPhase = 2;
				return;
			}
			if (miPhase == 2)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiKey_Enter, true);
				}
				miPhase = 3;
				return;
			}
			if (miPhase == 3)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiKey_Enter, false);
				}
				miPhase = 4;
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

		case AgentScriptKind::kWheel:
		{
			// Camera-zoom sink only: RawInputManager folds the lifetime accumulator into the published wheel value.
			if (miPhase == 0)
			{
				miSyntheticScrollAccumulator += mScript.iWheelNotches * kiWheelDelta;
				miPhase = 1;
			}
			if (++miPhaseFrame >= 2)
			{
				Finish(AgentScriptStatus::kDone);
			}
			return;
		}

		case AgentScriptKind::kTypeText:
		{
			// ImGui IO only: type the text into the active text field, then press Enter on its own frames.
			if (miPhase == 0)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddInputCharactersUTF8(mScript.pcTypedText);
				}
				miPhase = 1;
				return;
			}
			if (miPhase == 1)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiKey_Enter, true);
				}
				miPhase = 2;
				return;
			}
			if (miPhase == 2)
			{
				if (pInputOutput != nullptr)
				{
					pInputOutput->AddKeyEvent(ImGuiKey_Enter, false);
				}
				miPhase = 3;
				return;
			}
			Finish(AgentScriptStatus::kDone);
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

	// NOTE: the synthetic scroll accumulator is deliberately NOT added here. iScrollWheelValue is a lifetime
	// accumulator that consumers diff, so its synthetic offset is folded in on every publish in RawInputManager::
	// Update (script active or not) — adding it here too would double-count it while a script runs.
}

} // namespace engine

#endif // defined(BT_CLIENT)
