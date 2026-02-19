#include "plugin.hpp"

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

template <class BlankPanel>
struct SSDisplay : LedDisplay {
	BlankPanel* module;

	float padding = 2.f;
	float thickness = 2.5f;
	float gapX = 7.f;
	float gapY = 3.f;
	NVGcolor color = SCHEME_GREEN;

	void drawSegment(NVGcontext* vg, int seg, float x, float y, float w, float segH, float thick) {
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

	void drawLayer(const DrawArgs& args, int layer) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x;
		float H = box.size.y;

		// Black background
		nvgFillColor(vg, nvgRGB(0, 0, 0));
		nvgBeginPath(vg);
		nvgRect(vg, 0.f, 0.f, W, H);
		nvgFill(vg);

		if (!module) {
			LedDisplay::drawLayer(args, layer);
			return;
		}

		int digit = module->value % 10;
		const bool* on = SEGMENT_MAP[digit];

		float x = padding;
		float y = padding;
		float w = W - 2.f * padding;
		float h = H - 2.f * padding;
		float segH = (h - 3.f * thickness) / 2.f;
		float thick = thickness;

		nvgFillColor(vg, color);
		for (int s = 0; s < 7; s++) {
			if (!on[s]) continue;
			nvgBeginPath(vg);
			drawSegment(vg, s, x, y, w, segH, thick);
			nvgFill(vg);
		}

		LedDisplay::drawLayer(args, layer);
	}
};


struct BlankPanel : Module {
	enum ParamId {
		GATE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		VALUE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::BooleanTrigger gateBoolean;

	int value = 0;

	BlankPanel() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(GATE_PARAM, "Gate");
		configOutput(VALUE_OUTPUT, "Value");
	}

	void process(const ProcessArgs& args) override {
		bool gate = params[GATE_PARAM].getValue() > 0.f;
		if (gateBoolean.process(gate)) {
			this->value = (this->value + 1) % 10;
		}

		outputs[VALUE_OUTPUT].setVoltage(this->value * 1.f);
	}
};


struct BlankPanelWidget : ModuleWidget {
	BlankPanelWidget(BlankPanel* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BlankPanel.svg"), asset::plugin(pluginInstance, "res/BlankPanel_dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH + RACK_GRID_WIDTH / 2, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH + RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<MomentarySwitch<BefacoPush>>(mm2px(Vec(10, 60)), module, BlankPanel::GATE_PARAM));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(10, 70)), module, BlankPanel::VALUE_OUTPUT));

		SSDisplay<BlankPanel>* display = createWidget<SSDisplay<BlankPanel>>(mm2px(Vec(0.004, 13.04)));
		display->box.size = mm2px(Vec(20, 29.224));
		display->color = SCHEME_YELLOW;
		display->padding = 10.f;
		display->thickness = 5.f;
		display->module = module;
		addChild(display);
	}
};


Model* modelBlankPanel = createModel<BlankPanel, BlankPanelWidget>("BlankPanel");