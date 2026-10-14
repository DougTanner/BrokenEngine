#include "ChatScreen.h"

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"

#include "Game.h"

namespace engine
{

constexpr float kfChatEdgeMarginFraction = 0.025f;
constexpr float kfChatWidthFraction = 0.3f;
constexpr float kfChatHistoryMaximumHeightFraction = 0.4f;
constexpr std::chrono::seconds kChatFadeDuration = 8s;

void ChatScreen::Render()
{
	ChatLog& rChatLog = game::gpGame->mChatLog;
	if ((game::gpGame->mGameFlags & GameFlags::kMainMenu) || game::gpGame->meUiState != UiState::kNone)
	{
		rChatLog.bInputOpen = false;
		rChatLog.pcInput[0] = '\0';
		return;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	ScopedMenuScale menuScale;
	ScopedMenuFont menuFont;

	ImVec2 vAnchor(rInputOutput.DisplaySize.x * kfChatEdgeMarginFraction, rInputOutput.DisplaySize.y * (1.0f - kfChatEdgeMarginFraction));
	float fWidth = rInputOutput.DisplaySize.x * kfChatWidthFraction;

	// The input box sits at the anchor and the history stacks on its top edge, so the box is drawn first.
	if (rChatLog.bInputOpen)
	{
		ImGui::SetNextWindowPos(vAnchor, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
		ImGui::SetNextWindowSize(ImVec2(fWidth, 0.0f));
		ImGui::Begin("ChatInput", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);

		// Appearing covers both the opening frame and an ImGuiManager recreation while the box stays open.
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
			// Holding the Enter that opened the box would otherwise repeat into the field and close it at once.
			ImGui::SetKeyOwner(ImGuiKey_Enter, ImGuiKeyOwner_Any, ImGuiInputFlags_LockUntilRelease);
			ImGui::SetKeyOwner(ImGuiKey_KeypadEnter, ImGuiKeyOwner_Any, ImGuiInputFlags_LockUntilRelease);
		}
		ImGui::SetNextItemWidth(-std::numeric_limits<float>::min());
		bool bEntered = ImGui::InputText("##Chat", rChatLog.pcInput, sizeof(rChatLog.pcInput), ImGuiInputTextFlags_EnterReturnsTrue);
		if (bEntered && gpClient != nullptr)
		{
			gpClient->SendChat(rChatLog.pcInput);
		}

		// Deactivation without Enter is Esc (InputText's own cancel) or a click elsewhere.
		if (bEntered || ImGui::IsItemDeactivated())
		{
			rChatLog.bInputOpen = false;
			rChatLog.pcInput[0] = '\0';
		}

		vAnchor.y = ImGui::GetWindowPos().y;
		ImGui::End();
	}

	// NoFocusOnAppearing: focusing this window would deactivate the input box while it is being typed in.
	ImGui::SetNextWindowPos(vAnchor, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(fWidth, 0.0f), ImVec2(fWidth, rInputOutput.DisplaySize.y * kfChatHistoryMaximumHeightFraction));
	ImGui::Begin("ChatHistory", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	for (int64_t i = 0; i < rChatLog.iCount; ++i)
	{
		const ChatLog::Entry& rEntry = rChatLog.pEntries[(rChatLog.iHead + i) % ChatLog::kiCapacity];
		float fAlpha = rChatLog.bInputOpen ? 1.0f : 1.0f - std::chrono::duration<float>(now - rEntry.receivedTime) / kChatFadeDuration;
		if (fAlpha <= 0.0f)
		{
			continue;
		}

		// The label is fixed mixed-case text outside the upper-cased translation table.
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fAlpha);
		ImGui::TextWrapped("Player %lld: %s", rEntry.iSenderId, rEntry.pcText);
		ImGui::PopStyleVar();
	}

	// Keeps the newest line visible once the open box shows more history than the height cap.
	ImGui::SetScrollHereY(1.0f);
	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
