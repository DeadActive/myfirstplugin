#include "plugin.hpp"

struct Slicer : Module {
	enum ParamId {
		TRIG__PARAM,
		TRIPLE__PARAM,
		SIZE__PARAM,
		PARAMS_LEN
	};
	enum InputId {
		IN__INPUT,
		CLK__INPUT,
		TRIG__INPUT,
		SIZE__INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT__OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(TRIG__LIGHT, 3),
		LIGHTS_LEN
	};

	constexpr static int MIN_BPM = 10;

	constexpr static size_t HISTORY_SIZE = 1 << 21;

	dsp::BooleanTrigger trigBoolean;
	dsp::BooleanTrigger clkTrigger;

	Slicer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(TRIG__PARAM, "Trigger");
		configSwitch(TRIPLE__PARAM, 0.f, 1.f, 0.f, "Triplet", {"Off", "On"});

		configParam<SizeParam>(SIZE__PARAM, 0.f, (float)(numSizeOptions - 1), 1.f, "Size");
		paramQuantities[SIZE__PARAM]->snapEnabled = true;

		configInput(IN__INPUT, "Audio In");
		configInput(CLK__INPUT, "Clock");
		configInput(TRIG__INPUT, "Trigger CV");
		configInput(SIZE__INPUT, "Size CV");
		configOutput(OUT__OUTPUT, "Audio Out");
		configLight(TRIG__LIGHT, "Trigger");
	}

	void process(const ProcessArgs& args) override {
		bool push = params[TRIG__PARAM].getValue() > 0.f;
		bool trig = trigBoolean.process(inputs[TRIG__INPUT].getVoltage() > 0.f);
		bool gate = push || trig;

		bool clk = clkTrigger.process(inputs[CLK__INPUT].getVoltage() > 0.f);

		// [[maybe_unused]] const float sizeBars = getSizeBarFraction(params[SIZE__PARAM].getValue());

		float colors[3] = {0.f, 0.f, 0.f};

		if(gate) {
			colors[0] = 1.f;
			colors[2] = 1.f;
		} else if (clk) {
			colors[1] = 1.f;
		}

		lights[TRIG__LIGHT].setBrightnessSmooth(colors[0], args.sampleTime);
		lights[TRIG__LIGHT + 1].setBrightnessSmooth(colors[1], args.sampleTime);
		lights[TRIG__LIGHT + 2].setBrightnessSmooth(colors[2], args.sampleTime);
	}
};


struct SlicerWidget : ModuleWidget {
	SlicerWidget(Slicer* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Slicer.svg")));

		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(4.97, 60.133)), module, Slicer::TRIG__PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(4.97, 72.112)), module, Slicer::TRIPLE__PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(4.97, 87.034)), module, Slicer::SIZE__PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 15.596)), module, Slicer::IN__INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 30.571)), module, Slicer::CLK__INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 43.521)), module, Slicer::TRIG__INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 101.21)), module, Slicer::SIZE__INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(4.97, 116.019)), module, Slicer::OUT__OUTPUT));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(4.97, 52.177)), module, Slicer::TRIG__LIGHT));
	}
};


Model* modelSlicer = createModel<Slicer, SlicerWidget>("Slicer");