#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class CurveData
{
public:

	static constexpr int64_t kiMaximumControlPoints = 16;

	CurveData() = delete;

	CurveData(std::initializer_list<ImVec2> initial, float fYMinimum, float fYMaximum)
	: mfYMinimum(fYMinimum)
	, mfYMaximum(fYMaximum)
	{
		ASSERT(mfYMinimum < mfYMaximum);
		ASSERT(initial.size() >= 2);
		mPoints.reserve(kiMaximumControlPoints);
		for (const ImVec2& rPoint : initial)
		{
			mPoints.push_back(rPoint);
		}
	}

	~CurveData() = default;

	bool IsEndpoint(int64_t iIndex) const
	{
		return iIndex == 0 || iIndex == std::ssize(mPoints) - 1;
	}

	// Add a new control point. Clamps X to (0, 1) (endpoints stay reserved at exactly 0 and 1).
	// Returns index of the new point, or -1 if at cap.
	int64_t AddPoint(ImVec2 point)
	{
		if (std::ssize(mPoints) >= kiMaximumControlPoints)
		{
			return -1;
		}
		point.x = std::clamp(point.x, std::nextafter(0.0f, 1.0f), std::nextafter(1.0f, 0.0f));
		point.y = std::clamp(point.y, mfYMinimum, mfYMaximum);
		int64_t iInsert = 1;
		while (iInsert < std::ssize(mPoints) && mPoints.at(iInsert).x < point.x)
		{
			++iInsert;
		}
		mPoints.insert(mPoints.begin() + iInsert, point);
		return iInsert;
	}

	// Remove a non-endpoint point. Silently ignored for endpoints or when at the floor of 2 points.
	void RemovePoint(int64_t iIndex)
	{
		if (iIndex <= 0 || iIndex >= std::ssize(mPoints) - 1)
		{
			return;
		}
		if (std::ssize(mPoints) <= 2)
		{
			return;
		}
		mPoints.erase(mPoints.begin() + iIndex);
	}

	// Move a point. Endpoints lock X to 0 or 1; interior points clamp X between neighbors
	// (with a small epsilon) so the sort order is preserved.
	void MovePoint(int64_t iIndex, ImVec2 point)
	{
		if (iIndex < 0 || iIndex >= std::ssize(mPoints))
		{
			return;
		}
		point.y = std::clamp(point.y, mfYMinimum, mfYMaximum);
		if (iIndex == 0)
		{
			mPoints.at(0) = ImVec2(0.0f, point.y);
			return;
		}
		if (iIndex == std::ssize(mPoints) - 1)
		{
			mPoints.at(iIndex) = ImVec2(1.0f, point.y);
			return;
		}
		static constexpr float kfSeparation = 1.0e-4f;
		float fMinimumX = mPoints.at(iIndex - 1).x + kfSeparation;
		float fMaximumX = mPoints.at(iIndex + 1).x - kfSeparation;
		point.x = std::clamp(point.x, fMinimumX, fMaximumX);
		mPoints.at(iIndex) = point;
	}

	// Sample the curve at fCurveX in [0, 1] using monotone cubic Hermite (Fritsch-Carlson).
	float Evaluate(float fCurveX) const
	{
		fCurveX = std::clamp(fCurveX, 0.0f, 1.0f);
		int64_t iCount = std::ssize(mPoints);
		if (fCurveX <= mPoints.at(0).x)
		{
			return mPoints.at(0).y;
		}
		if (fCurveX >= mPoints.at(iCount - 1).x)
		{
			return mPoints.at(iCount - 1).y;
		}
		int64_t i = 0;
		while (i < iCount - 1 && mPoints.at(i + 1).x < fCurveX)
		{
			++i;
		}
		float fOutgoingTangent;
		float fIncomingTangent;
		ComputeTangents(i, fOutgoingTangent, fIncomingTangent);
		float fX0 = mPoints.at(i).x;
		float fX1 = mPoints.at(i + 1).x;
		float fY0 = mPoints.at(i).y;
		float fY1 = mPoints.at(i + 1).y;
		float fSegmentWidth = fX1 - fX0;
		float fSegmentFraction = (fCurveX - fX0) / fSegmentWidth;
		float fSegmentFractionSquared = fSegmentFraction * fSegmentFraction;
		float fSegmentFractionCubed = fSegmentFractionSquared * fSegmentFraction;
		float fLeftValueBasis = 2.0f * fSegmentFractionCubed - 3.0f * fSegmentFractionSquared + 1.0f;
		float fLeftTangentBasis = fSegmentFractionCubed - 2.0f * fSegmentFractionSquared + fSegmentFraction;
		float fRightValueBasis = -2.0f * fSegmentFractionCubed + 3.0f * fSegmentFractionSquared;
		float fRightTangentBasis = fSegmentFractionCubed - fSegmentFractionSquared;
		float fY = fLeftValueBasis * fY0 + fLeftTangentBasis * fSegmentWidth * fOutgoingTangent + fRightValueBasis * fY1 + fRightTangentBasis * fSegmentWidth * fIncomingTangent;
		return std::clamp(fY, mfYMinimum, mfYMaximum);
	}

private:

	// Fritsch-Carlson tangents at segment endpoints i (left) and i+1 (right).
	void ComputeTangents(int64_t iSegment, float& rfLeftTangent, float& rfRightTangent) const
	{
		rfLeftTangent = SecantTangent(iSegment);
		rfRightTangent = SecantTangent(iSegment + 1);
		// Preserve monotonicity on the current segment.
		float fDelta = Secant(iSegment);
		if (fDelta == 0.0f)
		{
			rfLeftTangent = 0.0f;
			rfRightTangent = 0.0f;
			return;
		}
		float fAlpha = rfLeftTangent / fDelta;
		float fBeta = rfRightTangent / fDelta;
		float fSquaredMagnitude = fAlpha * fAlpha + fBeta * fBeta;
		if (fSquaredMagnitude > 9.0f)
		{
			float fScale = 3.0f / std::sqrt(fSquaredMagnitude);
			rfLeftTangent = fScale * fAlpha * fDelta;
			rfRightTangent = fScale * fBeta * fDelta;
		}
	}

	// Average of adjacent secant slopes (or the single adjacent secant at boundaries).
	float SecantTangent(int64_t iIndex) const
	{
		int64_t iCount = std::ssize(mPoints);
		if (iIndex == 0)
		{
			return Secant(0);
		}
		if (iIndex == iCount - 1)
		{
			return Secant(iCount - 2);
		}
		float fLeft = Secant(iIndex - 1);
		float fRight = Secant(iIndex);
		if (fLeft * fRight <= 0.0f)
		{
			return 0.0f;
		}
		return 0.5f * (fLeft + fRight);
	}

	float Secant(int64_t iSegment) const
	{
		float fDeltaX = mPoints.at(iSegment + 1).x - mPoints.at(iSegment).x;
		return (mPoints.at(iSegment + 1).y - mPoints.at(iSegment).y) / fDeltaX;
	}

public:

	std::vector<ImVec2> mPoints;
	float mfYMinimum = 0.0f;
	float mfYMaximum = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
