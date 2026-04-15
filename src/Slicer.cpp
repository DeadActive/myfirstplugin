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

	enum SampleRateMode {
		SAMPLE_RATE_HIFI,
		SAMPLE_RATE_LOFI,
	};

	SampleRateMode sampleRateMode = SAMPLE_RATE_HIFI;

	float getSampleRateHz() const {
		return sampleRateMode == SAMPLE_RATE_LOFI ? 22050.f : 44100.f;
	}

	constexpr static size_t BUFFER_SIZE = (44100 * 60) / MIN_BPM;

	dsp::DoubleRingBuffer<float, BUFFER_SIZE> historyBuffer;
	dsp::BooleanTrigger clkTrigger;

	bool last_gate = false;
	bool last_trig = false;

	uint32_t writePos = 0;
	uint32_t readPos = 0;

	/** Ring index of the sample written on gate rise (inclusive start of the slice). */
	uint32_t recordStart = 0;
	/** Samples elapsed since gate rise (including gate sample), incremented while waiting for clock. */
	uint32_t samplesSinceGate = 0;

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
		configInput(SIZE__INPUT, "Size CV");
		configOutput(OUT__OUTPUT, "Audio Out");
		configLight(TRIG__LIGHT, "Trigger");
	}

	void computeLoopLengthFromParams() {
		uint32_t baseLen = lastIntervalLength;
		if (baseLen == 0) {
			baseLen = samplesSinceLastClk;
			if (baseLen == 0)
				baseLen = 1;
		}
		baseLength = baseLen;

		bool isSizeCVConnected = inputs[SIZE__INPUT].isConnected();
		float sizeCV = isSizeCVConnected ? clamp(inputs[SIZE__INPUT].getVoltage(), -10.f, 10.f) * 0.1f + 0.5f : 0.f;
		float size = isSizeCVConnected ? sizeCV * numSizeOptions : params[SIZE__PARAM].getValue();
		loopLength = baseLength * getSizeBarFraction(size) * 4.f;

		if (params[TRIPLE__PARAM].getValue() > 0.f) {
			loopLength *= (2.f / 3.f);
		}
		loopLength = clamp(loopLength, 1, BUFFER_SIZE);
	}

	/** Forward from gate: length min(param, samples recorded until clock). */
	void beginRepeatFromRecordStart(uint32_t samplesRecorded) {
		computeLoopLengthFromParams();

		uint32_t effectiveLen = loopLength;
		if (effectiveLen > samplesRecorded)
			effectiveLen = samplesRecorded;
		if (effectiveLen < 1)
			effectiveLen = 1;

		loopStart = recordStart;
		loopEnd = (loopStart + effectiveLen) % BUFFER_SIZE;

		readPos = loopStart;
	}

	/** Instant CV path: slice ends at current write cursor, length from params (past buffer only). */
	void beginRepeatBackwardFromWritePos() {
		computeLoopLengthFromParams();

		loopEnd = writePos;
		loopStart = (loopEnd + BUFFER_SIZE - loopLength) % BUFFER_SIZE;

		readPos = loopStart;
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

		// GATE — record start on rise; button waits for clock. TRIG input rising skips wait and repeats immediately (backward slice).
		bool push = params[TRIG__PARAM].getValue() > 0.f;
		bool trigIn = inputs[TRIG__INPUT].isConnected();
		bool trigHigh = trigIn && inputs[TRIG__INPUT].getVoltage() > 0.f;
		bool gate = push || trigHigh;

		bool trigRising = trigIn && trigHigh && !last_trig;
		bool gateRising = gate && !last_gate;
		bool gateFalling = !gate && last_gate;

		if (trigRising) {
			recordStart = (writePos + BUFFER_SIZE - 1) % BUFFER_SIZE;
			beginRepeatBackwardFromWritePos();
			state = REPEATING;
		} else if (gateRising) {
			recordStart = (writePos + BUFFER_SIZE - 1) % BUFFER_SIZE;
			samplesSinceGate = 0;
			state = WAIT_FOR_CLOCK;
		}

		if (gateFalling) {
			state = IDLE;
		}

		if (state == WAIT_FOR_CLOCK)
			samplesSinceGate++;

		if (state == WAIT_FOR_CLOCK && clk) {
			beginRepeatFromRecordStart(samplesSinceGate);
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
				readPos = loopStart;
			}
		} else {
			if(inputs[IN__INPUT].isConnected()) {
				out = inputs[IN__INPUT].getVoltageSum();
			}
		}

		last_trig = trigHigh;
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
		json_object_set_new(rootJ, "sampleRateMode", json_integer(sampleRateMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* modeJ = json_object_get(rootJ, "sampleRateMode");
		if (modeJ) {
			int v = json_integer_value(modeJ);
			if (v == SAMPLE_RATE_HIFI || v == SAMPLE_RATE_LOFI)
				sampleRateMode = (SampleRateMode)v;
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

	struct HiFiOption : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->sampleRateMode = Slicer::SAMPLE_RATE_HIFI;
		}
	};

	struct LoFiOption : MenuItem {
		Slicer* module;
		void onAction(const event::Action& e) override {
			module->sampleRateMode = Slicer::SAMPLE_RATE_LOFI;
		}
	};

	struct SampleRateMenu : MenuItem {
		Slicer* module;
		Menu* createChildMenu() override {
			Menu* menu = new Menu;
			HiFiOption* hifi = createMenuItem<HiFiOption>("HiFi (44.1 kHz)", CHECKMARK(module->sampleRateMode == Slicer::SAMPLE_RATE_HIFI));
			hifi->module = module;
			menu->addChild(hifi);
			LoFiOption* lofi = createMenuItem<LoFiOption>("LoFi (22.05 kHz)", CHECKMARK(module->sampleRateMode == Slicer::SAMPLE_RATE_LOFI));
			lofi->module = module;
			menu->addChild(lofi);
			return menu;
		}
	};

	void appendContextMenu(Menu* menu) override {
		Slicer* module = dynamic_cast<Slicer*>(this->module);
		assert(module);
		menu->addChild(new MenuEntry);
		SampleRateMenu* sampleRateMenu = createMenuItem<SampleRateMenu>("Sample rate", RIGHT_ARROW);
		sampleRateMenu->module = module;
		menu->addChild(sampleRateMenu);
	}
};


Model* modelSlicer = createModel<Slicer, SlicerWidget>("Slicer");