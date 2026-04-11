#include "plugin.hpp"
#include <cmath>

// Seven-segment layout: 0=top, 1=top-right, 2=bottom-right, 3=middle, 4=bottom-left, 5=top-left, 6=bottom
static const bool SEGMENT_MAP[10][7] = {
	{1, 1, 1, 0, 1, 1, 1}, // 0
	{0, 1, 1, 0, 0, 0, 0}, // 1
	{1, 1, 0, 1, 1, 0, 1}, // 2
	{1, 1, 1, 1, 0, 0, 1}, // 3
	{0, 1, 1, 1, 0, 1, 0}, // 4
	{1, 0, 1, 1, 0, 1, 1}, // 5
	{1, 0, 1, 1, 1, 1, 1}, // 6
	{1, 1, 1, 0, 0, 0, 0}, // 7
	{1, 1, 1, 1, 1, 1, 1}, // 8
	{1, 1, 1, 1, 0, 1, 1}, // 9
};
void drawSSSegment(NVGcontext* vg, int seg, float x, float y, float w, float segH, float thick, float gapX, float gapY) {
	float y1 = y + thick;
		float y2 = y1 + segH;
		float y3 = y2 + thick;
		float y4 = y3 + segH;
		switch (seg) {
			case 0: nvgRect(vg, x + gapX, y, w - 2.f * gapX, thick); break;
			case 5: nvgRect(vg, x, y1 + gapY, thick, segH - 2.f * gapY); break;
			case 1: nvgRect(vg, x + w - thick, y1 + gapY, thick, segH - 2.f * gapY); break;
			case 3: nvgRect(vg, x + gapX, y2, w - 2.f * gapX, thick); break;
			case 4: nvgRect(vg, x, y3 + gapY, thick, segH - 2.f * gapY); break;
			case 2: nvgRect(vg, x + w - thick, y3 + gapY, thick, segH - 2.f * gapY); break;
			case 6: nvgRect(vg, x + gapX, y4, w - 2.f * gapX, thick); break;
			default: break;
		}
}

void drawSSLayer(NVGcontext* vg, math::Rect rect, float padding, float thickness, float gapX, float gapY, NVGcolor color, int value) {
		float W = rect.size.x;
		float H = rect.size.y;

		// Black background
		nvgFillColor(vg, nvgRGB(0, 0, 0));
		nvgBeginPath(vg);
		nvgRect(vg, rect.pos.x, 0.f, W, H);
		nvgFill(vg);

		int digit = value % 10;
		const bool* on = SEGMENT_MAP[digit];

		float x = rect.pos.x + padding;
		float y = 0.f + padding;
		float w = W - 2.f * padding;
		float h = H - 2.f * padding;
		float segH = (h - 3.f * thickness) / 2.f;
		float thick = thickness;

		nvgFillColor(vg, color);
		for (int s = 0; s < 7; s++) {
			if (!on[s]) continue;
			nvgBeginPath(vg);
			drawSSSegment(vg, s, x, y, w, segH, thick, gapX, gapY);
			nvgFill(vg);
		}
}

struct ISSDisplay: LedDisplay{
	float padding;
	float thickness;
	float gapX;
	float gapY;
	float gap2X;
	float gap2Y;
	math::Rect rect;
	NVGcolor color;
	int value;
};

template <class T>
struct SSDisplay : ISSDisplay {
	T* module;

	SSDisplay() {
		padding = 10.f;
		thickness = 5.f;
		gapX = 7.f;
		gapY = 3.f;
		color = SCHEME_GREEN;
		value = 0;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if(layer != 1 || !module) return;
		drawSSLayer(args.vg, rect, padding, thickness, gapX, gapY, color, abs(module->ssValues[value]));
	}
};

template <class T>
struct TwoSSDisplay : ISSDisplay {
	T* module;
	TwoSSDisplay() {
		padding = 5.f;
		thickness = 2.f;
		gapX = 3.5f;
		gapY = 1.5f;
		color = SCHEME_GREEN;
		value = 0;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if(layer != 1 || !module) return;

		math::Rect rect1 = math::Rect(Vec(rect.pos.x, rect.pos.y), Vec(rect.size.x / 2.f, rect.size.y));
		math::Rect rect2 = math::Rect(Vec(rect.pos.x + rect.size.x / 2.f, rect.pos.y), Vec(rect.size.x / 2.f, rect.size.y));
		drawSSLayer(args.vg, rect1, padding, thickness, gapX, gapY, color, abs(module->ssValues[value]) / 10);
		drawSSLayer(args.vg, rect2, padding, thickness, gapX, gapY, color, abs(module->ssValues[value]) % 10);
	}
};

template <class T>
struct ThreeSSDisplay : ISSDisplay {
	T* module;
	ThreeSSDisplay() {
		padding = 1.5f;
		thickness = 2.f;
		gapX = 3.5f;
		gapY = 1.5f;
		color = SCHEME_GREEN;
		value = 0;
	}
	
	void drawLayer(const DrawArgs& args, int layer) override {
		if(layer != 1 || !module) return;

		math::Rect rect1 = math::Rect(Vec(rect.pos.x, rect.pos.y), Vec(rect.size.x / 3.f, rect.size.y));
		math::Rect rect2 = math::Rect(Vec(rect.pos.x + rect.size.x / 3.f, rect.pos.y), Vec(rect.size.x / 3.f, rect.size.y));
		math::Rect rect3 = math::Rect(Vec(rect.pos.x + rect.size.x / 3.f * 2.f, rect.pos.y), Vec(rect.size.x / 3.f, rect.size.y));
		drawSSLayer(args.vg, rect1, padding, thickness, gapX, gapY, color, abs(module->ssValues[value]) / 100);
		drawSSLayer(args.vg, rect2, padding, thickness, gapX, gapY, color, abs(module->ssValues[value]) % 100 / 10);
		drawSSLayer(args.vg, rect3, padding, thickness, gapX, gapY, color, abs(module->ssValues[value]) % 10);
	}
};



struct BlankPanel : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		VALUE1_CV,
		VALUE2_CV,
		VALUE3_CV,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
	enum SSValues {
		SS_VALUE1,
		SS_VALUE2,
		SS_VALUE3,
		SS_VALUES_LEN
	};
	int ssValues[SS_VALUES_LEN] = {0, 0, 0};

	BlankPanel() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(VALUE1_CV, "Value 1 CV");
		configInput(VALUE2_CV, "Value 2 CV");
		configInput(VALUE3_CV, "Value 3 CV");
	}

	void process(const ProcessArgs& args) override {
		float value1 = inputs[VALUE1_CV].getVoltage() ;
		float value2 = inputs[VALUE2_CV].getVoltage();
		float value3 = inputs[VALUE3_CV].getVoltage();

		ssValues[SS_VALUE1] = floor(value1);
		ssValues[SS_VALUE2] = floor(value2 * 10.f);
		ssValues[SS_VALUE3] = floor(value3 * 100.f);
	}
};


struct BlankPanelWidget : ModuleWidget {
	BlankPanelWidget(BlankPanel* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BlankPanel.svg"), asset::plugin(pluginInstance, "res/BlankPanel_dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH + RACK_GRID_WIDTH / 2, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH + RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10, 50)), module, BlankPanel::VALUE1_CV));

		SSDisplay<BlankPanel>* ssDisplay = createWidget<SSDisplay<BlankPanel>>(mm2px(Vec(0.004, 13.04)));
		ssDisplay->box.size = mm2px(Vec(20, 29.224));
		ssDisplay->rect = math::Rect(Vec(0.004, 13.04), mm2px(Vec(20, 29.224)));
		ssDisplay->color = SCHEME_CYAN;
		ssDisplay->value = module->SS_VALUE1;
		ssDisplay->module = module;
		addChild(ssDisplay);

		TwoSSDisplay<BlankPanel>* twoDisplay = createWidget<TwoSSDisplay<BlankPanel>>(mm2px(Vec(0.004, 60)));
		twoDisplay->box.size = mm2px(Vec(20, 16));
		twoDisplay->rect = math::Rect(mm2px(Vec(0.004, 80)), mm2px(Vec(20, 16)));
		twoDisplay->color = SCHEME_YELLOW;
		twoDisplay->value = module->SS_VALUE2;
		twoDisplay->module = module;
		addChild(twoDisplay);

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10, 85)), module, BlankPanel::VALUE2_CV));

		ThreeSSDisplay<BlankPanel>* threeDisplay = createWidget<ThreeSSDisplay<BlankPanel>>(mm2px(Vec(0.004, 100)));
		threeDisplay->box.size = mm2px(Vec(20, 12));
		threeDisplay->rect = math::Rect(mm2px(Vec(0.004, 100)), mm2px(Vec(20, 12)));
		threeDisplay->color = SCHEME_RED;
		threeDisplay->value = module->SS_VALUE3;
		threeDisplay->module = module;
		addChild(threeDisplay);

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10, 120)), module, BlankPanel::VALUE3_CV));
	}
};


Model* modelBlankPanel = createModel<BlankPanel, BlankPanelWidget>("BlankPanel");