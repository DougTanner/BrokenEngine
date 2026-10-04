#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class DebugRender
{
public:

	static void Box(const XMFLOAT3A& rf3Position, const XMFLOAT3A& rf3Scale, const XMFLOAT4A& rf4Color);
	static void Sphere(const XMFLOAT3A& rf3Center, float fRadius, const XMFLOAT4A& rf4Color);
	static void Circle(const XMFLOAT3A& rf3Center, float fRadius, const XMFLOAT4A& rf4Color);
	static void Line(const XMFLOAT3A& rf3Start, const XMFLOAT3A& rf3End, const XMFLOAT4A& rf4Color);

	static inline bool msbEnabled = false;

	static void BeginRender(int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};

} // namespace engine

#endif // BT_CLIENT
