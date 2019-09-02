/*
 * Surge DPF Port
 * Copyright (C) 2019 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "DistrhoPluginInfo.h"
#include "../dpf/distrho/DistrhoPlugin.hpp"
#include "common/SurgeSynthesizer.h"

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

/**
  Simple plugin to demonstrate state usage (including UI).
  The plugin will be treated as an effect, but it will not change the host audio.
 */
class SurgePlugin : public Plugin
{
public:
    SurgePlugin()
        : Plugin(n_total_params, 0, 0), // parameters, programs, states
          _instance(nullptr),
          blockpos(0)
    {
        SurgeSynthesizer* const synth = (SurgeSynthesizer*)_aligned_malloc(sizeof(SurgeSynthesizer), 16);
        DISTRHO_SAFE_ASSERT_RETURN(synth != nullptr,);

        new(synth)SurgeSynthesizer(this);
        _instance = (SurgeSynthesizer*)synth;
        _instance->setSamplerate(getSampleRate());

        // needed?
        _instance->time_data.ppqPos = 0;
    }

    ~SurgePlugin() override
    {
        if (_instance == nullptr)
            return;

        _instance->~SurgeSynthesizer();
        _aligned_free(_instance);
        _instance = nullptr;
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * Information */

    const char* getLabel() const override
    {
        return "surge";
    }

    const char* getDescription() const override
    {
        return "";
    }

    const char* getMaker() const override
    {
        return "Vember Audio";
    }

    const char* getHomePage() const override
    {
        return "https://github.com/kurasu/surge";
    }

    const char* getLicense() const override
    {
        return "ISC";
    }

    uint32_t getVersion() const override
    {
        return d_version(1, 0, 0);
    }

    int64_t getUniqueId() const override
    {
        return d_cconst('d', 'x', 'x', 'x');
    }

   /* --------------------------------------------------------------------------------------------------------
    * Init */

    void initParameter(uint32_t index, Parameter& param) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr,);

        char label[256];
        _instance->getParameterName(_instance->remapExternalApiToInternalId(index), label);

        param.hints = kParameterIsAutomable;
        param.name = label;
        param.symbol = label;
        param.symbol.toBasic();
        param.ranges.def = _instance->getParameter01(_instance->remapExternalApiToInternalId(index));
    }

   /* --------------------------------------------------------------------------------------------------------
    * Internal data */

    void setParameterValue(uint32_t index, float value) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr,);

        _instance->setParameter01(_instance->remapExternalApiToInternalId(index), value, true);
    }

    float getParameterValue(uint32_t index) const  override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr, 0.0f);

        return _instance->getParameter01(_instance->remapExternalApiToInternalId(index));
    }

   /* --------------------------------------------------------------------------------------------------------
    * Process */

    void activate() override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr,);

        blockpos = 0;
        _instance->audio_processing_active = true;
    }

    void deactivate() override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr,);

        _instance->allNotesOff();
        _instance->audio_processing_active = false;
    }

    void processMidiEvent(const MidiEvent& midiEvent)
    {
        const uint8_t* const midiData = midiEvent.size > MidiEvent::kDataSize ? midiEvent.dataExt : midiEvent.data;

        const int status = midiData[0] & 0xf0;
        const int channel = midiData[0] & 0x0f;

        switch (status)
        {
        case 0x90:
        case 0x80:
        {
            const int newnote = midiData[1] & 0x7f;
            const int velo = midiData[2] & 0x7f;
            if ((status == 0x80) || (velo == 0))
            {
                _instance->releaseNote((char)channel, (char)newnote, (char)velo);
            }
            else
            {
                _instance->playNote((char)channel, (char)newnote, (char)velo, 0);
            }
            break;
        }

        case 0xe0: // pitch bend
        {
            const long value = (midiData[1] & 0x7f) + ((midiData[2] & 0x7f) << 7);
            _instance->pitchBend((char)channel, value - 8192);
            break;
        }

        case 0xB0: // controller
        {
            if (midiData[1] == 0x7b || midiData[1] == 0x7e)
                _instance->allNotesOff(); // all notes off
            else
                _instance->channelController((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
            break;
        }

        case 0xC0: // program change
            _instance->programChange((char)channel, midiData[1] & 0x7f);
            break;

        case 0xD0: // channel aftertouch
            _instance->channelAftertouch((char)channel, midiData[1] & 0x7f);
            break;

        case 0xA0: // poly aftertouch
            _instance->polyAftertouch((char)channel, midiData[1] & 0x7f, midiData[2] & 0x7f);
            break;

        case 0xfc:
        case 0xff: //  MIDI STOP or reset
            _instance->allNotesOff();
            break;
        }
    }

    void processMidiEvents(const uint32_t frameOffset,
                           const MidiEvent* const midiEvents,
                           const uint32_t midiEventCount,
                           uint32_t& eventIndex)
    {
        while (eventIndex < midiEventCount)
        {
            const MidiEvent& midiEvent(midiEvents[eventIndex]);

            if (midiEvent.frame < frameOffset)
            {
                processMidiEvent(midiEvent);
                eventIndex++;
            }
            else
            {
                break;
            }
        }
    }

    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr,);

        SurgeSynthesizer* const s = _instance;
        s->process_input = false; // true if synth

        int n_outputs = _instance->getNumOutputs();
        int n_inputs = _instance->getNumInputs();

        const TimePosition& timePos(getTimePosition());
        const double tempo = timePos.bbt.valid ? timePos.bbt.beatsPerMinute : 120.;

        // TODO
        // s->time_data.ppqPos = 0;

        timedata& td(s->time_data);
        s->time_data.tempo = tempo;

        uint32_t midiEventIndex = 0;

        for (uint32_t i=0; i<frames; ++i)
        {
            if (blockpos == 0)
            {
                if (timePos.playing)
                    s->time_data.ppqPos += (double)BLOCK_SIZE * tempo / (60. * getSampleRate());

                processMidiEvents(i, midiEvents, midiEventCount, midiEventIndex);
                s->process();
            }

            /*
            if (s->process_input)
            {
                for (int inp = 0; inp < n_inputs; inp++)
                {
                    s->input[inp][blockpos] = inputs[inp][i];
                }
            }
            */

            for (int outp = 0; outp < n_outputs; outp++)
                outputs[outp][i] = s->output[outp][blockpos];

            blockpos++;
            if (blockpos >= BLOCK_SIZE)
                blockpos = 0;
        }
    }

   /* --------------------------------------------------------------------------------------------------------
    * Callbacks (optional) */

    void sampleRateChanged(double newSampleRate) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(_instance != nullptr,);

        _instance->setSamplerate(newSampleRate);
    }

    // -------------------------------------------------------------------------------------------------------

private:
   SurgeSynthesizer* _instance;
   int blockpos;

   /**
      Set our plugin class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgePlugin)
};

/* ------------------------------------------------------------------------------------------------------------
 * Plugin entry point, called by DPF to create a new plugin instance. */

Plugin* createPlugin()
{
    return new SurgePlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
