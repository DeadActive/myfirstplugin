#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelSlicer;

struct BigButton : app::SvgSwitch {
	BigButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/BigButton.svg")));
	}
};

template <typename TBase>
struct BigButtonLight : TBase {
	BigButtonLight() {
		this->borderColor = color::BLACK_TRANSPARENT;
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = mm2px(math::Vec(14, 14));
	}
};

struct SmallButton: app::SvgSwitch {
    SmallButton() {
        momentary = true;
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/SmallButton.svg")));
    }
};

template <typename TBase>
struct SmallButtonLight : TBase {
    SmallButtonLight() {
        this->borderColor = color::BLACK_TRANSPARENT;
        this->bgColor = color::BLACK_TRANSPARENT;
        this->box.size = mm2px(math::Vec(5.6, 5.6));
    }
};


static const int numRatios = 35;
static const float ratioValues[numRatios] = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 23, 24, 29, 31, 32, 37, 41, 43, 47, 48, 53, 59, 61, 64, 96};
	
struct RatioParam : ParamQuantity {
	float getDisplayValue() override {
		int knobVal = (int) std::round(getValue());
		knobVal = clamp(knobVal, (numRatios - 1) * -1, numRatios - 1);
		if (knobVal < 0) {
			return -ratioValues[-knobVal];
		}
		return ratioValues[knobVal];
	}
	void setDisplayValue(float displayValue) override {
		bool div = displayValue < 0;
		float setVal;
		if (div) {
			displayValue = -displayValue;
		}
		// code below is not pretty, but easiest since irregular spacing of ratio values
		if (displayValue > 80.0f) {
			setVal = 34.0f;
		}
		else if (displayValue >= 62.5f) {
			setVal = 33.0f;
		}
		else if (displayValue >= 60.0f) {
			setVal = 32.0f;
		}
		else if (displayValue >= 56.0f) {
			setVal = 31.0f;
		}
		else if (displayValue >= 50.5f) {
			setVal = 30.0f;
		}
		else if (displayValue >= 47.5f) {
			setVal = 29.0f;
		}
		else if (displayValue >= 45.0f) {
			setVal = 28.0f;
		}
		else if (displayValue >= 42.0f) {
			setVal = 27.0f;
		}
		else if (displayValue >= 39.0f) {
			setVal = 26.0f;
		}
		else if (displayValue >= 34.5f) {
			setVal = 25.0f;
		}
		else if (displayValue >= 31.5f) {
			setVal = 24.0f;
		}
		else if (displayValue >= 30.0f) {
			setVal = 23.0f;
		}
		else if (displayValue >= 26.5f) {
			setVal = 22.0f;
		}
		else if (displayValue >= 23.5f) {
			setVal = 21.0f;
		}
		else if (displayValue >= 21.0f) {
			setVal = 20.0f;
		}
		else if (displayValue >= 18.0f) {
			setVal = 19.0f;
		}
		else if (displayValue >= 16.5f) {
			setVal = 18.0f;
		}
		else if (displayValue >= 2.75f) {
			// 3 to 16 map into 4 to 17
			setVal = 1.0 + std::round(displayValue);
		}
		else if (displayValue >= 2.25f) {
			setVal = 3.0f;
		}
		else if (displayValue >= 1.75f) {
			setVal = 2.0f;
		}
		else if (displayValue >= 1.25f) {
			setVal = 1.0f;
		}
		else {
			setVal = 0.0f;
		}
		if (setVal != 0.0f && div) {
			setVal *= -1.0f;
		}
		setValue(setVal);
	}	
	std::string getUnit() override {
		if (getValue() >= 0.0f) return std::string("x");
		return std::string(" (÷)");
	}
};

static const int numSizeOptions = 13;
// Fraction of one 4/4 bar; triplet entries are standard triplet note lengths within the bar.
static const float sizeOptionBarFractions[numSizeOptions] = {
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

inline float getSizeBarFraction(float paramValue) {
	int i = (int) std::round(paramValue);
	i = clamp(i, 0, numSizeOptions - 1);
	return sizeOptionBarFractions[i];
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
