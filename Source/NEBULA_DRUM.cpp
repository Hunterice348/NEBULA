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

class NebulaDrumAudioProcessor : public juce::AudioProcessor {
public:
    NebulaDrumAudioProcessor() : AudioProcessor(juce::AudioProcessor::BusesProperties().withOutput("Out", juce::AudioChannelSet::stereo(), true)),
        apvts(*this, nullptr, "Params", {
            std::make_unique<juce::AudioParameterFloat>("TEMPO", "Tempo", 60.0f, 200.0f, 120.0f),
            std::make_unique<juce::AudioParameterFloat>("VOL", "Volume", -48.0f, 0.0f, -6.0f)
        }) {
        for(int i=0;i<6;++i) for(int j=0;j<16;++j) grid[i][j] = (i==0 && j%4==0) || (i==1 && (j==4||j==12)) || (i==2 && j%2==0);
    }
    ~NebulaDrumAudioProcessor() override {}
    void prepareToPlay(double sr, int) override { sampleRate = sr; currentStep = 0; nextStep = 0; for(auto& v: voices) v.active = false; }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals; buf.clear();
        float vol = juce::Decibels::decibelsToGain(*apvts.getRawParameterValue("VOL"));
        double sps = sampleRate / ((*apvts.getRawParameterValue("TEMPO") / 60.0) * 4.0);
        for (int i = 0; i < buf.getNumSamples(); ++i) {
            if (currentSample >= nextStep) {
                for (int inst = 0; inst < 6; ++inst) if (grid[inst][currentStep]) trigger(inst);
                currentStep = (currentStep + 1) % 16; nextStep += (int64)sps;
            }
            float outL = 0, outR = 0;
            for (auto& v : voices) {
                if (!v.active) continue;
                float freq = v.inst == 0 ? 60.0f : (v.inst == 1 ? 200.0f : 800.0f);
                float s = (v.inst == 2) ? (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) : std::sin(v.phase * 2.0 * juce::MathConstants<double>::pi);
                v.phase += freq / sampleRate; v.env *= (v.inst == 0 ? 0.95f : 0.85f);
                if (v.env < 0.001f) v.active = false; else { outL += s * v.env; outR += s * v.env; }
            }
            buf.setSample(0, i, outL * vol); buf.setSample(1, i, outR * vol); currentSample++;
        }
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new Editor(*this); }
    const juce::String getName() const override { return "NEBULA DRUM"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override { Nebula::StateManager::save(apvts, dest, uiW, uiH); }
    void setStateInformation(const void* data, int size) override { Nebula::StateManager::restore(apvts, data, size, uiW, uiH); }

    juce::AudioProcessorValueTreeState apvts;
    bool grid[6][16];
    int uiW = 800, uiH = 600;
private:
    struct Voice { bool active = false; int inst = 0; double phase = 0; float env = 1.0f; };
    Voice voices[8]; double sampleRate = 44100.0; int64 currentSample = 0, nextStep = 0; int currentStep = 0;
    void trigger(int inst) { for (auto& v : voices) if (!v.active) { v.active = true; v.inst = inst; v.phase = 0; v.env = 1.0f; break; } }
    
    class Editor : public juce::AudioProcessorEditor {
    public:
        Editor(NebulaDrumAudioProcessor& p) : AudioProcessorEditor(&p), proc(p) {
            setResizable(true, true); setResizeLimits(700, 500, 1800, 1000); setSize(proc.uiW, proc.uiH);
            for(int i=0;i<6;++i) { 
                labels[i].setText(i==0?"KICK":i==1?"SNARE":i==2?"HAT":"PERC", juce::dontSendNotification); 
                labels[i].setColour(juce::Label::textColourId, juce::Colours::white); 
                addAndMakeVisible(labels[i]);
                for(int j=0;j<16;++j) { 
                    btns[i][j].setClickingTogglesState(true); 
                    btns[i][j].setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFF0055)); 
                    btns[i][j].setToggleState(proc.grid[i][j], juce::dontSendNotification);
                    btns[i][j].onClick = [this,i,j](){ proc.grid[i][j] = btns[i][j].getToggleState(); }; 
                    addAndMakeVisible(btns[i][j]); 
                }
            }
            tempo.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); 
            tempo.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20); 
            tempo.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00AAFF));
            tempo.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            addAndMakeVisible(tempo);
            att.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, "TEMPO", tempo));
        }
        void paint(juce::Graphics& g) override { 
            g.fillAll(juce::Colour(0xFF151515)); 
            auto a = getLocalBounds().reduced(20); 
            g.setDropShadow(juce::DropShadow(juce::Colours::black.withAlpha(0.8f), 10, {})); 
            g.fillRoundedRectangle(a.toFloat(), 12.0f); 
            g.setColour(juce::Colours::white); 
            g.drawText("NEBULA DRUM", a.removeFromTop(40), juce::Justification::centred); 
        }
        void resized() override { 
            auto a = getLocalBounds().reduced(20, 60); 
            tempo.setBounds(a.removeFromTop(100).removeFromLeft(100)); 
            int rh = a.getHeight()/6, rw = a.getWidth()/17; 
            for(int i=0;i<6;++i) { 
                auto row = a.removeFromTop(rh); 
                labels[i].setBounds(row.removeFromLeft(80)); 
                for(int j=0;j<16;++j) btns[i][j].setBounds(row.removeFromLeft(rw).reduced(2)); 
            } 
            proc.uiW = getWidth(); proc.uiH = getHeight(); 
        }
    private:
        NebulaDrumAudioProcessor& proc; juce::ToggleButton btns[6][16]; juce::Label labels[6]; juce::Slider tempo; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NebulaDrumAudioProcessor(); }