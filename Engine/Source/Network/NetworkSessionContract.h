#pragma once

namespace engine
{

template <typename T>
concept NetworkSessionContractType = requires
{
	typename T::Frame;
	typename T::StatusChange;
	typename T::GamePacket;
	{ T::Frame::kiVersion } -> std::convertible_to<int64_t>;
	{ T::kTickDuration } -> std::convertible_to<std::chrono::nanoseconds>;
	{ T::kiCoordinateSlots } -> std::convertible_to<int64_t>;
	{ T::kbDebugFrames } -> std::convertible_to<bool>;
	{ T::WriteFrame(std::declval<std::ostream&>(), std::declval<const typename T::Frame&>()) } -> std::same_as<void>;
	{ T::ReadFrame(std::declval<std::istream&>(), std::declval<typename T::Frame&>()) } -> std::same_as<void>;
	{ T::CompressStatusChanges(std::declval<std::span<const typename T::StatusChange>>(), std::declval<std::span<uint8_t>>()) } -> std::same_as<int64_t>;
	{ T::DecompressStatusChanges(std::declval<std::span<const uint8_t>>(), std::declval<typename T::StatusChange*>()) } -> std::same_as<int64_t>;
};

static_assert(NetworkSessionContractType<game::NetworkSessionContract>);

} // namespace engine
