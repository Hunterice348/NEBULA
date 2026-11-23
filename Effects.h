class EffectBase
{
public:
    virtual ~EffectBase() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    void process(juce::AudioBuffer<float>& buffer)
    {
        juce::ScopedNoDenormals noDenormals;

        const int samples = buffer.getNumSamples();
        const int channels = buffer.getNumChannels();

        if (samples <= 0 || channels <= 0)
            return;

        if (!enabled.load(std::memory_order_relaxed))
            return;

        if (samples > dryBuffer.getNumSamples()
            || channels > dryBuffer.getNumChannels())
            return;

        const float targetMix = nebula::dsp::clampFinite(
            mixAmount.load(std::memory_order_relaxed),
            0.0f, 1.0f, 1.0f);

        mixSmoother.setTargetValue(targetMix);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* input = buffer.getWritePointer(ch);

            for (int i = 0; i < samples; ++i)
            {
                input[i] = nebula::dsp::sanitizeSample(input[i]);
                dryBuffer.setSample(ch, i, input[i]);
            }
        }

        processWet(buffer);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* dry = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < samples; ++i)
            {
                const float amount = mixSmoother.getNextValue();

                const float wetSample =
                    nebula::dsp::sanitizeSample(wet[i]);

                const float drySample =
                    nebula::dsp::sanitizeSample(dry[i]);

                const float output =
                    drySample * (1.0f - amount)
                    + wetSample * amount;

                wet[i] = nebula::dsp::sanitizeSample(output);
            }
        }
    }

    virtual juce::String getTypeID() const = 0;
    virtual juce::String getDisplayName() const = 0;
    virtual int getNumParams() const = 0;
    virtual juce::String getParamName(int index) const = 0;
    virtual float getParam(int index) const = 0;
    virtual void setParam(int index, float value) = 0;

    bool isEnabled() const
    {
        return enabled.load(std::memory_order_relaxed);
    }

    void setEnabled(bool value)
    {
        enabled.store(value, std::memory_order_relaxed);
    }

    float getMix() const
    {
        return mixAmount.load(std::memory_order_relaxed);
    }

    void setMix(float value)
    {
        value = nebula::dsp::clampFinite(
            value, 0.0f, 1.0f, 1.0f);

        mixAmount.store(value, std::memory_order_relaxed);
    }

    virtual juce::ValueTree saveState() const
    {
        juce::ValueTree tree(getTypeID());

        tree.setProperty("on", isEnabled(), nullptr);
        tree.setProperty("mix", getMix(), nullptr);

        for (int i = 0; i < getNumParams(); ++i)
            tree.setProperty(
                "p" + juce::String(i),
                getParam(i),
                nullptr);

        return tree;
    }

    virtual void loadState(const juce::ValueTree& tree)
    {
        if (!tree.isValid() || !tree.hasType(getTypeID()))
            return;

        setEnabled((bool)tree.getProperty("on", true));
        setMix((float)tree.getProperty("mix", 1.0f));

        for (int i = 0; i < getNumParams(); ++i)
        {
            const juce::Identifier property(
                "p" + juce::String(i));

            if (tree.hasProperty(property))
                setParam(
                    i,
                    (float)tree.getProperty(property));
        }
    }

protected:
    virtual void processWet(
        juce::AudioBuffer<float>& buffer) = 0;

    void prepareBase(
        double sampleRate,
        int maxBlockSize,
        int maxChannels = 2)
    {
        sr = std::isfinite(sampleRate) && sampleRate > 1000.0
            ? sampleRate : 44100.0;

        bs = juce::jmax(1, maxBlockSize);

        dryBuffer.setSize(
            juce::jmax(1, maxChannels),
            bs,
            false,
            true,
            true);

        mixSmoother.prepare(sr, 15.0);
        mixSmoother.reset(
            mixAmount.load(std::memory_order_relaxed));
    }

    std::atomic<bool> enabled { true };
    std::atomic<float> mixAmount { 1.0f };

    double sr = 44100.0;
    int bs = 512;

    juce::AudioBuffer<float> dryBuffer;

private:
    nebula::dsp::LinearSmoother mixSmoother;
};