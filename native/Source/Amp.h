// Vintage amp furniture: tweed covering, control plate, voicing tabs, pedals with patch cables.
#pragma once
#include "Widgets.h"

namespace altui
{
// A 96x96 tile of lacquered-tweed: tan base, dark vertical pinstripes, fine weave and speckle.
inline const juce::Image& tweedTile (bool dark = false)
{
    static juce::Image tiles[2];
    juce::Image& t = tiles[dark ? 1 : 0];
    if (! t.isValid())
    {
        t = juce::Image (juce::Image::RGB, 96, 96, false);
        juce::Image::BitmapData bd (t, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 96; ++y)
            for (int x = 0; x < 96; ++x)
            {
                unsigned h = (unsigned) (x * 73856093u) ^ (unsigned) (y * 19349663u); h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
                float r = 214, g = 172, b = 98;                                  // tan
                const int sx = x % 16;
                if (sx < 2) { r = 120; g = 78; b = 36; }                         // dark pinstripe
                else if (sx == 2 || sx == 15) { r = 190; g = 144; b = 74; }
                const float weave = (((x >> 1) + (y >> 1)) & 1) ? 9.f : -9.f;
                const float speck = ((int) (h & 15) - 7) * 1.6f;
                const float k = dark ? 0.62f : 1.f;
                auto cl = [&] (float v) { return (juce::uint8) juce::jlimit (0.f, 255.f, (v + weave + speck) * k); };
                bd.setPixelColour (x, y, juce::Colour (cl (r), cl (g), cl (b)));
            }
    }
    return t;
}

inline void fillTweed (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.saveState();
    juce::Path p; p.addRoundedRectangle (r, corner); g.reduceClipRegion (p);
    g.setTiledImageFill (tweedTile(), (int) r.getX(), (int) r.getY(), 1.f); g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.22f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (juce::Colour (0xff2a1a0c)); g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.5f);
}

inline void drawRivet (juce::Graphics& g, float x, float y)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (x - 3.5f, y - 2.5f, 7, 7);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdedad0), x - 2, y - 2, juce::Colour (0xff5a5850), x + 2, y + 2, false)); g.fillEllipse (x - 3, y - 3, 6, 6);
}

// Row of voicing tabs. Left/Right arrows (handled by the main window) move through them.
class VoicingTabs : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    void setNames (juce::StringArray n) { names = std::move (n); repaint(); }
    void setSelected (int i) { if (i != sel) { sel = i; repaint(); } }
    int getSelected() const { return sel; }
    void paint (juce::Graphics& g) override
    {
        const int n = names.size(); if (n == 0) return;
        const float w = (float) getWidth() / n;
        g.setFont (juce::Font (juce::FontOptions (14.f, juce::Font::bold)));
        for (int i = 0; i < n; ++i)
        {
            auto r = juce::Rectangle<float> (i * w + 3, 4.f, w - 6, (float) getHeight() - 4.f);
            const bool on = i == sel, hot = i == hover;
            juce::Path p; p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() + 8, 7.f, 7.f, true, true, false, false);
            g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xffffc866), 0, r.getY(), juce::Colour (0xffd88a24), 0, r.getBottom(), false)
                                  : juce::ColourGradient (hot ? juce::Colour (0xff4a3a2a) : juce::Colour (0xff35291d), 0, r.getY(), juce::Colour (0xff1c140d), 0, r.getBottom(), false));
            g.fillPath (p);
            g.setColour (on ? juce::Colour (0xff3a2208) : juce::Colour (0xff6a5236)); g.strokePath (p, juce::PathStrokeType (1.2f));
            g.setColour (on ? juce::Colour (0xff1d1206) : juce::Colour (0xffe8d8b8)); g.drawText (names[i], r.toNearestInt(), juce::Justification::centred);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int i = indexAt (e.x); if (i >= 0 && onSelect) onSelect (i); }
private:
    juce::StringArray names; int sel = -1, hover = -1;
    int indexAt (int x) const { const int n = names.size(); return n ? juce::jlimit (0, n - 1, x * n / std::max (1, getWidth())) : -1; }
};

// Chrome stomp switch
class FootSwitch : public juce::Button
{
public:
    FootSwitch() : juce::Button ("footswitch") { setClickingTogglesState (true); setWantsKeyboardFocus (false); setMouseClickGrabsKeyboardFocus (false); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dce0), 0, r.getY(), juce::Colour (0xff7d8189), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (juce::Colour (0xff2a2b30)); g.drawRoundedRectangle (r, 5.f, 1.5f);
        auto in = r.reduced (6.f, 5.f); g.setColour (down ? juce::Colour (0xff3a3b40) : (over ? juce::Colour (0xff55575d) : juce::Colour (0xff4a4c52))); g.fillRoundedRectangle (in, 3.f);
        g.setColour (juce::Colour (0x40ffffff)); g.drawRoundedRectangle (in, 3.f, 1.f);
    }
};

// One compact pedal: three small knobs, a footswitch and an LED
class Pedal : public juce::Component
{
public:
    juce::String title; juce::Colour body, ink; FootSwitch foot; std::vector<std::unique_ptr<Knob>> knobs;
    std::function<void()> onChange;
    Pedal (juce::String t, juce::Colour c, juce::Colour i) : title (std::move (t)), body (c), ink (i)
    {
        addAndMakeVisible (foot); foot.onClick = [this] { repaint(); if (onChange) onChange(); };
    }
    Knob& add (const juce::String& name, double def)
    {
        auto k = std::make_unique<Knob> (name, 0, 10, def, 2); k->setInk (ink, ink);
        k->s.onValueChange = [this] { if (onChange) onChange(); };
        addAndMakeVisible (*k); knobs.push_back (std::move (k)); return *knobs.back();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.f);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (2, 3), 9.f);
        g.setGradientFill (juce::ColourGradient (body.brighter (0.18f), 0, r.getY(), body.darker (0.35f), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 9.f);
        g.setColour (juce::Colours::black.withAlpha (0.75f)); g.drawRoundedRectangle (r, 9.f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (r.reduced (2.f), 7.f, 1.f);
        g.setColour (ink); g.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold | juce::Font::italic)));
        g.drawText (title, juce::Rectangle<int> (getWidth() / 2 - 80, 4, 160, 18), juce::Justification::centred);
        // LED
        const bool on = foot.getToggleState(); const float lx = r.getCentreX(), ly = r.getY() + 31;
        g.setColour (on ? juce::Colour (0x55ff3a2a) : juce::Colours::transparentBlack); g.fillEllipse (lx - 9, ly - 9, 18, 18);
        g.setColour (on ? juce::Colour (0xffff4a38) : juce::Colour (0xff3a1210)); g.fillEllipse (lx - 4.5f, ly - 4.5f, 9, 9);
        g.setColour (juce::Colours::black); g.drawEllipse (lx - 4.5f, ly - 4.5f, 9, 9, 1.f);
        // jacks on the sides
        for (float sx : { r.getX() + 1.f, r.getRight() - 1.f }) { g.setColour (juce::Colour (0xff1a1a1c)); g.fillRoundedRectangle (sx - 4, getHeight() * 0.42f, 8, 14, 2.f); g.setColour (juce::Colour (0xffa6a9ae)); g.drawRoundedRectangle (sx - 4, getHeight() * 0.42f, 8, 14, 2.f, 1.f); }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 0); r.removeFromTop (38);
        auto kr = r.removeFromTop (54); const int n = (int) knobs.size(), kw = n ? kr.getWidth() / n : 0;
        for (auto& k : knobs) k->setBounds (kr.removeFromLeft (kw).reduced (1, 0));
        r.removeFromTop (4); r.removeFromBottom (7); foot.setBounds (r.withSizeKeepingCentre (getWidth() - 56, std::max (18, r.getHeight())));
    }
};

// The three pedals with patch cables between them
class PedalBoard : public juce::Component
{
public:
    Pedal drive { "Blues Crunch", juce::Colour (0xff1e5fb3), juce::Colour (0xffeef3ff) };
    Pedal delay { "Echo Delay", juce::Colour (0xff7a3fa8), juce::Colour (0xfff6ecff) };
    Pedal reverb { "Hall Reverb", juce::Colour (0xff1f8a6a), juce::Colour (0xffeafff6) };
    PedalBoard() { addAndMakeVisible (drive); addAndMakeVisible (delay); addAndMakeVisible (reverb); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff111114)); g.fillRoundedRectangle (r, 8.f);
        g.setColour (juce::Colour (0xff2c2e34)); g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
        g.drawText ("PEDALBOARD   -   Blues Crunch sits in front of the amp, Echo and Hall after it", 14, 3, getWidth() - 28, 18, juce::Justification::centredLeft);
        const float y = (float) drive.getY() + drive.getHeight() * 0.42f + 7.f;
        auto cable = [&] (float x0, float x1)
        {
            juce::Path p; p.startNewSubPath (x0, y); p.cubicTo (x0 + (x1 - x0) * 0.3f, y + 30, x1 - (x1 - x0) * 0.3f, y + 30, x1, y);
            g.setColour (juce::Colours::black); g.strokePath (p, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xff3a3d46)); g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            for (float px : { x0, x1 }) { g.setColour (juce::Colour (0xffb5b8bd)); g.fillRoundedRectangle (px - 6, y - 4, 12, 8, 2.f); g.setColour (juce::Colours::black); g.drawRoundedRectangle (px - 6, y - 4, 12, 8, 2.f, 1.f); }
        };
        cable (14.f, (float) drive.getX() + 4);
        cable ((float) drive.getRight() - 4, (float) delay.getX() + 4);
        cable ((float) delay.getRight() - 4, (float) reverb.getX() + 4);
        cable ((float) reverb.getRight() - 4, (float) getWidth() - 14.f);
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold)));
        g.drawText ("GUITAR", 4, (int) y + 6, 50, 14, juce::Justification::centredLeft); g.drawText ("OUT", getWidth() - 40, (int) y + 6, 36, 14, juce::Justification::centredRight);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (60, 0); r.removeFromTop (20); r.removeFromBottom (6);
        const int gap = 56, pw = (r.getWidth() - gap * 2) / 3;
        drive.setBounds (r.removeFromLeft (pw)); r.removeFromLeft (gap); delay.setBounds (r.removeFromLeft (pw)); r.removeFromLeft (gap); reverb.setBounds (r.removeFromLeft (pw));
    }
};

// A clickable section header with an arrow, used to collapse the noise control
class FoldHeader : public juce::Component
{
public:
    juce::String text; bool open = true; std::function<void()> onToggle;
    void paint (juce::Graphics& g) override
    {
        g.setColour (cAmber); juce::Path p;
        if (open) p.addTriangle (6.f, 9.f, 18.f, 9.f, 12.f, 19.f); else p.addTriangle (8.f, 6.f, 8.f, 20.f, 19.f, 13.f);
        g.fillPath (p);
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold))); g.drawText (text, 28, 0, getWidth() - 40, getHeight(), juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent&) override { open = ! open; if (onToggle) onToggle(); repaint(); }
    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::PointingHandCursor; }
};
} // namespace altui
