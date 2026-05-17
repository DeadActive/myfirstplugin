#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelSlice;

static const int numSizeOptions = 13;
// Fraction of one 4/4 bar; triplet entries are standard triplet note lengths within the bar.
static const float sizeOptionBarTripletFractions[numSizeOptions] = {
	2.f,
	1.f,
	0.5f,
	0.25f,
	1.f / 6.f,
	0.125f,
	1.f / 12.f,
	0.0625f,
	1.f / 24.f,
	0.03125f,
	0.015625f,
	0.0078125f,
	0.00390625f,
};
static const float sizeOptionBarFractions[numSizeOptions] = {
	2.f,
	1.f,
	0.5f,
	0.25f,
	0.25f,
	0.125f,
	0.235f,
	0.0625f,
	0.0625f,
	0.03125f,
	0.015625f,
	0.0078125f,
	0.00390625f,
};
static const char* const sizeOptionLabels[numSizeOptions] = {
	"2 Bars",
	"1 Bar",
	"1/2",
	"1/4",
	"1/4 trip.",
	"1/8",
	"1/8 trip.",
	"1/16",
	"1/16 trip.",
	"1/32",
	"1/64",
	"1/128",
	"1/256",
};

inline float getSizeBarFraction(float paramValue, bool triplet = false) {
	int i = (int) std::round(paramValue);
	i = clamp(i, 0, numSizeOptions - 1);
	return triplet ? sizeOptionBarTripletFractions[i] : sizeOptionBarFractions[i];
}

struct SizeParam : ParamQuantity {
	float getDisplayValue() override {
		return getSizeBarFraction(getValue());
	}
	std::string getDisplayValueString() override {
		int i = (int) std::round(getValue());
		i = clamp(i, 0, numSizeOptions - 1);
		return std::string(sizeOptionLabels[i]);
	}
	std::string getUnit() override {
		return "";
	}
};

inline uint32_t bpmToSamples(uint16_t bpm, uint16_t sampleRate) {
	return (sampleRate * 60) / bpm;
}
