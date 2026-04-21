#include <cassert>

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

	enum PPQMode {
		PPQ_MODE_1 = 1,
		PPQ_MODE_2 = 2,
		PPQ_MODE_4 = 4,
		PPQ_MODE_8 = 8,
		PPQ_MODE_12 = 12,
		PPQ_MODE_16 = 16,
		PPQ_MODE_24 = 24,
	};

	PPQMode ppqMode = PPQ_MODE_1;

	float getPPQLength() const {
		return ppqMode * 4.f;
	}

	constexpr static size_t BUFFER_SIZE = (48000 * 60) / MIN_BPM * 8;

	dsp::DoubleRingBuffer<float, BUFFER_SIZE> historyBuffer;
	dsp::BooleanTrigger clkTrigger;

	bool last_gate = false;

	uint32_t writePos = 0;
	uint32_t readPos = 0;

	uint32_t baseLength = 0;
	uint32_t loopStart = 0;
	uint32_t loopEnd = 0;
	uint32_t loopLength = 0;

	uint32_t samplesSinceLastClk = 0;
	uint32_t lastIntervalLength = 0;

	enum State {
		IDLE,
		WAIT_FOR_CLOCK,
		REPEATING,
	};
	State state = IDLE;

	Slicer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(TRIG__PARAM, "Trigger");
		configSwitch(TRIPLE__PARAM, 0.f, 1.f, 0.f, "Triplet", {"Off", "On"});

		configParam<SizeParam>(SIZE__PARAM, 0.f, (float)(numSizeOptions - 1), 1.f, "Size");
		paramQuantities[SIZE__PARAM]->snapEnabled = true;

		configInput(IN__INPUT, "Audio In");
		configInput(CLK__INPUT, "Clock");
		configInput(TRIG__INPUT, "Trigger CV");
		configInput(SIZE__INPUT, "Size CV (-5V to +5V)");
		configOutput(OUT__OUTPUT, "Audio Out");
		configLight(TRIG__LIGHT, "Trigger");
	}

	float getLoopLength() {
		baseLength = lastIntervalLength;
		bool isSizeCVConnected = inputs[SIZE__INPUT].isConnected();
		float sizeCV = isSizeCVConnected ? clamp(inputs[SIZE__INPUT].getVoltage(), -10.f, 10.f) * 0.1f + 0.5f : 0.f;
		float size = isSizeCVConnected ? sizeCV * numSizeOptions : params[SIZE__PARAM].getValue();
		loopLength = baseLength * getSizeBarFraction(size) * getPPQLength();
				if(params[TRIPLE__PARAM].getValue() > 0.f) {
					loopLength *= (2.f / 3.f);
				}
				return clamp(loopLength, 1, BUFFER_SIZE);
	}

	void process(const ProcessArgs& args) override {
		// WRITE TO BUFFER
		if (inputs[IN__INPUT].isConnected()) {
			float in = inputs[IN__INPUT].getVoltageSum();
			if (historyBuffer.full())
				historyBuffer.shift();
			historyBuffer.push(in);
			writePos = (writePos + 1) % BUFFER_SIZE;
		}

		// CLOCK
		bool clk = clkTrigger.process(inputs[CLK__INPUT].getVoltage() > 0.f);
		samplesSinceLastClk++;
		if(clk) {
			lastIntervalLength = samplesSinceLastClk;
			samplesSinceLastClk = 0;
		}

		// GATE
		bool push = params[TRIG__PARAM].getValue() > 0.f;
		bool trig = inputs[TRIG__INPUT].isConnected() && inputs[TRIG__INPUT].getVoltage() > 0.f;
		bool gate = push || trig;

		bool gateRising = gate && !last_gate;
		bool gateFalling = !gate && last_gate;

		if(gateRising) {
			state = WAIT_FOR_CLOCK;
		}
		if(gateFalling) {
			state = IDLE;
		}

		if(state == WAIT_FOR_CLOCK && clk) {
			loopLength = getLoopLength();

			loopStart = writePos;
			loopEnd = (loopStart + loopLength) % BUFFER_SIZE;
			
			readPos = loopStart;
			state = REPEATING;
		}

		float out = 0.f;

		if(state == REPEATING) {
			out = historyBuffer.data[readPos];
			readPos++;

			if(readPos >= BUFFER_SIZE) {
				readPos = 0;
			}

			if(readPos == loopEnd) {
				loopLength = getLoopLength();
				loopEnd = (loopStart + loopLength) % BUFFER_SIZE;

				readPos = loopStart;
			}
		} else {
			if(inputs[IN__INPUT].isConnected()) {
				out = inputs[IN__INPUT].getVoltageSum();
			}
		}

		last_gate = gate;

		// AUDIO OUTPUT
		outputs[OUT__OUTPUT].setVoltage(out);

		// SET LIGHT
		float colors[3] = {0.f, 0.f, 0.f};

		if(state == WAIT_FOR_CLOCK) {
			colors[2] = 1.f;
		} else if(state == REPEATING) {
			colors[0] = 1.f;
			colors[2] = 1.f;
		} else if (clk) {
			colors[1] = 1.f;
		}

		lights[TRIG__LIGHT].setBrightnessSmooth(colors[0], args.sampleTime);
		lights[TRIG__LIGHT + 1].setBrightnessSmooth(colors[1], args.sampleTime);
		lights[TRIG__LIGHT + 2].setBrightnessSmooth(colors[2], args.sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "ppqMode", json_integer(ppqMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* ppqJ = json_object_get(rootJ, "ppqMode");
		if (ppqJ) {
			int v = json_integer_value(ppqJ);
			if (v == PPQ_MODE_1 || v == PPQ_MODE_4)
				ppqMode = (PPQMode)v;
		}
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

	struct PPQ1Option : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->ppqMode = Slicer::PPQ_MODE_1;
		}
	};

	struct PPQ2Option : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->ppqMode = Slicer::PPQ_MODE_2;
		}
	};

	struct PPQ4Option : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->ppqMode = Slicer::PPQ_MODE_4;
		}
	};

	struct PPQ8Option : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->ppqMode = Slicer::PPQ_MODE_8;
		}
	};

	struct PPQ16Option : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->ppqMode = Slicer::PPQ_MODE_16;
		}
	};

	struct PPQ24Option : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->ppqMode = Slicer::PPQ_MODE_24;
		}
	};

	struct PPQMenu : MenuItem {
		Slicer* module;
		Menu* createChildMenu() override {
			Menu* menu = new Menu;
			PPQ1Option* ppq1 = createMenuItem<PPQ1Option>("1", CHECKMARK(module->ppqMode == Slicer::PPQ_MODE_1));
			ppq1->module = module;
			menu->addChild(ppq1);
			PPQ2Option* ppq2 = createMenuItem<PPQ2Option>("2", CHECKMARK(module->ppqMode == Slicer::PPQ_MODE_2));
			ppq2->module = module;
			menu->addChild(ppq2);
			PPQ4Option* ppq4 = createMenuItem<PPQ4Option>("4", CHECKMARK(module->ppqMode == Slicer::PPQ_MODE_4));
			ppq4->module = module;
			menu->addChild(ppq4);
			PPQ8Option* ppq8 = createMenuItem<PPQ8Option>("8", CHECKMARK(module->ppqMode == Slicer::PPQ_MODE_8));
			ppq8->module = module;
			menu->addChild(ppq8);
			PPQ16Option* ppq16 = createMenuItem<PPQ16Option>("16", CHECKMARK(module->ppqMode == Slicer::PPQ_MODE_16));
			ppq16->module = module;
			menu->addChild(ppq16);
			PPQ24Option* ppq24 = createMenuItem<PPQ24Option>("24", CHECKMARK(module->ppqMode == Slicer::PPQ_MODE_24));
			ppq24->module = module;
			menu->addChild(ppq24);
			return menu;
		}
	};

	void appendContextMenu(Menu* menu) override {
		Slicer* module = dynamic_cast<Slicer*>(this->module);
		assert(module);

		menu->addChild(new MenuEntry);
		PPQMenu* ppqMenu = createMenuItem<PPQMenu>("PPQN", RIGHT_ARROW);
		ppqMenu->module = module;
		menu->addChild(ppqMenu);
	}
};


Model* modelSlicer = createModel<Slicer, SlicerWidget>("Slicer");