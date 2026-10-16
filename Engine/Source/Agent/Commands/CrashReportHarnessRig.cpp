#include "Pch.h"

#include "Agent/Commands/CrashReportHarnessRig.h"

#include "CrashReport.h"

namespace engine
{

// Simulates a production crash on demand: writes a real crash report through the production handler (no exception
// object, so its text is "Unknown exception"), uploads it from a connected client as the main-loop crash path does,
// and exits. The exit deliberately leaves the request unanswered; the agent observes the crash as transport loss plus
// game exit code 0.
void CommandCrashReportHarnessRig([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("crash_report_harness_rig requires kbDebugInput build");
	}
	else
	{
		if (!rParameters.empty())
		{
			throw std::runtime_error("crash_report_harness_rig requires empty params");
		}

		HandleException();
#if defined(BT_CLIENT)
		TryUploadCrashReport();
#endif
		ExitProcess(0);
	}
}

} // namespace engine
