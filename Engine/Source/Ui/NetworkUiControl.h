#pragma once

#if defined(BT_CLIENT)

namespace engine
{

enum class NetworkUiControlFlags : uint8_t
{
	kPending = 0x01,
};

template <typename T>
class NetworkUiControl
{
public:

	void Update(T currentState)
	{
		if (mFlags & NetworkUiControlFlags::kPending)
		{
			if (currentState != mStateWhenRequested)
			{
				mFlags.Set(NetworkUiControlFlags::kPending, false);
			}
		}
		else
		{
			mStateWhenRequested = currentState;
		}
	}

	void SetPending()
	{
		mFlags.Set(NetworkUiControlFlags::kPending);
	}

	bool IsPending() const
	{
		return mFlags & NetworkUiControlFlags::kPending;
	}

	void Reset()
	{
		mFlags.meFlags = static_cast<decltype(mFlags.meFlags)>(0);
		mStateWhenRequested = T {};
	}

private:

	common::Flags<NetworkUiControlFlags> mFlags {};
	T mStateWhenRequested {};
};

} // namespace engine

#endif // defined(BT_CLIENT)
