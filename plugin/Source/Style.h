// Colours and text helpers of the COMPLICATOR panel (the approved draft, 2026-11-17).
// Original look: dark ink panel, Osc A in coral, Osc B in teal, the shared sections in lavender.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CxStyle
{
    inline juce::Colour panel()      { return juce::Colour (0xFF16171B); }
    inline juce::Colour window()     { return juce::Colour (0xFF0D0E10); }   // behind the rounded panel corners
    inline juce::Colour control()    { return juce::Colour (0xFF1E1F24); }   // menus and switch backgrounds
    inline juce::Colour knob()       { return juce::Colour (0xFF25262C); }
    inline juce::Colour line()       { return juce::Colour (0xFF3A3B42); }
    inline juce::Colour text()       { return juce::Colour (0xFFE8E6DF); }
    inline juce::Colour muted()      { return juce::Colour (0xFF9A9993); }
    inline juce::Colour oscA()       { return juce::Colour (0xFFF0997B); }
    inline juce::Colour oscB()       { return juce::Colour (0xFF5DCAA5); }
    inline juce::Colour shared()     { return juce::Colour (0xFFAFA9EC); }
    inline juce::Colour tagLine()    { return juce::Colour (0xFFEF9F27); }
    inline juce::Colour tagText()    { return juce::Colour (0xFFFAC775); }
    inline juce::Colour scopeBack()  { return juce::Colour (0xFF0F1013); }

    inline constexpr float dimAlpha = 0.38f;        // controls that arrive in a later milestone

    // The panel is designed at 680 x 664 units and scaled to the window.
    inline constexpr int panelW = 680;
    inline constexpr int panelH = 664;

    inline juce::Font font (float size) { return juce::Font (juce::FontOptions (size)); }

    // Text centred on x, with y as its baseline (like the draft's SVG text).
    inline void textAt (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size = 11.0f,
                        juce::Justification just = juce::Justification::centred)
    {
        g.setColour (c);
        g.setFont (font (size));
        const float w = just == juce::Justification::centredLeft ? 640.0f : 260.0f, h = size * 1.4f;
        juce::Rectangle<float> r;
        if (just == juce::Justification::centredLeft) r = { x, y - size * 1.0f, w, h };
        else                                          r = { x - w * 0.5f, y - size * 1.0f, w, h };
        g.drawText (s, r, just, false);
    }
}
