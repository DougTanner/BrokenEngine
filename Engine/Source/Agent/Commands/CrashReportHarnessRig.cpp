#include "Pch.h"

#include "Agent/Commands/CrashReportHarnessRig.h"

#include "CrashReport.h"

namespace engine
{

// Writes a real crash report through the production handler (no exception object, so its text is the deterministic
// "Unknown exception") and then exits. The exit is deliberate and intentionally leaves the request unanswered: the
// harness observes transport loss plus exit code 0, so no response is built here.
void CommandCrashReportHarnessRig([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("crash_report_harness_rig requires kbDebugInput build");
	}
	else
	{
		if (!rParameters.is_object())
		{
			throw std::runtime_error("crash_report_harness_rig requires empty params");
		}
		if (!rParameters.empty())
		{
			throw std::runtime_error("crash_report_harness_rig requires empty params");
		}

		HandleException();
		ExitProcess(0);
	}
}

} // namespace engine
