/*
    This file is part of Equalizer APO, a system-wide equalizer.
*/

#pragma once

#include <vector>

namespace iso226
{
struct Point
{
	double frequency;
	double alpha;
	double lu;
	double tf;
};

const std::vector<Point>& getTable();

double computeSpl(double frequency, double phon);
double interpolateSpl(double frequency, double phon);
}
