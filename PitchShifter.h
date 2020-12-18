/***************************************************************************************
This class implements a Doppler-effect based pitch shifting algorithm
****************************************************************************************/


#include <JuceHeader.h>
#define TP_RANGE 0.010           // specifies the transposition range in milliseconds (used for allocation of delay buffer)

class PitchShifter {

public:

    PitchShifter()
    {
        // initialization happens in initialize()
    }
    
    //************* Initialization of the delay buffer, hardcoded for 2 channels atm for demonstration purposes*******************//
    //************* Regarding scalibiltiy, nr. of channels and buffer size should be provided as parameters under GUI control ***//

    void initialize(int SamplesPerBlockExpected, double SampleRate) {

        transposition_range = TP_RANGE * SampleRate;
        delayBufferSize = SamplesPerBlockExpected + transposition_range;
        delayBuffer.setSize(2, delayBufferSize);
        delayBuffer.clear();
    }


    //************ Actual DSP callback, applying the pitch shift to a single channel**********************************************//
    //************ Hence, when using multi-channel (polyphonic) pitch shift, this function has to be called in a channel loop. ***//
    //************ The implementation uses two different 'delay lines' within the same delay buffer, by sawtooth modulation ******//
    //************ of the delay time. Each delay line has its separate sawtooth modulator and they are 180° out of phase. ********//
    //************ To eliminate glitches, we use sine envelopes for each delay line, and since they are 180° out of phase, *******//
    //************ output power is constant. *************************************************************************************//

    void process(AudioBuffer<float>* inbuffer, int startSample, int numSamples, int maxDelayInSamples, int channel, float deviceGain)						
    {
        float* writeBuffer = inbuffer->getWritePointer(channel, startSample);

        const int delayBufferSize = delayBuffer.getNumSamples();
        fillDelaybuffer(inbuffer->getNumSamples(), channel, delayBufferSize, inbuffer->getReadPointer(channel, startSample), 1.0);

        for (auto sample = 0; sample < numSamples; ++sample)

        {

            const float* delay = delayBuffer.getReadPointer(channel, startSample);

            float delaySamples1 = sawtooth1(maxDelayInSamples, channel);
            float delaySamples2 = sawtooth2(maxDelayInSamples, channel);
            int delayTime1 = static_cast<int>(delaySamples1);
            int delayTime2 = static_cast<int>(delaySamples2);

            int readPosition1 = (delayBufferSize + delayBufferWritePosition - delayTime1) % delayBufferSize;
            int readPosition2 = (delayBufferSize + delayBufferWritePosition - delayTime2) % delayBufferSize;


            float gain1 = sin(double_Pi * delaySamples1 / maxDelayInSamples);
            float gain2 = sin(double_Pi * delaySamples2 / maxDelayInSamples);


            writeBuffer[sample] = deviceGain*(gain1 * (delay[(readPosition1 + sample) % delayBufferSize]) + gain2 * (delay[(readPosition2 + sample) % delayBufferSize]));

        }

    }

    //******** This function copies each packet received at the callback into the circular delay buffer. This allows the algorithm***//
    //******** to use an 'arbitrarily' delayed sample within the transposition range. This way, the sawtooth modulators *************//
    //******** can specifiy the position in the buffer at any time instance for their respective delay line *************************//

    void fillDelaybuffer(const int bufferLength, int channel, const int delayBufferLength, const float* bufferData, const float gain)
    {

        if (delayBufferLength > bufferLength + delayBufferWritePosition)
        {
            delayBuffer.copyFromWithRamp(channel, delayBufferWritePosition, bufferData, bufferLength, gain, gain);
        }
        else
        {
            const int delayBufferRemaining = delayBufferLength - delayBufferWritePosition;
            delayBuffer.copyFromWithRamp(channel, delayBufferWritePosition, bufferData, delayBufferRemaining, gain, gain);
            delayBuffer.copyFromWithRamp(channel, 0, bufferData + delayBufferRemaining, bufferLength - delayBufferRemaining, gain, gain);

        }

    }


    //********* The sawtooth modulators, that output at each given time instance the amount of delay that needs to be implemented in *****//
    //********* the delay lines. It returns a float at this point, to leave room for possible (linear) interpolation of the delay time. **//
    //********* When assessing the audio qualitatively, interpolation seemed not necessary in this implementation.************************//


    float sawtooth1(int maxDelayInSamples, int channel) {																	

        float samplespercycle = sampleRate / sawtoothFrequency;
        sawtoothPhase1[channel] += (1 / samplespercycle);   																
        if (sawtoothPhase1[channel] >= 1) sawtoothPhase1[channel] -= 1;
        if (pitchUporDown == false) return maxDelayInSamples * sawtoothPhase1[channel];
        if (pitchUporDown == true) return maxDelayInSamples * (1 - sawtoothPhase1[channel]);
    }


    float sawtooth2(int maxDelayInSamples, int channel) {
        
        float samplespercycle = sampleRate / sawtoothFrequency;                                                             
        sawtoothPhase2[channel] += (1 / samplespercycle);   																
        if (sawtoothPhase2[channel] >= 1) sawtoothPhase2[channel] -= 1;
        if (pitchUporDown == false) return maxDelayInSamples * sawtoothPhase2[channel];
        if (pitchUporDown == true) return maxDelayInSamples * (1 - sawtoothPhase2[channel]);

    }


    //************ To update the write index of the circular buffer after storing a packet in the callback. We don't do this in the fillDelayBuffer ** //
    //************ as we can only adjust it after each channel has been copied. Hence, it has to be called by the owning class after the channel loop *//
    //************ has completed **********************************************************************************************************************//

    void adjustDelayBufferWritePosition(int numsamplesInBuffer)                                                             
    {
        delayBufferWritePosition += numsamplesInBuffer;
        delayBufferWritePosition %= delayBufferSize;
    }



    //**********  Setter member functions for GUI controlled owner of the pitch shifting object ***********************************************//


    void setUp()
    {
        pitchUporDown = true;
    }

    void setDown()
    {
        pitchUporDown = false;
    }

    void setLevel(float rate)
    {
        sawtoothFrequency = rate;
    }

 
private:
    
    float sawtoothPhase1[2]{ 0,0 }, sawtoothPhase2[2]{ 0.5,0.5 };  // sawtooth functions shifted by pi/2 with respect to each other
    float sawtoothFrequency{0.0 };

    float sampleRate{ 44100 };
    int delayBufferWritePosition{ 0 };
    int transposition_range;
    int delayBufferSize;
    bool pitchUporDown{ false };
    juce::AudioBuffer<float> delayBuffer;

};