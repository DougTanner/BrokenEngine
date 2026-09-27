#include "LogDifference.h"

namespace common
{

bool XM_CALLCONV LogDifference_Vec(std::string_view name, FXMVECTOR rOne, FXMVECTOR rTwo)
{
	XMFLOAT4A f4One, f4Two;
	XMStoreFloat4A(&f4One, rOne);
	XMStoreFloat4A(&f4Two, rTwo);
	bool bEqual = std::memcmp(&f4One, &f4Two, sizeof(XMFLOAT4A)) == 0;

	if (!bEqual) [[unlikely]]
	{
		LOG(kNetwork, kError, "LogDifferences {} {} Client: {} Server: {}", gpLogDifferenceContext, name, WbV4(rOne, kiLogDifferencePrecision), WbV4(rTwo, kiLogDifferencePrecision));
	}

	return bEqual;
}

bool XM_CALLCONV LogDifference_Vec(std::string_view name, int64_t iIndex, FXMVECTOR rOne, FXMVECTOR rTwo)
{
	XMFLOAT4A f4One, f4Two;
	XMStoreFloat4A(&f4One, rOne);
	XMStoreFloat4A(&f4Two, rTwo);
	bool bEqual = std::memcmp(&f4One, &f4Two, sizeof(XMFLOAT4A)) == 0;

	if (!bEqual) [[unlikely]]
	{
		LOG(kNetwork, kError, "LogDifferences {} {}[{}] Client: {} Server: {}", gpLogDifferenceContext, name, iIndex, WbV4(rOne, kiLogDifferencePrecision), WbV4(rTwo, kiLogDifferencePrecision));
	}

	return bEqual;
}

} // namespace common
