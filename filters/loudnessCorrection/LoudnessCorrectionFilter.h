/*
    This file is part of Equalizer APO, a system-wide equalizer.
    Copyright (C) 2017  Alexander Walch

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#pragma once

#include "ParameterArchive.h"
#include <IFilter.h>
#include <filters/BiQuad.h>

#include <regex>

#pragma AVRT_VTABLES_BEGIN
class LoudnessCorrectionFilter : public IFilter
{
public:
	struct FilterParameters
	{
		bool enabled;
		float referencePhon;
		float targetPhon;
		float strength;
		float maxBoostDb;
		float maxCutDb;

		std::vector<char> serialize();
		template<typename T> bool deSerialize(const T& parameters);
		FilterParameters();
		template<typename T> FilterParameters(T input)
		{
			_isInitialized = !deSerialize<T>(input);
		}
		bool isInitialized() {return _isInitialized;}

	private:
		bool _isInitialized;
	};

	LoudnessCorrectionFilter(const FilterParameters& fParameters);
	virtual ~LoudnessCorrectionFilter();
	virtual bool getInPlace() {return true;}
	virtual std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount, std::vector<std::wstring> channelNames);
	virtual void process(double** output, double** input, unsigned frameCount);

private:
	void updateCurve();
	double getCorrectionDb(double frequency) const;
	static double clamp(double value, double minValue, double maxValue);

	FilterParameters _parameters;
	size_t _channelCount;
	float _sampleRate;
	std::vector<BiQuad> _lowShelfBiquads;
	std::vector<BiQuad> _lowMidPeakingBiquads;
	std::vector<BiQuad> _highMidPeakingBiquads;
	std::vector<BiQuad> _highShelfBiquads;
	bool _neutral;
};
#pragma AVRT_VTABLES_END

template<typename T> bool LoudnessCorrectionFilter::FilterParameters::deSerialize(const T& parameters)
{
	ParameterArchive archive(parameters);
	int error(0);
	error += archive.get(enabled, std::wregex(L"\\s*(Enabled|State)\\s+(0|1)"));

	// Backward compatibility: old format used ReferenceLevel/ReferenceOffset.
	float legacyReferenceLevel = 80.0f;
	float legacyReferenceOffset = 40.0f;
	int referenceError = archive.get(referencePhon, std::wregex(L"\\s*ReferencePhon\\s+([-+0-9]+((\\.|,)[0-9]+)?)"));
	if (referenceError > 0)
		referenceError = archive.get(legacyReferenceLevel, std::wregex(L"\\s*ReferenceLevel\\s+([-+0-9]+((\\.|,)[0-9]+)?)"));
	error += referenceError;

	int targetError = archive.get(targetPhon, std::wregex(L"\\s*TargetPhon\\s+([-+0-9]+((\\.|,)[0-9]+)?)"));
	if (targetError > 0)
	{
		int legacyError = archive.get(legacyReferenceOffset, std::wregex(L"\\s*ReferenceOffset\\s+([-+0-9]+((\\.|,)[0-9]+)?)"));
		if (legacyError == 0)
		{
			targetPhon = legacyReferenceLevel - legacyReferenceOffset;
			targetError = 0;
		}
	}
	error += targetError;

	int strengthError = archive.get(strength, std::wregex(L"\\s*(Strength|Attenuation)\\s+((1((\\.|,)0+)?)|(0((\\.|,)[0-9]+)?))"));
	if (strengthError > 0)
		strength = 1.0f;

	int maxBoostError = archive.get(maxBoostDb, std::wregex(L"\\s*MaxBoostDb\\s+([-+0-9]+((\\.|,)[0-9]+)?)"));
	if (maxBoostError > 0)
		maxBoostDb = 18.0f;
	int maxCutError = archive.get(maxCutDb, std::wregex(L"\\s*MaxCutDb\\s+([-+0-9]+((\\.|,)[0-9]+)?)"));
	if (maxCutError > 0)
		maxCutDb = 18.0f;

	return error == 0 ? false : true;
}
