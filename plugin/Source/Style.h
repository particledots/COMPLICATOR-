// Colours, fonts and text helpers of the COMPLICATOR panel.
// Look (approved 2026-11-26, the "gold sections" look of Anamnesis 0.3.1): black panel, gold hairline frames and flow
// lines, cream knob caps in gold range arcs, blue lamps as the only colour (on / chosen).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CxStyle
{
    inline juce::Colour panel()       { return juce::Colour (0xFF0B0B0C); }
    inline juce::Colour window()      { return juce::Colour (0xFF0B0B0C); }   // behind the rounded panel corners
    inline juce::Colour gold()        { return juce::Colour (0xFFB08D57); }   // frames, lines, outlines
    inline juce::Colour goldText()    { return juce::Colour (0xFFD4B27A); }   // the title, section and row titles
    inline juce::Colour text()        { return juce::Colour (0xFFE8E1D0); }   // labels, chosen settings
    inline juce::Colour soft()        { return juce::Colour (0xFFB5AC98); }   // labels of switches that are off
    inline juce::Colour muted()       { return juce::Colour (0xFFA39886); }   // values, choices not chosen
    inline juce::Colour lamp()        { return juce::Colour (0xFF4C9BFF); }   // lit lamps
    inline juce::Colour lampFill()    { return juce::Colour (0xFF1B3A66); }   // highlighted menu entry
    inline juce::Colour skirt()       { return juce::Colour (0xFF121212); }   // knob skirts
    inline juce::Colour skirtLine()   { return juce::Colour (0xFF2E2A22); }
    inline juce::Colour cap()         { return juce::Colour (0xFFE9E2D2); }   // knob caps
    inline juce::Colour capLine()     { return juce::Colour (0xFFBFB6A2); }
    inline juce::Colour pointer()     { return juce::Colour (0xFF161514); }
    inline juce::Colour menu()        { return juce::Colour (0xFF16161A); }   // pop-up menus

    // The panel is designed at 680 x 708 units and scaled to the window (drag the corner to resize).
    inline constexpr int panelW = 680;
    inline constexpr int panelH = 708;

    // The title "COMPLICATOR" is centred at x = 340 and must stay between these two x positions at every window size
    // (inside the two gold ornament lines; checked by ComplicatorCheck, also in the drawn pixels).
    inline constexpr float titleLeft = 220.0f, titleRight = 460.0f, titleSize = 22.0f, titleSpacing = 7.0f, titleBaseline = 33.0f;
    inline constexpr float ornamentLeftEnd = 210.0f, ornamentRightStart = 470.0f;

    // Space Grotesk (built in, so every Mac and the check measure the same). size = the draft's font size,
    // spacing = extra letter spacing in units.
    juce::Font font (float size, bool medium = false, float spacing = 0.0f);
    // Marcellus (built in): the title.
    juce::Font titleFont();

    // The fonts of the panel's recurring texts.
    inline juce::Font capsFont()  { return font (11.0f, false, 2.0f); }    // section titles
    inline juce::Font rowFont()   { return font (11.0f, false, 1.5f); }    // row titles
    inline juce::Font labelFont() { return font (10.0f, false, 1.2f); }    // knob labels
    inline juce::Font menuFont()  { return font (10.5f, false, 0.2f); }    // what a closed menu shows
    inline juce::Font pillFont()  { return font (11.0f, false, 0.8f); }    // switch labels

    // Every text the fixed artwork draws can be recorded (for the check program: no text collides or leaves the panel).
    inline std::vector<juce::Rectangle<float>>*& textLog() { static std::vector<juce::Rectangle<float>>* p = nullptr; return p; }

    inline float widthOf (const juce::String& s, const juce::Font& f)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }
    inline float textWidth (const juce::String& s, float size, bool medium = false, float spacing = 0.0f)
    {
        return widthOf (s, font (size, medium, spacing));
    }
    inline float titleWidth() { return widthOf ("COMPLICATOR", titleFont()); }

    // Text with y as its baseline; x is the left end, the right end or the centre.
    inline void textIn (juce::Graphics& g, const juce::String& s, const juce::Font& f, float x, float y, juce::Colour c,
                        juce::Justification just)
    {
        g.setColour (c);
        g.setFont (f);
        const float asc = f.getAscent(), h = f.getHeight();
        const float w = 600.0f;
        juce::Rectangle<float> r;
        if (just == juce::Justification::centredLeft)       r = { x, y - asc, w, h };
        else if (just == juce::Justification::centredRight) r = { x - w, y - asc, w, h };
        else                                                 r = { x - w * 0.5f, y - asc, w, h };
        g.drawText (s, r, just, false);
        if (auto* log = textLog())
        {
            const float tw = widthOf (s, f);
            const float left = just == juce::Justification::centredLeft ? x : just == juce::Justification::centredRight ? x - tw : x - tw * 0.5f;
            log->push_back ({ left, y - f.getAscent() * 0.8f, tw, f.getAscent() * 0.8f + 2.0f });
        }
    }

    // A lamp: lit = blue with a soft halo; off = a thin ring.
    inline void drawLamp (juce::Graphics& g, float cx, float cy, bool lit, float r = 3.2f)
    {
        if (lit)
        {
            g.setColour (lamp().withAlpha (0.22f));
            g.fillEllipse (cx - r * 2.2f, cy - r * 2.2f, r * 4.4f, r * 4.4f);
            g.setColour (lamp());
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            g.setColour (muted());
            g.drawEllipse (cx - r + 0.3f, cy - r + 0.3f, r * 2.0f - 0.6f, r * 2.0f - 0.6f, 0.9f);
        }
    }
}
