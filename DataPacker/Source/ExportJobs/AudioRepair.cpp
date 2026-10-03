#include "AudioRepair.h"

namespace audiorepair
{


constexpr float kfDcOffsetThreshold = 0.002f;
constexpr float kfPeakNormalizeThreshold = 1.001f; // a legit -32768 16-bit sample lands at 1.00003 after /32767 — don't rescale a whole file for one LSB
constexpr float kfEdgeAmplitudeThreshold = 0.01f;
constexpr int64_t kiEdgeProbeFrames = 8; // sub-0.2ms attack is a click even when frame 0 is small
constexpr std::chrono::duration<float> kfEdgeFadeDuration = std::chrono::duration<float>{0.003f}; // raised cosine; 132 samples @ 44.1kHz
constexpr int64_t kiEdgeFadeMinSamples = 16;

// Declip detection — shared by the fix path and every warn-only branch so reported numbers mean
// the same thing either way. Detection is relative to the channel's own peak so sources attenuated
// after clipping are still caught; the absolute floor keeps quiet material from false-positiving.
constexpr float kfClipRunLevelFraction = 0.999f;
constexpr float kfClipDetectMinPeak = 0.95f;
constexpr int64_t kiClipRunMinSamples = 3;
constexpr int64_t kiClipRunMaxFixSamples = 32; // longer runs: warn-only, unfixable by short-interval interpolation
constexpr int64_t kiClipSupportSamplesPerSide = 8;
constexpr int64_t kiClipSupportMinSamplesPerSide = 4;
constexpr float kfDeclipMaxReconstruction = 2.0f; // spline blowup cap
constexpr int64_t kiDeclipWarnOnlyRunCount = 256; // more runs per channel => mastering-style limiting, warn-only

constexpr float kfLoopSeamWarnThreshold = 0.005f;

struct ClipRun
{
	int64_t iStart = 0; // first clipped frame, inclusive
	int64_t iEnd = 0;   // last clipped frame, inclusive
	float fRailValue = 0.0f; // signed mean of the run's samples — the flat-top level
};

static int64_t ScrubNonFinite(std::vector<float>& rfSamples)
{
	int64_t iScrubbed = 0;
	for (float& rfSample : rfSamples)
	{
		if (!std::isfinite(rfSample))
		{
			rfSample = 0.0f;
			++iScrubbed;
		}
	}
	return iScrubbed;
}

// Returns the per-channel mean actually subtracted (0.0 when below threshold)
static float RemoveDcOffset(std::vector<float>& rfSamples, int64_t iChannels, int64_t iChannel)
{
	double fSum = 0.0;
	int64_t iFrames = std::ssize(rfSamples) / iChannels;
	for (int64_t i = 0; i < iFrames; ++i)
	{
		fSum += rfSamples[i * iChannels + iChannel];
	}
	float fMean = static_cast<float>(fSum / static_cast<double>(iFrames));
	if (std::abs(fMean) <= kfDcOffsetThreshold)
	{
		return 0.0f;
	}

	for (int64_t i = 0; i < iFrames; ++i)
	{
		rfSamples[i * iChannels + iChannel] -= fMean;
	}
	return fMean;
}

// Natural cubic spline second derivatives via the Thomas algorithm (M_first = M_last = 0),
// non-uniform x spacing. Doubles internally — trivial cost offline.
static void SolveNaturalCubicSpline(std::span<const double> samplePositions, std::span<const double> sampleValues, std::span<double> secondDerivatives)
{
	int64_t iCount = std::ssize(samplePositions);
	secondDerivatives[0] = 0.0;
	secondDerivatives[iCount - 1] = 0.0;
	if (iCount < 3)
	{
		return;
	}

	// Forward elimination over the interior tridiagonal system
	std::vector<double> fDiagonal(iCount, 0.0);
	std::vector<double> fRightHandSide(iCount, 0.0);
	for (int64_t i = 1; i < iCount - 1; ++i)
	{
		double fPreviousIntervalWidth = samplePositions[i] - samplePositions[i - 1];
		double fNextIntervalWidth = samplePositions[i + 1] - samplePositions[i];
		fDiagonal[i] = 2.0 * (fPreviousIntervalWidth + fNextIntervalWidth);
		fRightHandSide[i] = 6.0 * ((sampleValues[i + 1] - sampleValues[i]) / fNextIntervalWidth - (sampleValues[i] - sampleValues[i - 1]) / fPreviousIntervalWidth);

		if (i > 1)
		{
			double fFactor = fPreviousIntervalWidth / fDiagonal[i - 1];
			fDiagonal[i] -= fFactor * fPreviousIntervalWidth;
			fRightHandSide[i] -= fFactor * fRightHandSide[i - 1];
		}
	}

	// Back substitution
	for (int64_t i = iCount - 2; i >= 1; --i)
	{
		double fNextIntervalWidth = samplePositions[i + 1] - samplePositions[i];
		secondDerivatives[i] = (fRightHandSide[i] - fNextIntervalWidth * secondDerivatives[i + 1]) / fDiagonal[i];
	}
}

static double EvaluateCubicSpline(const double* pSamplePositions, const double* pSampleValues, const double* pSecondDerivatives, int64_t iInterval, double fSamplePosition)
{
	double fIntervalWidth = pSamplePositions[iInterval + 1] - pSamplePositions[iInterval];
	double fDistanceToRightSupport = pSamplePositions[iInterval + 1] - fSamplePosition;
	double fDistanceFromLeftSupport = fSamplePosition - pSamplePositions[iInterval];
	return pSecondDerivatives[iInterval] * fDistanceToRightSupport * fDistanceToRightSupport * fDistanceToRightSupport / (6.0 * fIntervalWidth)
		+ pSecondDerivatives[iInterval + 1] * fDistanceFromLeftSupport * fDistanceFromLeftSupport * fDistanceFromLeftSupport / (6.0 * fIntervalWidth)
		+ (pSampleValues[iInterval] / fIntervalWidth - pSecondDerivatives[iInterval] * fIntervalWidth / 6.0) * fDistanceToRightSupport
		+ (pSampleValues[iInterval + 1] / fIntervalWidth - pSecondDerivatives[iInterval + 1] * fIntervalWidth / 6.0) * fDistanceFromLeftSupport;
}

struct ClipRunAnalysis
{
	std::vector<uint8_t> uiClippedMask;
	std::vector<ClipRun> runs;
};

static ClipRunAnalysis DetectClipRuns(const std::vector<float>& rSamples, int64_t iFrames, int64_t iChannels, int64_t iChannel)
{
	ClipRunAnalysis analysis {};
	auto Sample = [&](int64_t iFrame) -> const float&
	{
		return rSamples[iFrame * iChannels + iChannel];
	};

	// Rails detect independently: an asymmetric source (e.g. clipped then DC-shifted) can have one
	// rail well below the other's peak, so a single per-channel level would miss it
	float fPositivePeak = 0.0f;
	float fNegativePeak = 0.0f;
	for (int64_t i = 0; i < iFrames; ++i)
	{
		float fSample = Sample(i);
		fPositivePeak = std::max(fPositivePeak, fSample);
		fNegativePeak = std::max(fNegativePeak, -fSample);
	}
	bool bDetectPositive = fPositivePeak >= kfClipDetectMinPeak;
	bool bDetectNegative = fNegativePeak >= kfClipDetectMinPeak;
	if (!bDetectPositive && !bDetectNegative)
	{
		return analysis;
	}

	float fPositiveClipLevel = kfClipRunLevelFraction * fPositivePeak;
	float fNegativeClipLevel = kfClipRunLevelFraction * fNegativePeak;
	analysis.uiClippedMask.assign(iFrames, 0);
	for (int64_t i = 0; i < iFrames; ++i)
	{
		float fSample = Sample(i);
		analysis.uiClippedMask[i] = ((bDetectPositive && fSample >= fPositiveClipLevel) || (bDetectNegative && fSample <= -fNegativeClipLevel)) ? 1 : 0;
	}

	for (int64_t i = 0; i < iFrames;)
	{
		if (analysis.uiClippedMask[i] == 0)
		{
			++i;
			continue;
		}
		bool bPositive = Sample(i) >= 0.0f;
		int64_t iStart = i;
		double fRailSum = 0.0;
		while (i < iFrames && analysis.uiClippedMask[i] != 0 && (Sample(i) >= 0.0f) == bPositive)
		{
			fRailSum += Sample(i);
			++i;
		}
		int64_t iLength = i - iStart;
		if (iLength >= kiClipRunMinSamples)
		{
			analysis.runs.push_back(ClipRun { .iStart = iStart, .iEnd = i - 1, .fRailValue = static_cast<float>(fRailSum / static_cast<double>(iLength)), });
		}
	}
	return analysis;
}

enum class DeclipPolicy
{
	kPervasiveWarning,
	kDisabledWarning,
	kRepair
};

struct DeclipPolicyClassification
{
	DeclipPolicy ePolicy = DeclipPolicy::kRepair;
	int64_t iLongestRun = 0;
};

static DeclipPolicyClassification ClassifyDeclipPolicy(const std::vector<ClipRun>& rRuns, bool bAllowDeclip)
{
	int64_t iLongestRun = 0;
	for (const ClipRun& rRun : rRuns)
	{
		iLongestRun = std::max(iLongestRun, rRun.iEnd - rRun.iStart + 1);
	}

	if (static_cast<int64_t>(rRuns.size()) > kiDeclipWarnOnlyRunCount)
	{
		return { .ePolicy = DeclipPolicy::kPervasiveWarning, .iLongestRun = iLongestRun, };
	}
	if (!bAllowDeclip)
	{
		return { .ePolicy = DeclipPolicy::kDisabledWarning, .iLongestRun = iLongestRun, };
	}
	return { .ePolicy = DeclipPolicy::kRepair, .iLongestRun = iLongestRun, };
}

struct DeclipStatistics
{
	int64_t iRunsFixed = 0;
	int64_t iLongestFixed = 0;
	int64_t iRunsSkipped = 0;
	float fMaxReconstruction = 0.0f;
};

static DeclipStatistics ReconstructClipRuns(std::vector<float>& rfSamples, int64_t iFrames, int64_t iChannels, int64_t iChannel, const std::vector<uint8_t>& ruiClippedMask, const std::vector<ClipRun>& rRuns)
{
	DeclipStatistics statistics {};
	auto Sample = [&](int64_t iFrame) -> float&
	{
		return rfSamples[iFrame * iChannels + iChannel];
	};

	std::vector<double> fSamplePositions;
	std::vector<double> fSampleValues;
	std::vector<double> fSecondDerivatives;
	for (const ClipRun& rRun : rRuns)
	{
		int64_t iLength = rRun.iEnd - rRun.iStart + 1;
		if (iLength > kiClipRunMaxFixSamples || rRun.iStart == 0 || rRun.iEnd == iFrames - 1)
		{
			++statistics.iRunsSkipped;
			continue;
		}

		// Nearest clean frames each side, skipping neighboring runs' rail samples (they bias the fit low)
		fSamplePositions.clear();
		fSampleValues.clear();
		int64_t iLeftCount = 0;
		for (int64_t i = rRun.iStart - 1; i >= 0 && iLeftCount < kiClipSupportSamplesPerSide; --i)
		{
			if (ruiClippedMask[i] == 0)
			{
				fSamplePositions.push_back(static_cast<double>(i));
				fSampleValues.push_back(Sample(i));
				++iLeftCount;
			}
		}
		if (iLeftCount < kiClipSupportMinSamplesPerSide)
		{
			++statistics.iRunsSkipped;
			continue;
		}
		std::reverse(fSamplePositions.begin(), fSamplePositions.end());
		std::reverse(fSampleValues.begin(), fSampleValues.end());

		int64_t iRightCount = 0;
		for (int64_t i = rRun.iEnd + 1; i < iFrames && iRightCount < kiClipSupportSamplesPerSide; ++i)
		{
			if (ruiClippedMask[i] == 0)
			{
				fSamplePositions.push_back(static_cast<double>(i));
				fSampleValues.push_back(Sample(i));
				++iRightCount;
			}
		}
		if (iRightCount < kiClipSupportMinSamplesPerSide)
		{
			++statistics.iRunsSkipped;
			continue;
		}

		fSecondDerivatives.assign(fSamplePositions.size(), 0.0);
		SolveNaturalCubicSpline(fSamplePositions, fSampleValues, fSecondDerivatives);

		// The gap lies in the interval between the innermost support points
		int64_t iGapInterval = iLeftCount - 1;
		bool bPositive = rRun.fRailValue >= 0.0f;
		for (int64_t i = rRun.iStart; i <= rRun.iEnd; ++i)
		{
			float fReconstructed = static_cast<float>(EvaluateCubicSpline(fSamplePositions.data(), fSampleValues.data(), fSecondDerivatives.data(), iGapInterval, static_cast<double>(i)));
			// Declip only restores magnitude the clip removed: force the run's sign and rail floor,
			// and cap against spline blowup on pathological support. min/max ordering (not std::clamp):
			// an over-full-scale float source can rail beyond the cap, and clamp with lo > hi is UB — the cap wins
			if (bPositive)
			{
				fReconstructed = std::min(std::max(fReconstructed, rRun.fRailValue), kfDeclipMaxReconstruction);
			}
			else
			{
				fReconstructed = std::max(std::min(fReconstructed, rRun.fRailValue), -kfDeclipMaxReconstruction);
			}
			Sample(i) = fReconstructed;
			statistics.fMaxReconstruction = std::max(statistics.fMaxReconstruction, std::abs(fReconstructed));
		}
		++statistics.iRunsFixed;
		statistics.iLongestFixed = std::max(statistics.iLongestFixed, iLength);
	}
	return statistics;
}

// The clipped mask stays fixed so each run reconstructs from the original clean samples, independent of run order.
static void DeclipChannel(std::vector<float>& rfSamples, int64_t iChannels, int64_t iChannel, bool bAllowDeclip, std::string_view relativeFile)
{
	int64_t iFrames = std::ssize(rfSamples) / iChannels;
	ClipRunAnalysis analysis = DetectClipRuns(rfSamples, iFrames, iChannels, iChannel);
	if (analysis.runs.empty())
	{
		return;
	}

	DeclipPolicyClassification classification = ClassifyDeclipPolicy(analysis.runs, bAllowDeclip);
	if (classification.ePolicy == DeclipPolicy::kPervasiveWarning)
	{
		LOG(kDefault, kWarning, "{}: pervasive clipping, {} runs (longest {}) on channel {} - left as-is (mastering-style limiting)", relativeFile, analysis.runs.size(), classification.iLongestRun, iChannel);
		return;
	}
	if (classification.ePolicy == DeclipPolicy::kDisabledWarning)
	{
		LOG(kDefault, kWarning, "{}: clipping detected, {} runs (longest {}) on channel {} - declip disabled for this asset", relativeFile, analysis.runs.size(), classification.iLongestRun, iChannel);
		return;
	}

	DeclipStatistics statistics = ReconstructClipRuns(rfSamples, iFrames, iChannels, iChannel, analysis.uiClippedMask, analysis.runs);
	if (statistics.iRunsFixed > 0)
	{
		LOG(kDefault, kWarning, "{}: declipped {} runs (longest {}, max reconstruction {:.3f}) on channel {}", relativeFile, statistics.iRunsFixed, statistics.iLongestFixed, statistics.fMaxReconstruction, iChannel);
	}
	if (statistics.iRunsSkipped > 0)
	{
		LOG(kDefault, kWarning, "{}: {} clip runs unfixable (too long, at file edge, or insufficient clean support) on channel {}", relativeFile, statistics.iRunsSkipped, iChannel);
	}
}

// Whole-file rescale by one factor across all channels (preserves the stereo image). Absorbs
// declip reconstruction overshoot and over-full-scale float sources.
static void NormalizePeak(std::vector<float>& rfSamples, std::string_view relativeFile)
{
	float fPeak = 0.0f;
	for (float fSample : rfSamples)
	{
		fPeak = std::max(fPeak, std::abs(fSample));
	}
	if (fPeak <= kfPeakNormalizeThreshold)
	{
		return;
	}

	float fScale = 1.0f / fPeak;
	for (float& rfSample : rfSamples)
	{
		rfSample *= fScale;
	}
	LOG(kDefault, kWarning, "{}: peak {:.4f} above full scale, rescaled by {:.4f}", relativeFile, fPeak, fScale);
}

static void ValidateLoopSeam(const std::vector<float>& rfSamples, int64_t iChannels, std::string_view relativeFile, bool bDcCorrected)
{
	int64_t iFrames = std::ssize(rfSamples) / iChannels;
	for (int64_t i = 0; i < iChannels; ++i)
	{
		float fSeamJump = std::abs(rfSamples[i] - rfSamples[(iFrames - 1) * iChannels + i]);
		if (fSeamJump > kfLoopSeamWarnThreshold)
		{
			LOG(kDefault, kWarning, "{}: loop seam mismatch {:.4f} > {:.4f} on channel {} - will click every loop iteration (warn only){}", relativeFile, fSeamJump, kfLoopSeamWarnThreshold, i, bDcCorrected ? "; DC offset was also corrected - check the source edit" : "");
		}
	}
}

// Raised-cosine fade-in/out when an edge is audibly non-zero. All channels fade together (fading
// one channel of a stereo pair would skew the image). Runs last: every earlier pass changes edge
// amplitudes, and measuring the final value avoids fading files an earlier pass already cured.
static void ApplyEdgeFades(std::vector<float>& rfSamples, int64_t iChannels, int64_t iSamplesPerSecond, std::string_view relativeFile, bool bAllowEdgeFades)
{
	int64_t iFrames = std::ssize(rfSamples) / iChannels;
	int64_t iFadeFrames = std::max(kiEdgeFadeMinSamples, static_cast<int64_t>(kfEdgeFadeDuration.count() * static_cast<float>(iSamplesPerSecond)));
	iFadeFrames = std::min(iFadeFrames, iFrames / 2);
	if (iFadeFrames < 2)
	{
		return;
	}

	// Probe a few frames in, not just the edge frame: a ramp to high amplitude within ~0.2ms is
	// still a click even when frame 0 itself is small
	int64_t iProbeFrames = std::min(kiEdgeProbeFrames, iFrames);
	float fOnsetPeak = 0.0f;
	float fTailPeak = 0.0f;
	for (int64_t i = 0; i < iProbeFrames; ++i)
	{
		for (int64_t j = 0; j < iChannels; ++j)
		{
			fOnsetPeak = std::max(fOnsetPeak, std::abs(rfSamples[i * iChannels + j]));
			fTailPeak = std::max(fTailPeak, std::abs(rfSamples[(iFrames - 1 - i) * iChannels + j]));
		}
	}

	if (fOnsetPeak > kfEdgeAmplitudeThreshold)
	{
		if (bAllowEdgeFades)
		{
			for (int64_t i = 0; i < iFadeFrames; ++i)
			{
				float fGain = 0.5f * (1.0f - std::cos(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(iFadeFrames)));
				for (int64_t j = 0; j < iChannels; ++j)
				{
					rfSamples[i * iChannels + j] *= fGain;
				}
			}
			LOG(kDefault, kWarning, "{}: faded {}-frame onset (edge amplitude {:.3f})", relativeFile, iFadeFrames, fOnsetPeak);
		}
		else
		{
			LOG(kDefault, kWarning, "{}: onset amplitude {:.3f} above {:.3f} (warn only)", relativeFile, fOnsetPeak, kfEdgeAmplitudeThreshold);
		}
	}

	if (fTailPeak > kfEdgeAmplitudeThreshold)
	{
		if (bAllowEdgeFades)
		{
			for (int64_t i = 0; i < iFadeFrames; ++i)
			{
				float fGain = 0.5f * (1.0f - std::cos(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(iFadeFrames)));
				for (int64_t j = 0; j < iChannels; ++j)
				{
					rfSamples[(iFrames - 1 - i) * iChannels + j] *= fGain;
				}
			}
			LOG(kDefault, kWarning, "{}: faded {}-frame tail (edge amplitude {:.3f})", relativeFile, iFadeFrames, fTailPeak);
		}
		else
		{
			LOG(kDefault, kWarning, "{}: tail amplitude {:.3f} above {:.3f} (warn only)", relativeFile, fTailPeak, kfEdgeAmplitudeThreshold);
		}
	}
}

// Modified Bessel function of the first kind, order 0 — series sum_{k>=0} ((x/2)^k / k!)^2.
// Doubles throughout; the series converges fast for the Kaiser beta range used here.
static double BesselI0(double fArgument)
{
	double fSum = 1.0;
	double fTerm = 1.0;
	double fHalfArgument = 0.5 * fArgument;
	for (int64_t k = 1; k < 64; ++k)
	{
		fTerm *= fHalfArgument / static_cast<double>(k);
		double fTermSquared = fTerm * fTerm;
		fSum += fTermSquared;
		if (fTermSquared < 1.0e-12 * fSum)
		{
			break;
		}
	}
	return fSum;
}

// Normalized sinc: sin(pi x) / (pi x), with the removable singularity at 0 handled.
static double NormalizedSinc(double fArgument)
{
	if (fArgument == 0.0)
	{
		return 1.0;
	}
	double fPiTimesArgument = std::numbers::pi_v<double> * fArgument;
	return std::sin(fPiTimesArgument) / fPiTimesArgument;
}


void Resample(std::vector<float>& rfSamples, int64_t iChannels, int64_t iSourceRate, int64_t iTargetRate, std::string_view relativeFile)
{
	if (iSourceRate == iTargetRate)
	{
		return;
	}
	if (rfSamples.empty())
	{
		return;
	}

	int64_t iSourceFrames = static_cast<int64_t>(rfSamples.size()) / iChannels;
	if (iSourceFrames < 2)
	{
		return;
	}

	// Input frames advanced per output frame; output count preserves duration.
	double fStep = static_cast<double>(iSourceRate) / static_cast<double>(iTargetRate);
	int64_t iTargetFrames = std::llround(static_cast<double>(iSourceFrames) / fStep);
	if (iTargetFrames < 1)
	{
		return;
	}

	// Kaiser-windowed sinc. Cutoff sits at the lower of the two Nyquists (input-relative), so
	// downsampling widens the kernel to band-limit before decimation; upsampling leaves it at the
	// input Nyquist. ~32 taps per output sample at unity rate (16 sinc zero-crossings each side).
	// Per-output weight-sum normalization pins DC gain to 1 and absorbs edge-clamp asymmetry.
	static constexpr int64_t kiZeroCrossings = 16;
	static constexpr double kfKaiserBeta = 9.0; // ~ -90 dB stopband
	double fCutoff = std::min(1.0, 1.0 / fStep); // == min(1, target/source)
	double fHalfWidth = static_cast<double>(kiZeroCrossings) / fCutoff; // support half-width, in input frames
	double fInverseI0Beta = 1.0 / BesselI0(kfKaiserBeta);

	std::vector<float> fResampled(static_cast<size_t>(iTargetFrames * iChannels));
	for (int64_t i = 0; i < iTargetFrames; ++i)
	{
		double fCenter = static_cast<double>(i) * fStep; // position in input frames
		int64_t iFirstTap = static_cast<int64_t>(std::ceil(fCenter - fHalfWidth));
		int64_t iLastTap = static_cast<int64_t>(std::floor(fCenter + fHalfWidth));

		double fAccumulator[2] = {0.0, 0.0}; // caller asserts iChannels <= 2
		double fWeightSum = 0.0;
		for (int64_t j = iFirstTap; j <= iLastTap; ++j)
		{
			double fDelta = fCenter - static_cast<double>(j);
			double fWindowArgument = fDelta / fHalfWidth;
			if (fWindowArgument <= -1.0 || fWindowArgument >= 1.0)
			{
				continue;
			}
			double fWindow = BesselI0(kfKaiserBeta * std::sqrt(1.0 - fWindowArgument * fWindowArgument)) * fInverseI0Beta;
			double fWeight = NormalizedSinc(fCutoff * fDelta) * fWindow;
			int64_t iClampedTap = std::clamp(j, static_cast<int64_t>(0), iSourceFrames - 1); // repeat edges
			for (int64_t k = 0; k < iChannels; ++k)
			{
				fAccumulator[k] += static_cast<double>(rfSamples[iClampedTap * iChannels + k]) * fWeight;
			}
			fWeightSum += fWeight;
		}

		double fInverseWeightSum = (fWeightSum != 0.0) ? 1.0 / fWeightSum : 0.0;
		for (int64_t j = 0; j < iChannels; ++j)
		{
			fResampled[i * iChannels + j] = static_cast<float>(fAccumulator[j] * fInverseWeightSum);
		}
	}

	rfSamples.swap(fResampled);
	LOG(kDefault, kInfo, "{}: resampled {} -> {} Hz ({} -> {} frames)", relativeFile, iSourceRate, iTargetRate, iSourceFrames, iTargetFrames);
}

void RepairAudio(std::vector<float>& rfSamples, int64_t iChannels, int64_t iSamplesPerSecond, std::string_view relativeFile, bool bLoopAsset, bool bAllowDeclip, bool bAllowEdgeFades)
{
	if (rfSamples.empty())
	{
		return;
	}

	// Pass order is load-bearing: scrub before any mean/peak, DC before
	// declip/normalize/fades, declip before normalize (reconstruction overshoot is absorbed there),
	// seam/fades last on final sample values.
	int64_t iScrubbed = ScrubNonFinite(rfSamples);
	if (iScrubbed > 0)
	{
		LOG(kDefault, kWarning, "{}: scrubbed {} non-finite samples to 0", relativeFile, iScrubbed);
	}

	bool bDcCorrected = false;
	for (int64_t i = 0; i < iChannels; ++i)
	{
		float fMean = RemoveDcOffset(rfSamples, iChannels, i);
		if (fMean != 0.0f)
		{
			bDcCorrected = true;
			LOG(kDefault, kWarning, "{}: DC offset {:+.4f} removed on channel {}", relativeFile, fMean, i);
		}
	}

	for (int64_t i = 0; i < iChannels; ++i)
	{
		DeclipChannel(rfSamples, iChannels, i, bAllowDeclip, relativeFile);
	}

	NormalizePeak(rfSamples, relativeFile);

	if (bLoopAsset)
	{
		ValidateLoopSeam(rfSamples, iChannels, relativeFile, bDcCorrected);
	}
	else
	{
		ApplyEdgeFades(rfSamples, iChannels, iSamplesPerSecond, relativeFile, bAllowEdgeFades);
	}
}

} // namespace audiorepair
