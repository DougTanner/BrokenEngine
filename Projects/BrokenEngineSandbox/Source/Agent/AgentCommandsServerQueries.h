#pragma once

#if defined(BT_SERVER)

namespace game
{

// rParameters is the request "params" object; rResult receives the response "result" object.
// Invalid parameters throw into AgentCommandServer::Drain()'s failure envelope.
// Injection commands are in Commands/ServerSimulationHarnessRigs.cpp;
// frame edits and reads are in Commands/ServerFrameEdit.cpp, which shares the queries' cell-frame lookup.
// AgentCommandsServer.cpp owns the status and load take-overs and the server command dispatch.
Frame& QueryFrame(const nlohmann::json& rParameters);
void CommandQueryFrame(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandQueryCollection(const nlohmann::json& rParameters, nlohmann::json& rResult);

// The one JSON form of a ShipController or ShipType value in every server command, read and written alike.
inline const char* EnumToken(ShipController eController)
{
	return ShipControllerName(eController);
}

inline const char* EnumToken(ShipType eType)
{
	return ShipTypeName(eType);
}

// reValue is an out parameter so that a call from an engine template finds this function by argument-dependent lookup.
template <typename ENUM>
void EnumFromToken(const nlohmann::json& rValue, std::string_view name, ENUM& reValue)
{
	std::string tokens;
	for (int64_t i = 0; i < static_cast<int64_t>(ENUM::kCount); ++i)
	{
		ENUM eValue = static_cast<ENUM>(i);
		if (rValue.is_string() && rValue.get_ref<const std::string&>() == EnumToken(eValue))
		{
			reValue = eValue;
			return;
		}
		tokens += tokens.empty() ? "" : "|";
		tokens += EnumToken(eValue);
	}
	throw std::runtime_error(std::format("'{}' must be {}", name, tokens));
}

} // namespace game

#endif // defined(BT_SERVER)
