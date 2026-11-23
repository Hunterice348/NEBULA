
void process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (!enabled.load(std::memory_order_relaxed))
        return;

    const float wetAmount = juce::jlimit(
        0.0f,
        1.0f,
        mixAmount.load(std::memory_order_relaxed)
    );

    if (wetAmount < 0.001f)
        return;

    const int capacity = dryBuffer.getNumSamples();

    if (capacity <= 0
        || numChannels > dryBuffer.getNumChannels()
        || numChannels > 2)
        return;

    // Process oversized host blocks in bounded chunks.
    // No heap allocation is performed in this loop.
    int offset = 0;

    while (offset < numSamples)
    {
        const int chunkSize = juce::jmin(
            capacity,
            numSamples - offset
        );

        std::array<float*, 2> channelPointers {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            channelPointers[static_cast<size_t>(ch)] =
                buffer.getWritePointer(ch, offset);
        }

        // A non-owning view into the original audio buffer.
        juce::AudioBuffer<float> chunkBuffer(
            channelPointers.data(),
            numChannels,
            chunkSize
        );

        // Preserve dry signal for this chunk.
        for (int ch = 0; ch < numChannels; ++ch)
        {
            dryBuffer.copyFrom(
                ch,
                0,
                chunkBuffer,
                ch,
                0,
                chunkSize
            );
        }

        // Run the actual effect DSP.
        processWet(chunkBuffer);

        // Apply per-effect wet/dry mix.
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = chunkBuffer.getWritePointer(ch);
            const auto* dry = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < chunkSize; ++i)
            {
                float sample = std::isfinite(wet[i])
                    ? wet[i]
                    : 0.0f;

                if (std::abs(sample) < 1.0e-30f)
                    sample = 0.0f;

                wet[i] =
                    dry[i] * (1.0f - wetAmount)
                    + sample * wetAmount;
            }
        }

        offset += chunkSize;
    }
}
