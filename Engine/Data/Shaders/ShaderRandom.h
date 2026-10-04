// 32-bit GLSL analogue of common::RandomEngine — same xorshift family + splitmix-style seed init,
// but width and shift triple necessarily differ from the 64-bit C++ version (Common/Math/Random.h).
// GLSL uint is universally supported; uint64 needs GL_ARB_gpu_shader_int64.
struct RandomEngine
{
	uint uiState;
};

uint RandomNext(inout RandomEngine rRandomEngine)
{
	uint uiState = rRandomEngine.uiState;
	uiState ^= uiState << 13;
	uiState ^= uiState >> 17;
	uiState ^= uiState << 5;
	rRandomEngine.uiState = uiState;
	return uiState;
}

float Random01(inout RandomEngine rRandomEngine)
{
	return float(RandomNext(rRandomEngine)) * (1.0f / 4294967296.0f);
}

// Splitmix-style seed mixing parallels common::RandomEngine::Seed in Common/Math/Random.cpp.
RandomEngine SeedRandomEngine(uint uiSeed)
{
	uint uiState = uiSeed + 0x9e3779b9u;
	uiState = (uiState ^ (uiState >> 16)) * 0x7feb352du;
	uiState = (uiState ^ (uiState >> 15)) * 0x846ca68bu;
	uiState ^= uiState >> 16;
	if (uiState == 0u)
	{
		uiState = 1u;
	}
	RandomEngine randomEngine;
	randomEngine.uiState = uiState;
	return randomEngine;
}
