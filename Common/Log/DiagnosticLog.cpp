#include "DiagnosticLog.h"

namespace common
{

DiagnosticLog::DiagnosticLog(int64_t iIndex, std::string_view filename)
: mFile((std::filesystem::create_directories(std::filesystem::path(filename).parent_path()), std::filesystem::path(filename)), std::ios::out | std::ios::trunc)
, miIndex(iIndex)
{
	ASSERT(gpDiagnosticLogs[miIndex] == nullptr);

	gpDiagnosticLogs[miIndex] = this;
}

DiagnosticLog::~DiagnosticLog()
{
	if (gpDiagnosticLogs[miIndex] == this)
	{
		gpDiagnosticLogs[miIndex] = nullptr;
	}
}

} // namespace common
