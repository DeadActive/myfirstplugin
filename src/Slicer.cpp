#include <cassert>
#include <cmath>

#include "plugin.hpp"

struct Slice : Module {
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
	constexpr static float DECLICK_TIME = 0.0025f;

	enum GateSyncMode {
		GATE_SYNC_MODE_OFF = 0,
		GATE_SYNC_MODE_ON = 1,
	};

	GateSyncMode gateSyncMode = GATE_SYNC_MODE_OFF;

	constexpr static size_t BUFFER_SIZE = (48000 * 60) / MIN_BPM * 8;

	dsp::RingBuffer<float, BUFFER_SIZE> historyBuffer;
	dsp::SchmittTrigger clkTrigger;
	dsp::ClockDivider clockDivider;

	dsp::PulseGenerator clockPulse;

	bool last_gate = false;
	uint32_t writePos = 0;
	uint32_t readPos = 0;
	uint32_t declickPos = 1;
	uint32_t declickLength = 1;

	uint32_t baseLength = 0;
	uint32_t loopStart = 0;
	uint32_t loopEnd = 0;
	uint32_t loopLength = 0;

	uint32_t samplesSinceLastClk = 0;
	uint32_t lastIntervalLength = bpmToSamples(120, 48000);

	float lastSize = 0.f;
	float lastOut = 0.f;
	float lastRawPlay = 0.f;

	enum DeclickReason : uint8_t {
		DECLICK_NONE = 0,
		DECLICK_GATE_ON,
		DECLICK_GATE_OFF,
		DECLICK_LOOP_WRAP,
	};
	DeclickReason declickReason = DECLICK_NONE;
	float declickTail = 0.f;

	enum State {
		IDLE,
		WAIT_FOR_CLOCK,
		REPEATING,
	};
	State state = IDLE;

	Slice() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(TRIG__PARAM, "Trigger");
		configSwitch(TRIPLE__PARAM, 0.f, 1.f, 0.f, "Triplet", {"Off", "On"});

		configParam<SizeParam>(SIZE__PARAM, 0.f, (float)(numSizeOptions - 1), 6.f, "Size");
		paramQuantities[SIZE__PARAM]->snapEnabled = true;

		configInput(IN__INPUT, "Audio In");
		configInput(CLK__INPUT, "Clock");
		configInput(TRIG__INPUT, "Trigger CV");
		configInput(SIZE__INPUT, "Size CV (-10V to +10V)");
		configOutput(OUT__OUTPUT, "Audio Out");
		configLight(TRIG__LIGHT, "Trigger");

		clockDivider.setDivision(lastIntervalLength);
	}

	float getLoopLength() {
		baseLength = lastIntervalLength;
		float size = 0.f;
		if(inputs[SIZE__INPUT].isConnected()) {
			float sizeCV = (clamp(inputs[SIZE__INPUT].getVoltage(), -10.f, 10.f) + 10.f) / 20.f;
			size = sizeCV * (numSizeOptions - 1);
		} else {
			size = params[SIZE__PARAM].getValue();
		}
		loopLength = baseLength * getSizeBarFraction(size, params[TRIPLE__PARAM].getValue() > 0.f) * 4.f;
		return clamp(loopLength, 1, BUFFER_SIZE);
	}

	void startDeclick(float sampleRate, DeclickReason reason, float tailSample = 0.f) {
		declickReason = reason;
		if(reason == DECLICK_GATE_OFF || reason == DECLICK_LOOP_WRAP)
			declickTail = tailSample;
		uint32_t fadeSamples = (uint32_t) (sampleRate * DECLICK_TIME + 0.5f);
		declickLength = fadeSamples > 0 ? fadeSamples : 1;
		declickPos = 0;
	}

	float applyDeclick(float live, float rawPlay) {
		if(declickReason == DECLICK_NONE || declickPos >= declickLength) {
			declickReason = DECLICK_NONE;
			return rawPlay;
		}

		float t = (float) (declickPos + 1) / (float) declickLength;
		float c = std::cos(0.5f * static_cast<float>(M_PI) * t);
		float s = std::sin(0.5f * static_cast<float>(M_PI) * t);
		declickPos++;
		float y = rawPlay;
		switch(declickReason) {
			case DECLICK_GATE_ON:
				y = live * c + rawPlay * s;
				break;
			case DECLICK_GATE_OFF:
				y = declickTail * c + live * s;
				break;
			case DECLICK_LOOP_WRAP:
				y = declickTail * c + rawPlay * s;
				break;
			default:
				break;
		}
		if(declickPos >= declickLength)
			declickReason = DECLICK_NONE;
		return y;
	}

	void setLight(float colors[3], bool smooth = false, float time = 0.f ) {
		if(smooth) {
			lights[TRIG__LIGHT].setBrightnessSmooth(colors[0], time, 60.f);
			lights[TRIG__LIGHT + 1].setBrightnessSmooth(colors[1], time, 60.f);
			lights[TRIG__LIGHT + 2].setBrightnessSmooth(colors[2], time, 60.f);
		} else {
			lights[TRIG__LIGHT].setBrightness(colors[0]);
			lights[TRIG__LIGHT + 1].setBrightness(colors[1]);
			lights[TRIG__LIGHT + 2].setBrightness(colors[2]);
		}
	}

	void process(const ProcessArgs& args) override {
		// WRITE TO BUFFER
		if (inputs[IN__INPUT].isConnected()) {
			float in = inputs[IN__INPUT].getVoltageSum();
			historyBuffer.push(in);
		}

		bool clk = clockDivider.process();

		samplesSinceLastClk++;
		if(inputs[CLK__INPUT].isConnected()) {
			bool clkIn = clkTrigger.process(inputs[CLK__INPUT].getVoltage());
			clk = clkIn;
			if(clkIn) {
				lastIntervalLength = samplesSinceLastClk;
				clockDivider.reset();
				clockDivider.setDivision(lastIntervalLength);
				samplesSinceLastClk = 0;
			}
		}

		// GATE
		bool push = params[TRIG__PARAM].getValue() > .1f;
		bool trig = inputs[TRIG__INPUT].isConnected() && inputs[TRIG__INPUT].getVoltage() > 0.1f;
		bool gate = push || trig;

		bool gateRising = gate && !last_gate;
		bool gateFalling = !gate && last_gate;

		last_gate = gate;

		if(gateRising) {
			state = REPEATING;
			startDeclick(args.sampleRate, DECLICK_GATE_ON);

			loopStart = writePos;
			readPos = loopStart;
		}
		if(gateFalling) {
			state = IDLE;
			startDeclick(args.sampleRate, DECLICK_GATE_OFF, lastRawPlay);

			loopStart = 0;
			loopEnd = 0;
			loopLength = 0;
			readPos = 0;
		}

		loopLength = getLoopLength();
		loopEnd = (loopStart + loopLength) % BUFFER_SIZE;

		float live = inputs[IN__INPUT].getVoltageSum();
		float rawPlay = live;
		if(state == REPEATING) {
			rawPlay = historyBuffer.data[readPos];
			readPos = (readPos + 1) % BUFFER_SIZE;

			if(readPos >= loopEnd) {
				uint32_t lastIdx = (loopEnd + BUFFER_SIZE - 1) % BUFFER_SIZE;
				float loopTailSample = historyBuffer.data[lastIdx];
				readPos = loopStart;
				startDeclick(args.sampleRate, DECLICK_LOOP_WRAP, loopTailSample);
			}
			lastRawPlay = rawPlay;
		}

		// AUDIO OUTPUT
		float out = applyDeclick(live, rawPlay);
		outputs[OUT__OUTPUT].setVoltage(out);
		lastOut = out;

		// SET LIGHT
		if(clk) {clockPulse.trigger(0.1f);}
		if(clockPulse.process(args.sampleTime)) {
			float colors[3] = {0.f, 1.f, 0.f};
			setLight(colors, true, args.sampleTime);
		} else if (state == REPEATING) {
			float colors[3] = {1.f, 0.f, 1.f};
			setLight(colors);
		} else {
			float colors[3] = {0.f, 0.f, 0.f};
			setLight(colors);
		}

		if(inputs[IN__INPUT].isConnected()) {
			writePos = (writePos + 1) % BUFFER_SIZE;
		}

	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "gateSyncMode", json_integer(gateSyncMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* gateSyncModeJ = json_object_get(rootJ, "gateSyncMode");
		if (gateSyncModeJ) {
			int v = json_integer_value(gateSyncModeJ);
			if (v == GATE_SYNC_MODE_OFF || v == GATE_SYNC_MODE_ON)
				gateSyncMode = (GateSyncMode)v;
		}
	}
};


struct SliceWidget : ModuleWidget {
	SliceWidget(Slice* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Slicer.svg")));

		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(4.97, 60.133)), module, Slice::TRIG__PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(4.97, 72.112)), module, Slice::TRIPLE__PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(4.97, 87.034)), module, Slice::SIZE__PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 15.596)), module, Slice::IN__INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 30.571)), module, Slice::CLK__INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 43.521)), module, Slice::TRIG__INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(4.97, 101.21)), module, Slice::SIZE__INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(4.97, 116.019)), module, Slice::OUT__OUTPUT));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(4.97, 52.177)), module, Slice::TRIG__LIGHT));
	}

	struct GateSyncModeOffOption : MenuItem {
		Slice* module;
		void onAction(const event::Action& e) override {
			module->gateSyncMode = Slice::GATE_SYNC_MODE_OFF;
		}
	};

	struct GateSyncModeOnOption : MenuItem {
		Slice* module;
		void onAction(const event::Action& e) override {
			module->gateSyncMode = Slice::GATE_SYNC_MODE_ON;
		}
	};

	struct GateSyncModeMenu : MenuItem {
		Slice* module;
		Menu* createChildMenu() override {
			Menu* menu = new Menu;
			GateSyncModeOffOption* gateSyncModeOff = createMenuItem<GateSyncModeOffOption>("Off", CHECKMARK(module->gateSyncMode == Slice::GATE_SYNC_MODE_OFF));
			gateSyncModeOff->module = module;
			menu->addChild(gateSyncModeOff);
			GateSyncModeOnOption* gateSyncModeOn = createMenuItem<GateSyncModeOnOption>("On", CHECKMARK(module->gateSyncMode == Slice::GATE_SYNC_MODE_ON));
			gateSyncModeOn->module = module;
			menu->addChild(gateSyncModeOn);
			return menu;
		}
	};

	void appendContextMenu(Menu* menu) override {
		Slice* module = dynamic_cast<Slice*>(this->module);
		assert(module);

		menu->addChild(new MenuEntry);

		GateSyncModeMenu* gateSyncModeMenu = createMenuItem<GateSyncModeMenu>("Sync to clock", RIGHT_ARROW);
		gateSyncModeMenu->module = module;
		menu->addChild(gateSyncModeMenu);
	}
};


Model* modelSlice = createModel<Slice, SliceWidget>("Slice");