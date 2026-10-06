#pragma once

namespace engine
{

// Float-backed container for a setting (float / bool / discrete-enum flavors).
// Thread contract: no atomics. All writes (mfCurrent/Set/Reset/Toggle/SetPercent/SetIndex) are main-thread
// operations. The writer sites span several subsystems — menu/Tweaks screens (ImGuiManager::Prepare), input
// handling, render-target maintenance, swapchain creation, texture-capability clamps, and settings load, among
// others — so this is not a single-writer contract. mfCurrent is unsynchronized, so the invariant that keeps reads
// safe is not "single writer" but "no write overlaps an active gpMultithreading->Dispatch() window": the main thread
// is blocked inside Dispatch for the tick, so a wrapper read from a worker thread there (e.g. gBaseHeight in
// NavQuery.cpp) cannot race a concurrent writer.
class Wrapper
{
public:

	Wrapper() = delete;

	template <typename T>
	requires ((std::is_arithmetic_v<T> || std::is_enum_v<T>) && !std::is_same_v<std::remove_cv_t<T>, bool>)
	explicit Wrapper(T) = delete;

	explicit Wrapper(float fValue, float fMin, float fMax, float fStep = 0.0f)
	: mfStep(fStep)
	, mfDefault(Snap(fValue, fStep))
	, mfMin(Snap(fMin, fStep))
	, mfMax(Snap(fMax, fStep))
	, mfCurrent(mfDefault)
	, mfPrevious(mfCurrent)
	{
		// 500+ wrappers are file-scope globals built before main(), and these arguments are compile-time literals from
		// trusted internal callers: ASSERT here would throw out of a static initializer (immediate terminate, no
		// unwinding), so break instead — it logs a warning naming the site and never throws.
		if (fStep < 0.0f)
		{
			DEBUG_BREAK();
		}

		if (mfMin == mfMax)
		{
			DEBUG_BREAK();
		}

		if (mfDefault < mfMin || mfDefault > mfMax)
		{
			DEBUG_BREAK();
		}
	}

	explicit Wrapper(bool bValue)
	: mfDefault(bValue ? 1.0f : 0.0f)
	, mfCurrent(mfDefault)
	, mfPrevious(mfCurrent)
	{
	}

	template <typename T>
	Wrapper(T value, const std::vector<T>& rAllowedValues)
	: mAllowed([&rAllowedValues]
	{
		std::vector<float> allowed;
		allowed.reserve(static_cast<size_t>(std::ssize(rAllowedValues)));
		for (const T& rValue : rAllowedValues)
		{
			allowed.push_back(static_cast<float>(rValue));
		}

		return allowed;
	}())
	, mfDefault(static_cast<float>(value))
	, mfCurrent(mfDefault)
	, mfPrevious(mfCurrent)
	{
		for (const float& rfValue : mAllowed)
		{
			mfMax = std::max(rfValue, mfMax);
		}

		// Break for the same static-initialization reason as the float constructor above.
		if (mfMin == mfMax)
		{
			DEBUG_BREAK();
		}
	}

	~Wrapper() = default;

	template <typename T>
	std::tuple<T, T, bool> Changed()
	{
		std::tuple<T, T, bool> values = std::make_tuple(static_cast<T>(mfCurrent), static_cast<T>(mfPrevious), mfPrevious != mfCurrent);
		mfPrevious = mfCurrent;
		return values;
	}

	// Bool wrappers only: writes mfCurrent raw, bypassing Snap(), Set(float)'s clamp, and the discrete-enum
	// allowed-set check (GetIndex()). Set(bool) below shares the same raw-write nature. Callers: gFullscreen,
	// gDebugTexture (both bool).
	void Toggle()
	{
		mfCurrent = mfCurrent == 0.0f ? 1.0f : 0.0f;
	}

	// A NaN fails both comparisons and an infinity lies past the finite bounds, so this also rejects non-finite values.
	bool IsInRange(float fValue) const
	{
		return fValue >= mfMin && fValue <= mfMax;
	}

	template <typename T>
	T Get() const
	{
		static_assert(!std::is_same_v<T, float>);

		if constexpr (std::is_same_v<T, bool>)
		{
			return mfCurrent == 1.0f;
		}
		else
		{
			return static_cast<T>(mfCurrent);
		}
	}

	template <typename T>
	T GetDefault() const
	{
		static_assert(!std::is_same_v<T, float>);

		if constexpr (std::is_same_v<T, bool>)
		{
			return mfDefault == 1.0f;
		}
		else
		{
			return static_cast<T>(mfDefault);
		}
	}

	void Set(float fValue)
	{
		mfCurrent = std::clamp(Snap(fValue, mfStep), mfMin, mfMax);
	}

	void Set(bool bValue)
	{
		mfCurrent = bValue ? 1.0f : 0.0f;
	}

	template <typename T>
	void Set(T value)
	{
		static_assert(!std::is_same_v<T, float> && !std::is_same_v<T, bool>);

		mfCurrent = static_cast<float>(value);
		GetIndex();
	}

	template <typename T>
	void operator=(T value) = delete;

	void Reset(float fValue)
	{
		mfCurrent = mfPrevious = Snap(fValue, mfStep);
	}

	template <typename T>
	void Reset(T value)
	{
		static_assert(!std::is_same_v<T, float> && !std::is_same_v<T, bool>);

		mfCurrent = mfPrevious = static_cast<float>(value);
		GetIndex();
	}

	float Percent() const
	{
		return (mfCurrent - mfMin) / (mfMax - mfMin);
	}

	void SetPercent(float fPercent)
	{
		Set(mfMin + fPercent * (mfMax - mfMin));
	}

	int64_t GetIndex() const
	{
		for (int64_t i = 0; const float& rfValue : mAllowed)
		{
			if (mfCurrent == rfValue)
			{
				return i;
			}

			++i;
		}

		// Intentional soft-fall: an off-grid value is legitimately reachable — a corrupt/hand-edited persisted
		// gPresentMode or gUiTheme (settings-file load) — so this never throws. Returns index 0; the sole
		// return-consuming caller clamps it (TweaksScreenBase.cpp), the graphics path re-clamps present mode against
		// device support, and GetUiTheme() clamps the theme. DEBUG_BREAK logs a warning in every build and breaks
		// only when kbDebugBreak is enabled and a debugger is attached.
		DEBUG_BREAK();
		return 0;
	}

	void SetIndex(int64_t iIndex)
	{
		Set(mAllowed.at(iIndex));
	}

	// Discrete wrappers only; const because mfMax is derived from it at construction.
	const std::vector<float> mAllowed;

private:

	static float Snap(float fValue, float fStep)
	{
		return fStep > 0.0f ? std::round(fValue / fStep) * fStep : fValue;
	}

	// All wrapper flavors store floats; integer-backed values need exact float representation (every integer through
	// 2^24 is exact). gPresentMode includes FIFO_LATEST_READY_KHR (1000361000 rounds to 1000361024.0f). Settings can
	// load that value; SwapchainManager compares supported Mailbox/Immediate choices, defaults to FIFO, and resets the
	// wrapper before submitting the Vulkan swapchain request. The UI exposes only these three modes.
	float mfStep = 0.0f;

public:

	float mfDefault = 0.0f;
	float mfMin = 0.0f;
	float mfMax = 1.0f;

	float mfCurrent = 0.0f;

private:

	float mfPrevious = 0.0f;
};

// Internal-only wrappers (not bound to any UI: not Tweaks, not GraphicsMenuScreen, not SoundMenuScreen).
extern Wrapper gFieldOfView;
extern Wrapper gWireframe;
extern Wrapper gBaseHeight;

extern Wrapper gVisibleAreaExtraTop;
extern Wrapper gVisibleAreaExtraBottom;
extern Wrapper gTerrainElevationTextureMultiplier;

extern Wrapper gSmokeTrailPower;
extern Wrapper gSmokeTrailAlpha;


extern Wrapper gDebugTexture;
extern Wrapper gDebugTextureIndex;

} // namespace engine
