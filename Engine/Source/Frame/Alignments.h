#pragma once

namespace engine
{

// Simple alignment identifier (0 = invalid)
struct AlignmentIdentifier
{
	uint32_t uiValue = 0;

	constexpr AlignmentIdentifier() = default;
	constexpr explicit AlignmentIdentifier(int64_t iValue) : uiValue(static_cast<uint32_t>(iValue))
	{
	}

	constexpr bool operator==(const AlignmentIdentifier&) const = default;
	constexpr std::strong_ordering operator<=>(const AlignmentIdentifier&) const = default;

	void Write(std::ostream& rStream) const
	{
		common::Write(rStream, uiValue);
	}
	void Read(std::istream& rStream)
	{
		common::Read(rStream, uiValue);
	}
};

inline constexpr AlignmentIdentifier kInvalidAlignment {};

namespace AlignmentFlags
{
	inline constexpr uint8_t kuiNone = 0x00;
	inline constexpr uint8_t kuiEnemies = 0x01;
	inline constexpr uint8_t kuiAllies = 0x02;
} // namespace AlignmentFlags

struct AlignmentPair
{
	uint64_t uiKey = 0;
	uint8_t uiFlags = 0;

	bool operator==(const AlignmentPair&) const = default;
};

// Sorted flat vector for alignment collision filtering
// Keys are two 32-bit alignment IDs concatenated (lower ID first)
struct Alignments
{
	std::vector<AlignmentPair> alignmentPairs;

	void AddAlignment(AlignmentIdentifier alignmentA, AlignmentIdentifier alignmentB, uint8_t uiFlags);
	bool CanCollide(AlignmentIdentifier alignmentA, AlignmentIdentifier alignmentB) const;

	void CopyFrom(const Alignments& rOther);

	bool operator==(const Alignments& rOther) const;
	common::crc_t Crc() const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);

private:

	static uint64_t MakeAlignmentKey(AlignmentIdentifier alignmentA, AlignmentIdentifier alignmentB);
};

} // namespace engine
