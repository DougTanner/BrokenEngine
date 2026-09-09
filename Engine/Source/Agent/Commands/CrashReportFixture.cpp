#include "Pch.h"

#include "Agent/Commands/CrashReportFixture.h"

#include "CrashReport.h"

namespace engine
{

// Writes a real crash report through the production handler (no exception object, so its text is the deterministic
// "Unknown exception") and then exits. The exit is deliberate and intentionally leaves the request unanswered: the
// harness observes transport loss plus exit code 0, so no response is built here.
void CommandCrashReportFixture([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("crash_report_fixture requires kbDebugInput build");
	}
	else
	{
		if (!rParams.is_object())
		{
			throw std::runtime_error("crash_report_fixture requires empty params");
		}
		if (!rParams.empty())
		{
			throw std::runtime_error("crash_report_fixture requires empty params");
		}

		HandleException();
		ExitProcess(0);
	}
}

} // namespace engine
