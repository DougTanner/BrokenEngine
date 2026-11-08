#if defined(BT_CLIENT)

#include "Ui/Screens/TweaksScreen/TweaksScreen.h"
#include "Ui/Screens/ChatScreen.h"
#include "Ui/Screens/GameSettingsScreen.h"
#include "Ui/Screens/GraphicsMenuScreen.h"
#include "Ui/Screens/MainMenuScreen.h"
#include "Ui/Screens/ModalScreen.h"
#include "Ui/Screens/PauseMenuScreen.h"
#include "Ui/Screens/SoundMenuScreen.h"

#include "Ui/Screens/HudScreen.h"
#include "Ui/Screens/MapScreen.h"
#include "Game.h"

namespace engine
{

static std::unique_ptr<MainMenuScreen> spMainMenuScreen;
static std::unique_ptr<ModalScreen> spModalScreen;
static std::unique_ptr<PauseMenuScreen> spPauseMenuScreen;
static std::unique_ptr<GraphicsMenuScreen> spGraphicsMenuScreen;
static std::unique_ptr<SoundMenuScreen> spSoundMenuScreen;
static std::unique_ptr<GameSettingsScreen> spGameSettingsScreen;
static std::unique_ptr<game::HudScreen> spHudScreen;
static std::unique_ptr<game::MapScreen> spMapScreen;
static std::unique_ptr<ChatScreen> spChatScreen;

void CreateUiScreens()
{
	spMainMenuScreen = std::make_unique<MainMenuScreen>();
	spModalScreen = std::make_unique<ModalScreen>();
	spPauseMenuScreen = std::make_unique<PauseMenuScreen>();
	spGraphicsMenuScreen = std::make_unique<GraphicsMenuScreen>();
	spSoundMenuScreen = std::make_unique<SoundMenuScreen>();
	spGameSettingsScreen = std::make_unique<GameSettingsScreen>();
	spHudScreen = std::make_unique<game::HudScreen>();
	spMapScreen = std::make_unique<game::MapScreen>();
	spChatScreen = std::make_unique<ChatScreen>();
}

void RenderUiScreens()
{
	// First, under every other screen, and whatever the UI state: a world-hidden frame shows only this overlay of the world.
	spMapScreen->Render();

	// Called whatever Tweaks shows, because its discovery and auto-connect must keep running; it draws nothing while Tweaks is open.
	spMainMenuScreen->Render();

	// A Tweaks slider drag hides every screen but the map screen, so the dragged value's effect on the scene stays visible.
	if (gbShowImGui && !GetTweaksScreen().mActiveSlider.empty())
	{
		return;
	}

	// The modal must render even with Tweaks open, or a persisted-active Tweaks menu strands a rejected client
	// behind an unacknowledgeable modal. It self-gates on the game's UI state.
	spModalScreen->Render();

	if (!gbShowImGui)
	{
		spHudScreen->Render();
		spChatScreen->Render();
		spPauseMenuScreen->Render();
		spGraphicsMenuScreen->Render();
		spSoundMenuScreen->Render();
		spGameSettingsScreen->Render();
	}

	RenderImPlotGraphs();
	gpImGuiManager->RenderTextAreas();
}

void DestroyUiScreens()
{
	spMainMenuScreen.reset();
	spModalScreen.reset();
	spPauseMenuScreen.reset();
	spGraphicsMenuScreen.reset();
	spSoundMenuScreen.reset();
	spGameSettingsScreen.reset();
	spHudScreen.reset();
	spMapScreen.reset();
	spChatScreen.reset();
}

} // namespace engine

#endif // defined(BT_CLIENT)
