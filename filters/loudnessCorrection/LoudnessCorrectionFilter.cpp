/*
    This file is part of Equalizer APO, a system-wide equalizer.
    Copyright (C) 2017  Alexander Walch
*/

#include "stdafx.h"
#include "LoudnessCorrectionFilter.h"
#include "ISO226.h"

#include <algorithm>
#include <cmath>

LoudnessCorrectionFilter::FilterParameters::FilterParameters()
	: enabled(true),
	referencePhon(80.0f),
	targetPhon(40.0f),
	strength(1.0f),
	maxBoostDb(18.0f),
	maxCutDb(18.0f),
	_isInitialized(false)
{
}

std::vector<char> LoudnessCorrectionFilter::FilterParameters::serialize()
{
	ParameterArchive archive;
	archive.add(enabled, L"Enabled");
	archive.add(referencePhon, L"ReferencePhon");
	archive.add(targetPhon, L"TargetPhon");
	archive.add(strength, L"Strength");
	archive.add(maxBoostDb, L"MaxBoostDb");
	archive.add(maxCutDb, L"MaxCutDb");
	return archive.getSerializedParameters();
}

LoudnessCorrectionFilter::LoudnessCorrectionFilter(const FilterParameters& fParameters)
	: _parameters(fParameters), _channelCount(0), _sampleRate(44100.0f), _neutral(true)
{
	_parameters.referencePhon = (float)clamp(_parameters.referencePhon, 20.0, 90.0);
	_parameters.targetPhon = (float)clamp(_parameters.targetPhon, 20.0, 90.0);
	_parameters.strength = (float)clamp(_parameters.strength, 0.0, 1.0);
	_parameters.maxBoostDb = (float)clamp(_parameters.maxBoostDb, 0.0, 30.0);
	_parameters.maxCutDb = (float)clamp(_parameters.maxCutDb, 0.0, 30.0);
}

LoudnessCorrectionFilter::~LoudnessCorrectionFilter()
{
}

std::vector<std::wstring> LoudnessCorrectionFilter::initialize(float sampleRate, unsigned, std::vector<std::wstring> channelNames)
{
	_channelCount = channelNames.size();
	_sampleRate = sampleRate;

	_lowShelfBiquads.resize(_channelCount);
	_lowMidPeakingBiquads.resize(_channelCount);
	_highMidPeakingBiquads.resize(_channelCount);
	_highShelfBiquads.resize(_channelCount);

	updateCurve();
	return channelNames;
}

double LoudnessCorrectionFilter::clamp(double value, double minValue, double maxValue)
{
	return std::max(minValue, std::min(maxValue, value));
}

double LoudnessCorrectionFilter::getCorrectionDb(double frequency) const
{
	// Sign convention: reference - target. If targetPhon is lower, low/high frequencies receive positive gain.
	const double refSpl = iso226::interpolateSpl(frequency, _parameters.referencePhon);
	const double targetSpl = iso226::interpolateSpl(frequency, _parameters.targetPhon);
	double correctionDb = (refSpl - targetSpl) * _parameters.strength;
	correctionDb = clamp(correctionDb, -_parameters.maxCutDb, _parameters.maxBoostDb);
	if (!_finite(correctionDb))
		return 0.0;
	return correctionDb;
}

void LoudnessCorrectionFilter::updateCurve()
{
	const double lowShelfGain = getCorrectionDb(60.0);
	const double lowMidGain = getCorrectionDb(180.0);
	const double highMidGain = getCorrectionDb(3500.0);
	const double highShelfGain = getCorrectionDb(9000.0);

	_neutral = !_parameters.enabled || _parameters.strength <= 0.0f ||
		std::max(std::max(std::abs(lowShelfGain), std::abs(lowMidGain)),
			std::max(std::abs(highMidGain), std::abs(highShelfGain))) < 0.05;

	for (size_t i = 0; i < _channelCount; i++)
	{
		_lowShelfBiquads[i] = BiQuad(BiQuad::LOW_SHELF, lowShelfGain, 80.0, _sampleRate, 0.707, false);
		_lowMidPeakingBiquads[i] = BiQuad(BiQuad::PEAKING, lowMidGain, 220.0, _sampleRate, 1.0, false);
		_highMidPeakingBiquads[i] = BiQuad(BiQuad::PEAKING, highMidGain, 3200.0, _sampleRate, 1.0, false);
		_highShelfBiquads[i] = BiQuad(BiQuad::HIGH_SHELF, highShelfGain, 9500.0, _sampleRate, 0.707, false);
	}
}

#pragma AVRT_CODE_BEGIN
void LoudnessCorrectionFilter::process(double** output, double** input, unsigned frameCount)
{
	if (_neutral)
	{
		for (unsigned j = 0; j < frameCount; j++)
		{
			for (unsigned i = 0; i < _channelCount; i++)
				output[i][j] = input[i][j];
		}
		return;
	}

	for (unsigned i = 0; i < _channelCount; i++)
	{
		double* inputChannel = input[i];
		double* outputChannel = output[i];
		for (unsigned j = 0; j < frameCount; j++)
		{
			double value = inputChannel[j];
			value = _lowShelfBiquads[i].process(value);
			value = _lowMidPeakingBiquads[i].process(value);
			value = _highMidPeakingBiquads[i].process(value);
			value = _highShelfBiquads[i].process(value);
			outputChannel[j] = value;

			_lowShelfBiquads[i].removeDenormals();
			_lowMidPeakingBiquads[i].removeDenormals();
			_highMidPeakingBiquads[i].removeDenormals();
			_highShelfBiquads[i].removeDenormals();
		}
	}
}
#pragma AVRT_CODE_END
