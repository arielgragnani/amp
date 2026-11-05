// Vintage amp furniture: tweed covering, control plate, voicing tabs, toggle switches, pedals with patch cables, tuner.
#pragma once
#include "Widgets.h"

namespace altui
{
// ---------- covering ("skin") for the amp, one tint per voicing ----------
struct Skin { juce::Colour base, thread, cream; float lacquer; };
inline const Skin& skinFor (int i)
{
    static const Skin s[4] = {
        { juce::Colour (0xffd2a85c), juce::Colour (0xff6d4620), juce::Colour (0xffeed9a2), 0.28f },   // 0 amber lacquered tweed
        { juce::Colour (0xff6f7b88), juce::Colour (0xff1c232c), juce::Colour (0xffc6d0da), 0.10f },   // 1 cool blue-grey sparkle
        { juce::Colour (0xff8e4a3c), juce::Colour (0xff341611), juce::Colour (0xffd9a58e), 0.18f },   // 2 oxblood
        { juce::Colour (0xffd2a85c), juce::Colour (0xff6d4620), juce::Colour (0xffeed9a2), 0.28f } }; // custom: same as amber
    return s[juce::jlimit (0, 3, i)];
}

// 96x96 tile: vertical thread pairs, twill weave between them, fine speckle
inline const juce::Image& tweedTile (int skin)
{
    static juce::Image tiles[4];
    juce::Image& t = tiles[juce::jlimit (0, 3, skin)];
    if (! t.isValid())
    {
        const Skin& k = skinFor (skin);
        t = juce::Image (juce::Image::RGB, 96, 96, false);
        juce::Image::BitmapData bd (t, juce::Image::BitmapData::writeOnly);
        auto mix = [] (juce::Colour a, juce::Colour b, float f) { return a.interpolatedWith (b, juce::jlimit (0.f, 1.f, f)); };
        for (int y = 0; y < 96; ++y)
            for (int x = 0; x < 96; ++x)
            {
                unsigned h = (unsigned) (x * 73856093u) ^ (unsigned) (y * 19349663u); h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
                const float rnd = ((int) (h & 255) - 128) / 128.f;
                const int u = x % 12;                                                  // 12 px repeat
                juce::Colour c = k.base;
                if (u <= 1) c = mix (k.base, k.thread, 0.85f - 0.2f * u);              // dark thread pair
                else if (u == 2) c = mix (k.base, k.thread, 0.25f);
                else if (u == 9 || u == 10) c = mix (k.base, k.cream, 0.45f);          // light thread
                // twill: diagonal ticks that shift with the row
                const int tw = (x + y * 2) & 7;
                const float tick = tw < 2 ? 0.16f : (tw < 4 ? -0.06f : 0.0f);
                const float row = ((y & 3) == 0) ? -0.06f : 0.0f;                      // faint weft shadow
                c = tick >= 0 ? mix (c, k.cream, tick) : mix (c, juce::Colours::black, -tick);
                c = mix (c, juce::Colours::black, -row);
                c = c.withMultipliedBrightness (1.f + rnd * 0.05f);
                bd.setPixelColour (x, y, c);
            }
    }
    return t;
}

// Cover a rounded rectangle with the skin: lacquer sheen, rounded-edge shading, piping
inline void fillTweed (juce::Graphics& g, juce::Rectangle<float> r, float corner, int skin)
{
    const Skin& k = skinFor (skin);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 3), corner);
    g.saveState();
    juce::Path p; p.addRoundedRectangle (r, corner); g.reduceClipRegion (p);
    g.setTiledImageFill (tweedTile (skin), (int) r.getX(), (int) r.getY(), 1.f); g.fillRect (r);
    // lacquer: amber film, glossier toward the top
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd98a).withAlpha (k.lacquer), 0, r.getY(), juce::Colour (0xff6a3b0a).withAlpha (k.lacquer * 0.9f), 0, r.getBottom(), false)); g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.5f, r.getY() + r.getHeight(), false)); g.fillRect (r);
    // roll-off at the edges so it reads as a padded box
    for (int i = 0; i < 9; ++i) { g.setColour (juce::Colours::black.withAlpha (0.20f * (1.f - i / 9.f))); g.drawRoundedRectangle (r.reduced (0.5f + (float) i), juce::jmax (1.f, corner - i), 1.f); }
    g.restoreState();
    g.setColour (juce::Colour (0xff1a1008)); g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.6f);
    g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (2.f), corner - 1.5f, 1.f);
}

inline void drawRivet (juce::Graphics& g, float x, float y)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (x - 3.5f, y - 2.5f, 7, 7);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e4da), x - 2, y - 2, juce::Colour (0xff55534b), x + 2, y + 2, false)); g.fillEllipse (x - 3, y - 3, 6, 6);
}

// Dark aged control plate with fine brushing
inline void drawPlate (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 2.5f), 4.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35353a), 0, r.getY(), juce::Colour (0xff17171a), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 4.f);
    g.saveState(); juce::Path p; p.addRoundedRectangle (r, 4.f); g.reduceClipRegion (p);
    for (int i = 0, n = (int) r.getHeight(); i < n; i += 2) { unsigned h = (unsigned) i * 2654435761u; h ^= h >> 15; g.setColour (((h & 1) ? juce::Colours::white : juce::Colours::black).withAlpha (0.018f + 0.02f * ((h >> 3) & 3) / 3.f)); g.fillRect (r.getX(), r.getY() + (float) i, r.getWidth(), 1.f); }
    g.restoreState();
    g.setColour (juce::Colour (0xff77756d)); g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (2.f), 3.f, 1.f);
}

// ---------- voicing tabs ----------
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
        g.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold)));
        for (int i = 0; i < n; ++i)
        {
            auto r = juce::Rectangle<float> (i * w + 3, 4.f, w - 6, (float) getHeight() - 4.f);
            const bool on = i == sel, hot = i == hover; const Skin& k = skinFor (i);
            juce::Path p; p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() + 8, 7.f, 7.f, true, true, false, false);
            const auto top = on ? k.base.brighter (0.35f) : (hot ? k.thread.brighter (0.5f) : k.thread.brighter (0.15f)), bot = on ? k.base.darker (0.2f) : k.thread.darker (0.5f);
            g.setGradientFill (juce::ColourGradient (top, 0, r.getY(), bot, 0, r.getBottom(), false)); g.fillPath (p);
            g.setColour (juce::Colours::black.withAlpha (0.8f)); g.strokePath (p, juce::PathStrokeType (1.4f));
            g.setColour (on ? juce::Colour (0xff1a1006) : juce::Colour (0xfff0e6cc)); g.drawText (names[i], r.toNearestInt(), juce::Justification::centred);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int i = indexAt (e.x); if (i >= 0 && onSelect) onSelect (i); }
private:
    juce::StringArray names; int sel = -1, hover = -1;
    int indexAt (int x) const { const int n = names.size(); return n ? juce::jlimit (0, n - 1, x * n / std::max (1, getWidth())) : -1; }
};

// ---------- chrome toggle switch with a printed label ----------
class ToggleSwitch : public juce::Button
{
public:
    explicit ToggleSwitch (const juce::String& label) : juce::Button (label) { setClickingTogglesState (true); setWantsKeyboardFocus (false); setMouseClickGrabsKeyboardFocus (false); }
    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const float cx = getWidth() * 0.5f, by = 19.f; const bool on = getToggleState();
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - 11, by - 9, 22, 22);                                      // washer shadow
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e9ea), cx - 7, by - 9, juce::Colour (0xff6e7076), cx + 7, by + 9, false)); g.fillEllipse (cx - 10, by - 11, 20, 20);
        g.setColour (juce::Colour (0xff25262a)); g.drawEllipse (cx - 10, by - 11, 20, 20, 1.f);
        g.setColour (juce::Colour (0xff1b1b1e)); g.fillEllipse (cx - 5, by - 6, 10, 10);
        const float tipY = on ? by - 17.f : by + 14.f;                                                                                       // lever up = on
        juce::Path lever; lever.startNewSubPath (cx - 2.6f, by - 1.f); lever.lineTo (cx + 2.6f, by - 1.f); lever.lineTo (cx + 1.6f, tipY); lever.lineTo (cx - 1.6f, tipY); lever.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff7f7f8), cx - 3, 0, juce::Colour (0xff80838a), cx + 3, 0, false)); g.fillPath (lever);
        g.setColour (juce::Colour (0xff2a2b2f)); g.strokePath (lever, juce::PathStrokeType (0.8f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx - 2, tipY - 2, juce::Colour (0xff777a80), cx + 2, tipY + 3, true)); g.fillEllipse (cx - 3.4f, tipY - 3.4f, 6.8f, 6.8f);
        g.setColour (juce::Colour (0xffe9e2cf)); g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold)));
        g.drawText (getButtonText().toUpperCase(), 0, getHeight() - 14, getWidth(), 14, juce::Justification::centred);
    }
};

// ---------- pedals ----------
class FootSwitch : public juce::Button
{
public:
    FootSwitch() : juce::Button ("footswitch") { setClickingTogglesState (true); setWantsKeyboardFocus (false); setMouseClickGrabsKeyboardFocus (false); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.f); const float off = down ? 1.5f : 0.f;
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 2.5f), 6.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeceef0), 0, r.getY(), juce::Colour (0xff7b7f87), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 6.f);   // steel bezel
        g.setColour (juce::Colour (0xff1c1d20)); g.drawRoundedRectangle (r, 6.f, 1.4f);
        auto pad = r.reduced (5.f, 4.f).translated (0, off);
        g.setGradientFill (juce::ColourGradient (over ? juce::Colour (0xff4a4c52) : juce::Colour (0xff3b3d42), 0, pad.getY(), juce::Colour (0xff17181a), 0, pad.getBottom(), false)); g.fillRoundedRectangle (pad, 4.f);   // rubber pad
        g.saveState(); juce::Path p; p.addRoundedRectangle (pad, 4.f); g.reduceClipRegion (p);
        for (float x = pad.getX() + 5; x < pad.getRight(); x += 5.f) { g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect (x, pad.getY(), 1.f, pad.getHeight()); g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRect (x + 1, pad.getY(), 1.f, pad.getHeight()); }
        g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (pad.reduced (0.5f), 4.f, 1.f);
    }
};

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
    // y of the jack centre, in this component's coordinates
    float jackY() const { return getHeight() * 0.46f; }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.f, 1.f);
        // side jacks (stick out a little, the cables plug into them)
        for (int side = 0; side < 2; ++side)
        {
            const float jx = side == 0 ? r.getX() - 4.f : r.getRight() - 2.f, jy = jackY();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdadcde), jx, jy - 8, juce::Colour (0xff5c5f65), jx, jy + 8, false)); g.fillRoundedRectangle (jx, jy - 8, 6.f, 16.f, 2.f);
            g.setColour (juce::Colours::black); g.drawRoundedRectangle (jx, jy - 8, 6.f, 16.f, 2.f, 1.f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.5f, 3.f), 7.f);
        g.setGradientFill (juce::ColourGradient (body.brighter (0.28f), 0, r.getY(), body.darker (0.45f), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 7.f);
        g.saveState(); juce::Path clip; clip.addRoundedRectangle (r, 7.f); g.reduceClipRegion (clip);
        // metallic-flake speckle
        for (int i = 0; i < 260; ++i) { unsigned h = (unsigned) i * 2654435761u; h ^= h >> 13; h *= 0x5bd1e995u; const float x = r.getX() + (h & 1023) / 1023.f * r.getWidth(), y = r.getY() + ((h >> 10) & 1023) / 1023.f * r.getHeight();
                                          g.setColour (((h >> 20) & 1 ? juce::Colours::white : juce::Colours::black).withAlpha (0.06f + 0.05f * ((h >> 21) & 3) / 3.f)); g.fillRect (x, y, 1.f, 1.f); }
        // diagonal sheen
        juce::Path sheen; sheen.startNewSubPath (r.getX(), r.getY() + r.getHeight() * 0.25f); sheen.lineTo (r.getX() + r.getWidth() * 0.55f, r.getY()); sheen.lineTo (r.getX() + r.getWidth() * 0.9f, r.getY()); sheen.lineTo (r.getX(), r.getY() + r.getHeight() * 0.62f); sheen.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.5f, r.getY() + r.getHeight() * 0.4f, false)); g.fillPath (sheen);
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.85f)); g.drawRoundedRectangle (r, 7.f, 1.6f);
        g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawRoundedRectangle (r.reduced (1.8f), 5.5f, 1.f);
        // silkscreened name
        g.setColour (ink); g.setFont (juce::Font (juce::FontOptions (juce::jmin (16.f, getWidth() * 0.105f), juce::Font::bold | juce::Font::italic)));
        g.drawText (title, getLocalBounds().withY (nameY).withHeight (18), juce::Justification::centred);
        // LED
        const bool on = foot.getToggleState(); const float lx = r.getCentreX(), ly = (float) ledY;
        if (on) { g.setColour (juce::Colour (0x44ff3a2a)); g.fillEllipse (lx - 9, ly - 9, 18, 18); }
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffff7a66) : juce::Colour (0xff6a2420), lx - 2, ly - 2, on ? juce::Colour (0xffd01810) : juce::Colour (0xff2a0c0a), lx + 3, ly + 3, true)); g.fillEllipse (lx - 4.2f, ly - 4.2f, 8.4f, 8.4f);
        g.setColour (juce::Colour (0xff111111)); g.drawEllipse (lx - 4.2f, ly - 4.2f, 8.4f, 8.4f, 1.2f);
        // bottom screws
        for (float sx : { r.getX() + 7.f, r.getRight() - 7.f }) { g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (sx - 3.2f, r.getBottom() - 10.f, 6.4f, 6.4f); g.setColour (juce::Colour (0xffb9bcc2)); g.fillEllipse (sx - 2.6f, r.getBottom() - 10.6f, 5.2f, 5.2f);
                                                    g.setColour (juce::Colour (0xff3c3e43)); g.drawLine (sx - 1.8f, r.getBottom() - 8.f, sx + 1.8f, r.getBottom() - 8.f, 1.f); }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0); r.removeFromTop (6);
        auto kr = r.removeFromTop (46); const int n = (int) knobs.size(), kw = n ? kr.getWidth() / n : 0;
        for (auto& k : knobs) k->setBounds (kr.removeFromLeft (kw).reduced (1, 0));
        nameY = kr.getBottom() + 3; ledY = nameY + 18 + 8;
        const int sy = ledY + 9, sh = std::max (24, getHeight() - sy - 10);
        foot.setBounds (juce::Rectangle<int> (10, sy, getWidth() - 20, sh));
    }
private:
    int nameY = 52, ledY = 78;
};

// The pedals with patch cables between them
class PedalBoard : public juce::Component
{
public:
    Pedal comp   { "Sustain Comp", juce::Colour (0xffd29a1c), juce::Colour (0xff231705) };
    Pedal drive  { "Blues Crunch", juce::Colour (0xff1e5fb3), juce::Colour (0xffeef3ff) };
    Pedal delay  { "Echo Delay",   juce::Colour (0xff7a3fa8), juce::Colour (0xfff6ecff) };
    Pedal reverb { "Hall Reverb",  juce::Colour (0xff1f8a6a), juce::Colour (0xffeafff6) };
    PedalBoard() { for (auto* p : all()) addAndMakeVisible (*p); }
    std::array<Pedal*, 4> all() { return { &comp, &drive, &delay, &reverb }; }

    static void plug (juce::Graphics& g, float x, float y, int dir)     // dir +1: plug body extends to the right of x
    {
        const float len = 15.f, h = 9.f, x0 = dir > 0 ? x : x - len;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f4), 0, y - h / 2, juce::Colour (0xff6a6d73), 0, y + h / 2, false)); g.fillRoundedRectangle (x0, y - h / 2, len, h, 2.f);
        g.setColour (juce::Colour (0xff2a2b2e)); g.drawRoundedRectangle (x0, y - h / 2, len, h, 2.f, 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); for (float k = 3; k < len - 2; k += 4) g.fillRect (x0 + k, y - h / 2 + 1, 1.f, h - 2);
        const float bx = dir > 0 ? x0 + len : x0 - 9.f;                                                              // rubber strain relief
        juce::Path boot; boot.addRoundedRectangle (bx, y - 6.f, 9.f, 12.f, 3.f); g.setColour (juce::Colour (0xff131315)); g.fillPath (boot);
        g.setColour (juce::Colours::white.withAlpha (0.14f)); g.drawLine (bx + 1, y - 4.5f, bx + 8, y - 4.5f, 1.f);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1d20), 0, 0, juce::Colour (0xff0d0d0f), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 8.f);
        g.setColour (juce::Colour (0xff35373d)); g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
        // pedalboard carpet texture
        g.saveState(); juce::Path cp; cp.addRoundedRectangle (r.reduced (1.f), 7.f); g.reduceClipRegion (cp);
        for (int y = 20; y < getHeight(); y += 3) { g.setColour (juce::Colours::white.withAlpha (0.018f)); g.fillRect (0.f, (float) y, r.getWidth(), 1.f); }
        g.restoreState();
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
        g.drawText ("PEDALBOARD", 14, 3, 200, 16, juce::Justification::centredLeft);

        const float y = (float) comp.getY() + comp.jackY();
        auto cable = [&] (float x0, float x1)
        {
            const float sag = 22.f + std::abs (x1 - x0) * 0.12f;
            juce::Path p; p.startNewSubPath (x0, y); p.cubicTo (x0 + (x1 - x0) * 0.25f, y + sag, x1 - (x1 - x0) * 0.25f, y + sag, x1, y);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0, 4.f));
            g.setColour (juce::Colour (0xff0b0b0c)); g.strokePath (p, juce::PathStrokeType (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xff2a2c31)); g.strokePath (p, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (-0.4f, -0.9f));
            g.setColour (juce::Colours::white.withAlpha (0.20f)); g.strokePath (p, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (-0.8f, -1.6f));
            plug (g, x0 - 1.f, y, +1); plug (g, x1 + 1.f, y, -1);
        };
        auto ps = all();
        const float inX = 22.f, outX = (float) getWidth() - 22.f;
        cable (inX, (float) ps[0]->getX() - 1.f);
        for (size_t i = 0; i + 1 < ps.size(); ++i) cable ((float) ps[i]->getRight() + 1.f, (float) ps[i + 1]->getX() - 1.f);
        cable ((float) ps[3]->getRight() + 1.f, outX);
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (10.f, juce::Font::bold)));
        g.drawText ("GUITAR", 4, (int) y + 24, 50, 12, juce::Justification::centredLeft); g.drawText ("TO AMP", getWidth() - 56, (int) y + 24, 52, 12, juce::Justification::centredRight);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (50, 0); r.removeFromTop (20); r.removeFromBottom (6);
        const int pw = 158, gap = (r.getWidth() - pw * 4) / 3;
        for (auto* p : all()) { p->setBounds (r.removeFromLeft (pw)); r.removeFromLeft (gap); }
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

// ---------- tuner ----------
// YIN pitch detection. Returns Hz, or 0 when there is no clear pitch.
inline double detectPitch (const float* x, int n, double sr)
{
    double e = 0; for (int i = 0; i < n; ++i) e += (double) x[i] * x[i];
    if (std::sqrt (e / n) < 0.002) return 0;
    const int W = n / 2, tauMax = std::min (W - 1, (int) (sr / 60.0)), tauMin = std::max (2, (int) (sr / 1000.0));
    std::vector<double> d ((size_t) tauMax + 1, 0.0), c ((size_t) tauMax + 1, 1.0);
    for (int tau = 1; tau <= tauMax; ++tau) { double s = 0; for (int j = 0; j < W; ++j) { const double v = (double) x[j] - x[j + tau]; s += v * v; } d[(size_t) tau] = s; }
    double run = 0; for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; c[(size_t) tau] = run > 0 ? d[(size_t) tau] * tau / run : 1.0; }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (c[(size_t) tau] < 0.12) { while (tau + 1 < tauMax && c[(size_t) tau + 1] < c[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 0) return 0;
    double t = best;
    if (best > 1 && best < tauMax) { const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) best + 1], den = a - 2 * b + cc; if (std::fabs (den) > 1e-12) t += 0.5 * (a - cc) / den; }
    return sr / t;
}

class TunerPanel : public juce::Component
{
public:
    juce::TextButton muteBtn { "Mute amp" }, closeBtn { "Close tuner" };
    double freq = 0; bool have = false;
    TunerPanel()
    {
        muteBtn.setClickingTogglesState (true); muteBtn.setToggleState (true, juce::dontSendNotification); addAndMakeVisible (muteBtn); addAndMakeVisible (closeBtn);
    }
    void setReading (double f) { if (f > 0) { freq = f; have = true; lastSeen = juce::Time::getMillisecondCounter(); } else if (juce::Time::getMillisecondCounter() - lastSeen > 500) have = false; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c20), 0, 0, juce::Colour (0xff0b0b0d), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 10.f);
        g.setColour (juce::Colour (0xff4a3a24)); g.drawRoundedRectangle (r.reduced (0.5f), 10.f, 1.5f);
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        double cents = 0; juce::String note = "--", oct;
        if (have) { const double m = 69 + 12 * std::log2 (freq / 440.0); const int nn = (int) std::lround (m); cents = (m - nn) * 100; note = names[((nn % 12) + 12) % 12]; oct = juce::String (nn / 12 - 1); }
        const bool inTune = have && std::fabs (cents) < 4;
        g.setColour (! have ? cDim : (inTune ? cOk : cAmber)); g.setFont (juce::Font (juce::FontOptions (96.f, juce::Font::bold)));
        g.drawText (note, 0, 14, getWidth() / 2 + 20, 100, juce::Justification::centredRight);
        g.setFont (juce::Font (juce::FontOptions (30.f, juce::Font::bold))); g.setColour (cDim); g.drawText (oct, getWidth() / 2 + 24, 56, 60, 40, juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (15.f))); g.drawText (have ? juce::String (freq, 1) + " Hz   " + (cents >= 0 ? "+" : "") + juce::String (cents, 0) + " cents" : "Play a string...", 0, 112, getWidth(), 20, juce::Justification::centred);
        // scale
        const float cx = getWidth() * 0.5f, y0 = 150.f, span = getWidth() * 0.38f;
        g.setColour (juce::Colour (0xff2a2b30)); g.fillRoundedRectangle (cx - span, y0 - 4, span * 2, 8.f, 4.f);
        g.setColour (cOk.withAlpha (0.5f)); g.fillRoundedRectangle (cx - span * 0.08f, y0 - 4, span * 0.16f, 8.f, 3.f);
        g.setColour (juce::Colour (0xff9a9ea8)); g.setFont (juce::Font (juce::FontOptions (11.f)));
        for (int c = -50; c <= 50; c += 10) { const float x = cx + span * c / 50.f; g.fillRect (x - 0.5f, y0 - (c % 50 == 0 ? 16.f : 11.f), 1.f, c % 50 == 0 ? 10.f : 6.f); }
        g.drawText ("flat", (int) (cx - span) - 10, (int) y0 + 8, 50, 14, juce::Justification::centredLeft); g.drawText ("sharp", (int) (cx + span) - 40, (int) y0 + 8, 50, 14, juce::Justification::centredRight);
        if (have) { const float x = cx + span * (float) juce::jlimit (-50.0, 50.0, cents) / 50.f; g.setColour (inTune ? cOk : cAmber); juce::Path n; n.addTriangle (x - 9, y0 + 22, x + 9, y0 + 22, x, y0 + 6); g.fillPath (n); g.fillRect (x - 1.5f, y0 - 22, 3.f, 22.f); }
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (12.f))); g.drawText ("Standard tuning:  E 82.4   A 110   D 146.8   G 196   B 246.9   E 329.6 Hz", 0, getHeight() - 56, getWidth(), 16, juce::Justification::centred);
    }
    void resized() override { muteBtn.setBounds (getWidth() / 2 - 150, getHeight() - 36, 140, 28); closeBtn.setBounds (getWidth() / 2 + 10, getHeight() - 36, 140, 28); }
private:
    juce::uint32 lastSeen = 0;
};

// ---------- cabinet / microphone view ----------
class CabinetView : public juce::Component
{
public:
    // cab uses the engine numbering: 0 = 2x12 open back, 1 = 1x12 open back, 2 = 4x12 closed back
    std::function<void()> onBack;
    std::function<void (int cab, int micType, double micPos, double micDist)> onChange;
    juce::TextButton cabBtn[3] { juce::TextButton ("1x12 open back"), juce::TextButton ("2x12 open back"), juce::TextButton ("4x12 closed back") };
    juce::TextButton micBtn[5] { juce::TextButton ("Dynamic 57"), juce::TextButton ("Ribbon 121"), juce::TextButton ("Condenser"), juce::TextButton ("Dynamic 421"), juce::TextButton ("Tube Condenser") };
    juce::TextButton backBtn { "Back to amp" };
    juce::Slider distSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    CabinetView()
    {
        for (int i = 0; i < 3; ++i) { auto& b = cabBtn[i]; b.setRadioGroupId (11); b.setClickingTogglesState (true); addAndMakeVisible (b); b.onClick = [this, i] { if (b_ok (cabBtn[i])) { cab = cabOf (i); layoutMic (true); fire(); repaint(); } }; }
        for (int i = 0; i < 5; ++i) { auto& b = micBtn[i]; b.setRadioGroupId (12); b.setClickingTogglesState (true); addAndMakeVisible (b); b.onClick = [this, i] { if (b_ok (micBtn[i])) { micType = i; fire(); repaint(); } }; }
        distSl.setRange (0, 10, 0.1); distSl.setValue (3, juce::dontSendNotification); distSl.setDoubleClickReturnValue (true, 3); distSl.setTitle ("Mic distance");
        distSl.onValueChange = [this] { micDist = distSl.getValue(); fire(); repaint(); }; addAndMakeVisible (distSl);
        addAndMakeVisible (backBtn); backBtn.onClick = [this] { if (onBack) onBack(); };
        setWantsKeyboardFocus (false);
    }
    void setState (int cab_, int micType_, double micPos_, double micDist_)
    {
        cab = cab_; micType = micType_; micDist = micDist_; micPos = micPos_;
        cabBtn[0].setToggleState (cab == 1, juce::dontSendNotification); cabBtn[1].setToggleState (cab == 0, juce::dontSendNotification); cabBtn[2].setToggleState (cab == 2, juce::dontSendNotification);
        for (int i = 0; i < 5; ++i) micBtn[i].setToggleState (i == micType, juce::dontSendNotification);
        distSl.setValue (micDist, juce::dontSendNotification);
        layoutMic (false);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e1c19), 0, 0, juce::Colour (0xff0b0a09), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 10.f);
        g.setColour (juce::Colour (0xff4a3a24)); g.drawRoundedRectangle (r.reduced (0.5f), 10.f, 1.5f);
        const auto bf = baffle();
        // cabinet box (covering) and baffle board
        auto box = bf.expanded (16.f); altui::fillTweed (g, box, 8.f, 3);
        const bool closed = cab == 2;
        g.setGradientFill (juce::ColourGradient (closed ? juce::Colour (0xff2a2724) : juce::Colour (0xffb98f55), bf.getX(), bf.getY(), closed ? juce::Colour (0xff12100f) : juce::Colour (0xff7d5a30), bf.getRight(), bf.getBottom(), false)); g.fillRoundedRectangle (bf, 3.f);
        g.saveState(); juce::Path bp; bp.addRoundedRectangle (bf, 3.f); g.reduceClipRegion (bp);
        for (int i = 0; i < (int) bf.getHeight(); i += 3) { unsigned h = (unsigned) i * 2246822519u; h ^= h >> 15; g.setColour ((closed ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f + 0.05f * ((h & 7) / 7.f))); g.fillRect (bf.getX(), bf.getY() + (float) i, bf.getWidth(), 1.f + (h & 1)); }
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (bf, 3.f, 1.5f);
        for (auto& s : speakers()) drawSpeaker (g, s.centre, s.radius);
        if (closed) { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold))); g.drawText ("CLOSED BACK", bf.toNearestInt().withHeight (16).translated (0, -36), juce::Justification::centred); }
        else { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold))); g.drawText ("OPEN BACK", bf.toNearestInt().withHeight (16).translated (0, -36), juce::Justification::centred); }
        drawMic (g);
        // right side captions
        auto rc = juce::Rectangle<int> (cx0, 0, getWidth() - cx0 - 14, getHeight());
        g.setColour (cDim); g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
        g.drawText ("CABINET", rc.getX(), 10, 200, 16, juce::Justification::centredLeft); g.drawText ("MICROPHONE", rc.getX(), 66, 200, 16, juce::Justification::centredLeft);
        g.drawText ("DISTANCE", rc.getX(), 172, 100, 16, juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (11.f))); g.drawText ("close - more bass", rc.getX(), 212, 140, 14, juce::Justification::centredLeft); g.drawText ("far - thinner", rc.getRight() - 160, 212, 160, 14, juce::Justification::centredRight);
        g.setColour (cAmber.withAlpha (0.85f)); g.setFont (juce::Font (juce::FontOptions (12.f)));
        g.drawText (juce::String ("Drag the mic over the speaker.  Centre of the cone = bright,  edge = dark.   Position ") + juce::String (micPos, 1) + " / 10", rc.getX(), 144, rc.getWidth(), 24, juce::Justification::topLeft);
    }
    void resized() override
    {
        cx0 = 520; auto rc = juce::Rectangle<int> (cx0, 0, getWidth() - cx0 - 14, getHeight());
        { int x = rc.getX(); const int bw = 150; for (int i = 0; i < 3; ++i) { cabBtn[i].setBounds (x, 28, bw, 26); x += bw + 8; } }
        { int x = rc.getX(), y = 84; for (int i = 0; i < 5; ++i) { micBtn[i].setBounds (x, y, 150, 26); x += 158; if (i == 2) { x = rc.getX(); y += 30; } } }
        distSl.setBounds (rc.getX(), 188, rc.getWidth(), 22);
        backBtn.setBounds (12, getHeight() - 32, 120, 24);
    }
    void mouseDown (const juce::MouseEvent& e) override { dragMic (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { dragMic (e.position); }
    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::CrosshairCursor; }
private:
    int cab = 1, micType = 0, cx0 = 520; double micPos = 5, micDist = 3; juce::Point<float> xy; bool haveXY = false;
    struct Spk { juce::Point<float> centre; float radius; };
    static bool b_ok (const juce::Button& b) { return b.getToggleState(); }
    static int cabOf (int btn) { return btn == 0 ? 1 : (btn == 1 ? 0 : 2); }   // button order -> engine numbering
    juce::Rectangle<float> baffle() const
    {
        const float areaW = 470.f, areaH = (float) getHeight() - 78.f; const float cx = 16.f + areaW * 0.5f, cy = 40.f + areaH * 0.5f;
        float w = 230, h = 230; if (cab == 0) { w = 400; h = 200; } else if (cab == 2) { w = 250; h = 250; }
        h = std::min (h, areaH); if (cab != 0) w = h; return juce::Rectangle<float> (w, h).withCentre ({ cx, cy });
    }
    std::vector<Spk> speakers() const
    {
        const auto b = baffle(); std::vector<Spk> v;
        if (cab == 1) v.push_back ({ b.getCentre(), std::min (b.getWidth(), b.getHeight()) * 0.5f - 12.f });
        else if (cab == 0) { const float R = b.getHeight() * 0.5f - 12.f; v.push_back ({ { b.getX() + b.getWidth() * 0.25f, b.getCentreY() }, R }); v.push_back ({ { b.getX() + b.getWidth() * 0.75f, b.getCentreY() }, R }); }
        else { const float R = b.getWidth() * 0.25f - 8.f; for (int i = 0; i < 4; ++i) v.push_back ({ { b.getX() + b.getWidth() * (0.25f + 0.5f * (i % 2)), b.getY() + b.getHeight() * (0.25f + 0.5f * (i / 2)) }, R }); }
        return v;
    }
    Spk nearest (juce::Point<float> p) const { auto sp = speakers(); Spk best = sp[0]; float bd = 1e9f; for (auto& s : sp) { const float d = s.centre.getDistanceFrom (p); if (d < bd) { bd = d; best = s; } } return best; }
    double posFromXY() const { const auto s = nearest (xy); return juce::jlimit (0.0, 10.0, 10.0 * (1.0 - (double) (s.centre.getDistanceFrom (xy) / s.radius))); }
    void layoutMic (bool keepRadius)
    {
        const auto sp = speakers(); const auto s = sp[0];
        const double pos = keepRadius && haveXY ? posFromXY() : micPos; const float rr = (float) (1.0 - pos / 10.0) * s.radius;
        xy = { s.centre.x + std::cos (-0.75f) * rr, s.centre.y + std::sin (-0.75f) * rr }; haveXY = true; micPos = pos;
    }
    void dragMic (juce::Point<float> p)
    {
        const auto bf = baffle().reduced (4.f); xy = { juce::jlimit (bf.getX(), bf.getRight(), p.x), juce::jlimit (bf.getY(), bf.getBottom(), p.y) }; haveXY = true;
        micPos = posFromXY(); fire(); repaint();
    }
    void fire() { if (onChange) onChange (cab, micType, micPos, micDist); }

    static void drawSpeaker (juce::Graphics& g, juce::Point<float> c, float R)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (c.x - R - 2, c.y - R + 3, (R + 2) * 2, (R + 2) * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d9096), c.x - R, c.y - R, juce::Colour (0xff3a3c40), c.x + R, c.y + R, false)); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);   // steel frame
        g.setColour (juce::Colour (0xff111214)); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.4f);
        for (int i = 0; i < 8; ++i) { const float a = i * juce::MathConstants<float>::twoPi / 8 + 0.2f; const float x = c.x + std::cos (a) * R * 0.93f, y = c.y + std::sin (a) * R * 0.93f;
                                       g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillEllipse (x - 3.2f, y - 3.2f, 6.4f, 6.4f); g.setColour (juce::Colour (0xffc6c9cf)); g.fillEllipse (x - 2.2f, y - 2.4f, 4.4f, 4.4f); }
        const float Rs = R * 0.86f;                                                                                                                                         // rubber surround
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2a28), c.x - Rs, c.y - Rs, juce::Colour (0xff0e0e0d), c.x + Rs, c.y + Rs, false)); g.fillEllipse (c.x - Rs, c.y - Rs, Rs * 2, Rs * 2);
        const float Rc = R * 0.78f;                                                                                                                                         // paper cone
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8c9a6), c.x - Rc * 0.6f, c.y - Rc * 0.7f, juce::Colour (0xff6b5a3c), c.x + Rc * 0.8f, c.y + Rc * 0.9f, true)); g.fillEllipse (c.x - Rc, c.y - Rc, Rc * 2, Rc * 2);
        for (int i = 1; i < 14; ++i) { const float rr = Rc * (0.30f + 0.70f * i / 14.f); g.setColour (juce::Colours::black.withAlpha (0.08f + 0.05f * (i % 2))); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.f); }
        const float Rd = R * 0.27f;                                                                                                                                         // dust cap
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b4a30), c.x - Rd * 0.5f, c.y - Rd * 0.6f, juce::Colour (0xff1c150c), c.x + Rd, c.y + Rd, false)); g.fillEllipse (c.x - Rd, c.y - Rd, Rd * 2, Rd * 2);
        g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillEllipse (c.x - Rd * 0.55f, c.y - Rd * 0.8f, Rd * 0.9f, Rd * 0.4f);
    }
    void drawMic (juce::Graphics& g) const
    {
        struct Look { juce::Colour body, head, band; float w, len, headR; } looks[5] = {
            { juce::Colour (0xff6c6f75), juce::Colour (0xff2a2b2e), juce::Colour (0xff111113), 20.f, 110.f, 12.f },   // dynamic 57
            { juce::Colour (0xff1a1a1c), juce::Colour (0xffc7c9cc), juce::Colour (0xff77797e), 24.f, 130.f, 13.f },   // ribbon
            { juce::Colour (0xffcdbb95), juce::Colour (0xffd9d3c3), juce::Colour (0xff6e6044), 26.f, 120.f, 14.f },   // condenser
            { juce::Colour (0xff1a1a1c), juce::Colour (0xffb7babf), juce::Colour (0xff3a3b3e), 24.f, 116.f, 13.f },   // 421
            { juce::Colour (0xffb9b4a6), juce::Colour (0xffd8d2c4), juce::Colour (0xff4b4639), 30.f, 140.f, 16.f } }; // tube
        auto L = looks[juce::jlimit (0, 4, micType)]; L.len *= 0.78f;
        const juce::Point<float> dir (0.8f, -0.6f), perp (0.6f, 0.8f);           // body trails up and to the right
        auto at = [&] (float along, float across) { return juce::Point<float> (xy.x + dir.x * along + perp.x * across, xy.y + dir.y * along + perp.y * across); };
        juce::Path body; body.startNewSubPath (at (10.f, -L.w * 0.5f)); body.lineTo (at (L.len, -L.w * 0.5f)); body.lineTo (at (L.len, L.w * 0.5f)); body.lineTo (at (10.f, L.w * 0.5f)); body.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillPath (body, juce::AffineTransform::translation (5.f, 7.f));
        g.setGradientFill (juce::ColourGradient (L.body.brighter (0.4f), at (60.f, -L.w * 0.5f), L.body.darker (0.5f), at (60.f, L.w * 0.5f), false)); g.fillPath (body);
        g.setColour (L.band); { juce::Path ring; ring.startNewSubPath (at (L.len * 0.55f, -L.w * 0.5f)); ring.lineTo (at (L.len * 0.55f, L.w * 0.5f)); g.strokePath (ring, juce::PathStrokeType (5.f)); }
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.strokePath (body, juce::PathStrokeType (1.2f));
        juce::Path cable; cable.startNewSubPath (at (L.len, 0)); cable.quadraticTo (at (L.len + 40.f, 30.f), at (L.len + 70.f, 8.f)); g.setColour (juce::Colours::black); g.strokePath (cable, juce::PathStrokeType (4.f));
        const auto hc = at (2.f, 0.f);                                                                                         // capsule head, tip at the cone
        g.setGradientFill (juce::ColourGradient (L.head.brighter (0.3f), hc.x - L.headR * 0.5f, hc.y - L.headR * 0.5f, L.head.darker (0.6f), hc.x + L.headR, hc.y + L.headR, true)); g.fillEllipse (hc.x - L.headR, hc.y - L.headR, L.headR * 2, L.headR * 2);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); for (int i = -2; i <= 2; ++i) { g.drawLine (hc.x - L.headR * 0.7f, hc.y + i * 4.f, hc.x + L.headR * 0.7f, hc.y + i * 4.f, 1.f); }
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (hc.x - L.headR, hc.y - L.headR, L.headR * 2, L.headR * 2, 1.2f);
        g.setColour (juce::Colour (0xffff4a38).withAlpha (0.9f)); g.drawEllipse (xy.x - 3.f, xy.y - 3.f, 6.f, 6.f, 1.6f);   // the exact aim point
    }
};
} // namespace altui
