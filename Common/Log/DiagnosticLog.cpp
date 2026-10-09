#include "DiagnosticLog.h"

namespace common
{

DiagnosticLog::DiagnosticLog(int64_t iIndex, std::string_view filename)
: common::Singleton<DiagnosticLog>(gpDiagnosticLogs[iIndex])
, mFile((std::filesystem::create_directories(std::filesystem::path(filename).parent_path()), std::filesystem::path(filename)), std::ios::out | std::ios::trunc)
{
}

} // namespace common
