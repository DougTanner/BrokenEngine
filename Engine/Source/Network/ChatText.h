#pragma once

namespace engine
{

// True when text is 1 to ClientChatMessage::kiMaxTextBytes bytes of well-formed UTF-8 (Unicode Table 3-7) holding no
// C0, DEL, or C1 control code point.
bool IsValidChatText(std::string_view text);

} // namespace engine
