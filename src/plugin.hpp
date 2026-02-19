#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelBlankPanel;
extern Model* modelPush;


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
