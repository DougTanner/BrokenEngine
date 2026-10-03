#pragma once

namespace common
{

class InTheLastSecond
{
public:

	static constexpr int64_t kiCapacity = 1'024;

	void Set(int64_t iCount = 1);
	int64_t Get();

private:

	std::pair<std::chrono::steady_clock::time_point, int64_t> mFramesInTheLastSecond[kiCapacity] {};
	int64_t miHead = 0;
	int64_t miCount = 0;
};

template <typename VALUE_TYPE, int64_t COUNT = 128>
class Smoothed
{
public:

	static constexpr int64_t kiCapacity = COUNT;

	void operator=(VALUE_TYPE value)
	{
		mpValues[miNext++] = value;
		if (miNext == COUNT)
		{
			miNext = 0;
		}

		miCount = std::min(miCount + 1, COUNT);
	}

	VALUE_TYPE Maximum()
	{
		if (miCount == 0)
		{
			return {};
		}

		int64_t iCurrent = miNext - 1;
		if (iCurrent < 0)
		{
			iCurrent = COUNT - 1;
		}

		VALUE_TYPE maximum = mpValues[iCurrent];
		int64_t iCountLeft = miCount;
		while (iCountLeft > 0)
		{
			if (iCurrent < 0)
			{
				iCurrent = COUNT - 1;
			}

			maximum = std::max(maximum, mpValues[iCurrent]);

			--iCurrent;
			--iCountLeft;
		}

		return maximum;
	}

	VALUE_TYPE Current()
	{
		if (miCount == 0)
		{
			return {};
		}

		int64_t iCurrent = miNext - 1;
		if (iCurrent < 0)
		{
			iCurrent = COUNT - 1;
		}

		return mpValues[iCurrent];
	}

	VALUE_TYPE Average() const
	{
		if (miCount == 0)
		{
			return {};
		}

		VALUE_TYPE total = {};

		int64_t iCurrent = miNext - 1;
		int64_t iCountLeft = miCount;
		while (iCountLeft > 0)
		{
			if (iCurrent < 0)
			{
				iCurrent = COUNT - 1;
			}

			total += mpValues[iCurrent];

			--iCurrent;
			--iCountLeft;
		}

		return total /= static_cast<VALUE_TYPE>(miCount);
	}

	void Seed(VALUE_TYPE value)
	{
		*this = value;
		mSmoothedValue = value;
	}

	VALUE_TYPE Update()
	{
		static_assert(std::integral<VALUE_TYPE>, "Smoothed::Update() drift step is integer-only; instantiate with an integral VALUE_TYPE");

		if (miCount == 0)
		{
			return {};
		}

		VALUE_TYPE targetValue = Average();

		VALUE_TYPE difference = targetValue - mSmoothedValue;
		if (difference > -COUNT && difference < 0)
		{
			difference = -1;
		}
		else if (difference > 0 && difference < COUNT)
		{
			difference = 1;
		}
		mSmoothedValue += difference;

		return mSmoothedValue;
	}

	VALUE_TYPE mpValues[COUNT] {};
	int64_t miNext = 0;
	int64_t miCount = 0;

	VALUE_TYPE mSmoothedValue {};
};

} // namespace common
