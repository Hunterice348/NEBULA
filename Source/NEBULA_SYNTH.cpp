#include <JuceHeader.h>

namespace Nebula {
    struct StateManager {
        static void save(juce::AudioProcessorValueTreeState& apvts, juce::MemoryBlock& dest, int w, int h) {
            auto root = apvts.copyState();
            auto ui = juce::ValueTree("UI"); ui.setProperty("w", w, nullptr); ui.setProperty("h", h, nullptr);
            if (auto ex = root.getChildWithName("UI"); ex.isValid()) root.removeChild(ex, nullptr);
            root.appendChild(ui, nullptr);
            if (auto xml = root.createXml()) dest.replaceAll(xml->createDocumentToString("", true).toRawUTF8());
        }
        static void restore(juce::AudioProcessorValueTreeState& apvts, const void* data, int size, int& w, int& h) {
            if (size > 0) {
                if (auto xml = juce::XmlDocument::parse(juce::String(static_cast<const char*>(data), size))) {
                    if (xml->hasTagName(apvts.state.getType())) {
                        auto state = juce::ValueTree::fromXml(*xml);
                        if (auto ui = state.getChildWithName("UI"); ui.isValid()) { w = (int)ui.getProperty("w", 800); h = (int)ui.getProperty("h", 600); state.removeChild(ui, nullptr); }
                        apvts.replaceState(state);
                    }
                }
            }
        }
    };
}

class NebulaSynthAudioProcessor : public juce::AudioProcessor {
public:
    NebulaSynthAudioProcessor() : AudioProcessor(juce::AudioProcessor::BusesProperties().withOutput("Out", juce::AudioChannelSet::stereo(), true)),
        apvts(*this, nullptr, "Params", {
            std::make_unique<juce::AudioParameterFloat>("CUTOFF", "Cutoff", 20.0f, 20000.0f, 5000.0f, juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f)),
            std::make_unique<juce::AudioParameterFloat>("RES", "Resonance", 0.0f, 0.95f, 0.2f),
            std::make_unique<juce::AudioParameterFloat>("VOL", "Volume", -48.0f, 0.0f, -6.0f)
        }) {}
    ~NebulaSynthAudioProcessor() override {}
    void prepareToPlay(double sr, int) override { sampleRate = sr; for(auto& v: voices) { v.active = false; v.filter.prepare({sr, 512, 2}); v.filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass); } }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals noDenormals; buf.clear();
        for (const auto meta : midi) {
            auto msg = meta.getMessage();
            if (msg.isNoteOn()) { for(auto& v: voices) if(!v.active) { v.active=true; v.note=msg.getNoteNumber(); v.phase=0; v.env=1.0f; v.filter.reset(); break; } }
            else if (msg.isNoteOff()) { for(auto& v: voices) if(v.active && v.note==msg.getNoteNumber()) v.env = 0.0f; }
        }
        float cut = *apvts.getRawParameterValue("CUTOFF"), res = *apvts.getRawParameterValue("RES"), vol = juce::Decibels::decibelsToGain(*apvts.getRawParameterValue("VOL"));
        for (int i = 0; i < buf.getNumSamples(); ++i) {
            float outL = 0, outR = 0;
            for (auto& v : voices) {
                if (!v.active) continue;
                float freq = 440.0f * std::pow(2.0, (v.note - 69.0) / 12.0);
                float osc = 2.0f * (v.phase - std::floor(v.phase + 0.5f));
                v.phase += freq / sampleRate; if (v.phase > 1.0f) v.phase -= 1.0f;
                v.filter.setCutoffFrequency(cut); v.filter.setResonance(res);
                juce::dsp::AudioBlock<float> block(&osc, 1, 1); v.filter.process(juce::dsp::ProcessContextReplacing<float>(block));
                v.env *= 0.99f; if (v.env < 0.001f) v.active = false;
                float s = osc * v.env; if (!std::isfinite(s)) s = 0.0f;
                outL += s; outR += s;
            }
            buf.setSample(0, i, outL * vol); buf.setSample(1, i, outR * vol);
        }
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new Editor(*this); }
    const juce::String getName() const override { return "NEBULA SYNTH"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override { Nebula::StateManager::save(apvts, dest, uiW, uiH); }
    void setStateInformation(const void* data, int size) override { Nebula::StateManager::restore(apvts, data, size, uiW, uiH); }

    juce::AudioProcessorValueTreeState apvts;
    int uiW = 800, uiH = 600;
private:
    struct Voice { bool active = false; int note = 0; double phase = 0; float env = 0.0f; juce::dsp::StateVariableTPTFilter<float> filter; };
    Voice voices[16]; double sampleRate = 44100.0;
    
    class Editor : public juce::AudioProcessorEditor {
    public:
        Editor(NebulaSynthAudioProcessor& p) : AudioProcessorEditor(&p), proc(p) {
            setResizable(true, true); setResizeLimits(650, 500, 1600, 1100); setSize(proc.uiW, proc.uiH);
            auto setup = [](juce::Slider& s) { s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20); s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00FF88)); s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white); };
            setup(cut); setup(res); setup(vol); addAndMakeVisible(cut); addAndMakeVisible(res); addAndMakeVisible(vol);
            att1.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, "CUTOFF", cut));
            att2.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, "RES", res));
            att3.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, "VOL", vol));
        }
        void paint(juce::Graphics& g) override { 
            g.fillAll(juce::Colour(0xFF121212)); 
            auto a = getLocalBounds().reduced(20); 
            g.setDropShadow(juce::DropShadow(juce::Colours::black.withAlpha(0.8f), 15, {})); 
            g.fillRoundedRectangle(a.toFloat(), 12.0f); 
            g.setColour(juce::Colours::white); 
            g.drawText("NEBULA SYNTH", a.removeFromTop(50), juce::Justification::centred); 
        }
        void resized() override { 
            auto a = getLocalBounds().reduced(30, 80); 
            int sz = juce::jmin(100, a.getWidth()/3 - 20); 
            cut.setBounds(a.removeFromLeft(sz+20)); 
            res.setBounds(a.removeFromLeft(sz+20)); 
            vol.setBounds(a.removeFromLeft(sz+20)); 
            proc.uiW = getWidth(); proc.uiH = getHeight(); 
        }
    private:
        NebulaSynthAudioProcessor& proc; juce::Slider cut, res, vol; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att1, att2, att3;
    };
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NebulaSynthAudioProcessor(); }