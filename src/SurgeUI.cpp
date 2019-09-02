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
#include "../dpf/distrho/DistrhoUI.hpp"
#include "common/gui/SurgeGUIEditor.h"

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------

class SurgeUI : public UI
{
public:
    /* constructor */
    SurgeUI()
        : UI(),
          winId((void*)getNextWindowId())
    {
        SurgeSynthesizer* const synth = (SurgeSynthesizer*)getPluginInstancePointer();
        DISTRHO_SAFE_ASSERT_RETURN(synth != nullptr,);

        editor = new SurgeGUIEditor(this, synth);

        ERect *rect = nullptr;
        editor->getRect (&rect);
        DISTRHO_SAFE_ASSERT_RETURN(rect != nullptr,);

        setSize(rect->right, rect->bottom);
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks */

    void parameterChanged(uint32_t index, float value) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(editor != nullptr,);

//         editor->setParameter(index, value);
    }

   /* --------------------------------------------------------------------------------------------------------
    * Widget Callbacks */

    void setVisible(const bool yesNo) override
    {
        UI::setVisible(yesNo);

        DISTRHO_SAFE_ASSERT_RETURN(editor != nullptr,);

        if (yesNo)
        {
            d_stdout("set visible called %p", winId);
            editor->open(winId);
        }
        else
        {
            editor->close();
        }
    }

    void idle() override
    {
        if (editor != nullptr)
            editor->idle();
    }

    // -------------------------------------------------------------------------------------------------------

private:
    ScopedPointer<SurgeGUIEditor> editor;
    void* const winId;

   /**
      Set our UI class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgeUI)
};

// -----------------------------------------------------------------------

UI* createUI()
{
    return new SurgeUI();
}

// -----------------------------------------------------------------------

END_NAMESPACE_DISTRHO
