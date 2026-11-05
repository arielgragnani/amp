// Look and small reusable interface pieces: rotary knobs, level meters, track strips.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "RigAudio.h"

namespace altui
{
using juce::Colour;
static const Colour cBg (0xff0b0b0d), cPanel (0xff1b1c20), cPanel2 (0xff25272c), cLine (0xff363941), cText (0xffecebe6), cDim (0xff8c909a),
                    cAmber (0xffffb347), cAmberDim (0xff7a5a2a), cRed (0xffef4a3c), cOk (0xff4fbf6a), cSilver (0xffc7cbd1), cInk (0xff14151a);

class AltLook : public juce::LookAndFeel_V4
{
public:
    AltLook()
    {
        setColour (juce::ResizableWindow::backgroundColourId, cBg);
        setColour (juce::TextButton::buttonColourId, cPanel2);
        setColour (juce::TextButton::buttonOnColourId, cAmber);
        setColour (juce::TextButton::textColourOffId, cText);
        setColour (juce::TextButton::textColourOnId, cInk);
        setColour (juce::ComboBox::backgroundColourId, cPanel2);
        setColour (juce::ComboBox::textColourId, cText);
        setColour (juce::ComboBox::outlineColourId, cLine);
        setColour (juce::ComboBox::arrowColourId, cAmber);
        setColour (juce::PopupMenu::backgroundColourId, cPanel);
        setColour (juce::PopupMenu::textColourId, cText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cAmber);
        setColour (juce::PopupMenu::highlightedTextColourId, cInk);
        setColour (juce::Label::textColourId, cText);
        setColour (juce::Slider::rotarySliderFillColourId, cAmber);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3d45));
        setColour (juce::Slider::thumbColourId, cAmber);
        setColour (juce::Slider::trackColourId, cAmber);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff0b0c0e));
        setColour (juce::Slider::textBoxTextColourId, cDim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0c0e));
        setColour (juce::TextEditor::textColourId, cAmber);
        setColour (juce::TextEditor::outlineColourId, cLine);
        setColour (juce::AlertWindow::backgroundColourId, cPanel);
        setColour (juce::AlertWindow::textColourId, cText);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const int style = (int) s.getProperties().getWithDefault ("style", 0);
        const float cx = x + w * 0.5f, cy = y + h * 0.5f, ang = a0 + pos * (a1 - a0);
        if (style == 1)            // amp: black skirted knob with a painted pointer, printed scale around it
        {
            const float d = (float) std::min (w, h), r = d * 0.31f;
            g.setFont (juce::Font (juce::FontOptions (d * 0.11f, juce::Font::bold)));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / 10.f, sn = std::sin (a), cs = -std::cos (a);
                g.setColour (juce::Colour (0xffe9e2cf).withAlpha (0.88f));
                g.drawText (juce::String (i), juce::Rectangle<float> (cx + sn * d * 0.445f - 8, cy + cs * d * 0.445f - 6, 16, 12), juce::Justification::centred);
            }
            g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillEllipse (cx - r - 1, cy - r + 3, (r + 1) * 2, (r + 1) * 2);                 // cast shadow
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3f), cx - r, cy - r, juce::Colour (0xff050506), cx + r, cy + r, false));       // skirt
            g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
            g.saveState(); juce::Path sk; sk.addEllipse (cx - r, cy - r, r * 2, r * 2); g.reduceClipRegion (sk);
            for (int i = 0; i < 36; ++i) { const float a = (float) i / 36.f * juce::MathConstants<float>::twoPi; g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawLine (cx + std::sin (a) * r * 0.80f, cy - std::cos (a) * r * 0.80f, cx + std::sin (a) * r, cy - std::cos (a) * r, 1.f); }
            g.restoreState();
            g.setColour (juce::Colour (0xff070708)); g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.2f);
            const float r2 = r * 0.74f;                                                                                                           // raised top face
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4f), cx - r2 * 0.6f, cy - r2 * 0.8f, juce::Colour (0xff0d0d0f), cx + r2 * 0.7f, cy + r2, true));
            g.fillEllipse (cx - r2, cy - r2, r2 * 2, r2 * 2);
            g.setColour (juce::Colours::white.withAlpha (0.16f)); g.drawEllipse (cx - r2 + 0.5f, cy - r2 + 0.5f, r2 * 2 - 1, r2 * 2 - 1, 1.f);
            g.setColour (juce::Colours::white.withAlpha (0.14f)); g.fillEllipse (cx - r2 * 0.55f, cy - r2 * 0.85f, r2 * 0.8f, r2 * 0.35f);       // specular
            juce::Path p; p.addRoundedRectangle (-1.7f, -r * 0.97f, 3.4f, r * 0.80f, 1.5f);
            g.setColour (juce::Colour (0xfff4f1e8)); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (cx, cy));
            return;
        }
        if (style == 2)            // pedal: small silver-edged knob with a white line
        {
            const float r = (float) std::min (w, h) * 0.5f - 2.f;
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (cx - r - 1, cy - r + 1, r * 2 + 2, r * 2 + 2);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a6e), cx, cy - r, juce::Colour (0xff17171a), cx, cy + r, false)); g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
            g.setColour (juce::Colour (0xffb8bcc2)); g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.2f);
            juce::Path p; p.addRoundedRectangle (-1.3f, -r + 1.5f, 2.6f, r * 0.62f, 1.2f);
            g.setColour (juce::Colours::white); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (cx, cy));
            return;
        }
        const float d = (float) std::min (w, h) - 8.f, r = d * 0.5f;
        const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId), ring = s.findColour (juce::Slider::rotarySliderOutlineColourId);
        juce::Path track; track.addCentredArc (cx, cy, r, r, 0.f, a0, a1, true);
        g.setColour (ring); g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path val; val.addCentredArc (cx, cy, r, r, 0.f, a0, ang, true);
        g.setColour (fill); g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float cr = r - 6.f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c43), cx, cy - cr, juce::Colour (0xff0e0f12), cx, cy + cr, false));
        g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.f);
        juce::Path p; p.addRoundedRectangle (-1.5f, -cr + 3.f, 3.f, cr * 0.5f, 1.5f);
        g.setColour (juce::Colours::white); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (cx, cy));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        Colour base = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
        if (over && ! on) base = base.brighter (0.1f);
        if (down) base = base.darker (0.15f);
        g.setGradientFill (juce::ColourGradient (base.brighter (on ? 0.25f : 0.12f), 0, r.getY(), base.darker (0.15f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 4.f, 1.f);
        if (b.hasKeyboardFocus (true)) { g.setColour (cAmber); g.drawRoundedRectangle (r.reduced (2.f), 3.f, 1.5f); }
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (15.f, h * 0.6f), juce::Font::bold)); }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float cy = y + h * 0.5f;
        g.setColour (s.findColour (juce::Slider::backgroundColourId)); g.fillRoundedRectangle ((float) x, cy - 3.f, (float) w, 6.f, 3.f);
        g.setColour (s.findColour (juce::Slider::trackColourId)); g.fillRoundedRectangle ((float) x, cy - 3.f, pos - x, 6.f, 3.f);
        g.setColour (juce::Colour (0xffe9e7df)); g.fillEllipse (pos - 7.f, cy - 7.f, 14.f, 14.f);
    }
};

// A rotary knob with a name above and the value below
class Knob : public juce::Component
{
public:
    juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label name;
    int style = 0;
    Knob (const juce::String& label, double mn, double mx, double def, int style_ = 0)
    {
        style = style_; std::function<juce::String (double)> fmt = nullptr;
        s.getProperties().set ("style", style);
        name.setText (label, juce::dontSendNotification); name.setJustificationType (juce::Justification::centred);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);
        s.setRange (mn, mx, 0.05); s.setValue (def, juce::dontSendNotification); s.setDoubleClickReturnValue (true, def);
        s.setTextBoxStyle (style == 0 ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, true, 72, 16); s.setPopupDisplayEnabled (style != 0, true, nullptr);
        s.setWantsKeyboardFocus (false); name.setFont (juce::Font (juce::FontOptions (style == 2 ? 11.f : 13.f, juce::Font::bold)));
        s.setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
        s.setMouseDragSensitivity (180); s.setScrollWheelEnabled (true); s.setTitle (label);
        if (fmt) s.textFromValueFunction = fmt; else s.setNumDecimalPlacesToDisplay (1);
        addAndMakeVisible (s);
    }
    void setInk (Colour c, Colour arc)
    {
        name.setColour (juce::Label::textColourId, c); s.setColour (juce::Slider::textBoxTextColourId, c.withAlpha (0.75f));
        s.setColour (juce::Slider::rotarySliderFillColourId, arc); s.setColour (juce::Slider::rotarySliderOutlineColourId, c.withAlpha (0.25f));
    }
    void setValueQuiet (double v) { s.setValue (v, juce::dontSendNotification); }
    void resized() override { auto r = getLocalBounds(); name.setBounds (r.removeFromTop (style == 2 ? 14 : 18)); s.setBounds (r); }
};

class LevelMeter : public juce::Component
{
public:
    void setLevel (float l)
    {
        const float db = 20.f * std::log10 (l + 1e-6f), v = juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        level = std::max (v, level * 0.85f); if (l > 0.98f) clipHold = 40; else if (clipHold > 0) --clipHold;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0b0c0e)); g.fillRoundedRectangle (r, 2.f);
        const int segs = 24, sw = (int) (r.getWidth() / segs);
        for (int i = 0; i < segs; ++i)
        {
            const float f = (float) i / segs; const bool lit = f < level;
            Colour c = f > 0.9f ? cRed : (f > 0.7f ? cAmber : cOk);
            g.setColour (lit ? c : c.withAlpha (0.12f)); g.fillRect ((float) (i * sw + 1), r.getY() + 1, (float) sw - 2, r.getHeight() - 2);
        }
        if (clipHold > 0) { g.setColour (cRed); g.fillEllipse (r.getRight() - 9, r.getY() - 2, 8, 8); }
    }
private:
    float level = 0; int clipHold = 0;
};

// Waveform of one take (or the live recording), with the playhead
class Wave : public juce::Component
{
public:
    std::function<double()> viewSecs, playheadSecs; std::function<bool()> isLive; alt::RigAudio* rig = nullptr; int index = 0;
    std::shared_ptr<const alt::TrackData> data;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (juce::Colour (0xff08090b)); g.fillRoundedRectangle (r.toFloat(), 3.f);
        const bool live = isLive && isLive();
        const float* pk = nullptr; int np = 0;
        if (live) { pk = rig->livePeaks.data(); np = rig->livePeakN.load(); }
        else if (data) { pk = data->peaks.data(); np = (int) data->peaks.size(); }
        const double sr = rig->sampleRate(), view = viewSecs ? viewSecs() : 8.0, secPerB = alt::PeakBucket / sr;
        if (np <= 0)
        {
            g.setColour (juce::Colour (0xff4a4d57)); g.setFont (12.f);
            if (! live) g.drawText ("EMPTY", r, juce::Justification::centred);
        }
        else
        {
            g.setColour (live ? cRed : cAmber);
            const int w = r.getWidth(), h = r.getHeight();
            for (int x = 0; x < w; ++x)
            {
                const int b0 = (int) std::floor ((double) x / w * view / secPerB), b1 = std::max (b0 + 1, (int) std::floor ((double) (x + 1) / w * view / secPerB));
                if (b0 >= np) break;
                float m = 0; for (int b = b0; b < b1 && b < np; ++b) m = std::max (m, pk[b]);
                const float a = std::min (1.f, std::sqrt (m)) * h * 0.46f;
                if (a > 0.4f) g.fillRect ((float) x, h * 0.5f - a, 1.f, a * 2);
            }
        }
        if (playheadSecs)
        {
            const double t = playheadSecs();
            if (t >= 0) { g.setColour (juce::Colours::white.withAlpha (0.85f)); g.fillRect ((float) (t / view * r.getWidth()), 0.f, 2.f, (float) r.getHeight()); }
        }
    }
};

class TrackStrip : public juce::Component
{
public:
    juce::TextButton arm { juce::String::fromUTF8 ("\xE2\x97\x8F") }, mute { "M" }, solo { "S" }, reamp { "Re-amp" }, exportBtn { "Export WAV" }, clear { juce::String::fromUTF8 ("\xE2\x9C\x95") };
    juce::Label name, panLbl { {}, "Pan" }, volLbl { {}, "Vol" }; juce::Slider vol { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, pan { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }; Wave wave;
    std::function<void (int, bool)> onExport; std::function<void (int)> onArm, onClear;
    TrackStrip (alt::RigAudio& r, int i, std::function<double()> view) : rig (r), idx (i)
    {
        name.setText ("Track " + juce::String (i + 1), juce::dontSendNotification); name.setColour (juce::Label::textColourId, cDim);
        name.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold)));
        arm.setClickingTogglesState (false); arm.setColour (juce::TextButton::buttonOnColourId, cRed); arm.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        arm.setTooltip ("Arm this track for recording"); arm.onClick = [this] { if (onArm) onArm (idx); };
        mute.setClickingTogglesState (true); solo.setClickingTogglesState (true); reamp.setClickingTogglesState (true);
        mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe8b53a)); mute.setTooltip ("Mute");
        solo.setTooltip ("Solo"); reamp.setTooltip ("Play the dry guitar through the current amp settings instead of the recorded sound");
        mute.onClick = [this] { rig.tracks[(size_t) idx].mute = mute.getToggleState(); };
        solo.onClick = [this] { rig.tracks[(size_t) idx].solo = solo.getToggleState(); };
        reamp.onClick = [this] { rig.tracks[(size_t) idx].reamp = reamp.getToggleState(); };
        vol.setRange (0.0, 1.5, 0.01); vol.setValue (1.0, juce::dontSendNotification); vol.setDoubleClickReturnValue (true, 1.0);
        vol.onValueChange = [this] { rig.tracks[(size_t) idx].vol = (float) vol.getValue(); };
        pan.setRange (-1.0, 1.0, 0.01); pan.setValue (0.0, juce::dontSendNotification); pan.setDoubleClickReturnValue (true, 0.0); pan.setTooltip ("Pan (double-click to centre)");
        pan.onValueChange = [this] { rig.tracks[(size_t) idx].pan = (float) pan.getValue(); };
        for (auto* l : { &panLbl, &volLbl }) { l->setFont (juce::Font (juce::FontOptions (11.f))); l->setColour (juce::Label::textColourId, cDim); l->setJustificationType (juce::Justification::centredRight); l->setInterceptsMouseClicks (false, false); }
        exportBtn.setTooltip ("Save this track as a 24-bit WAV file");
        exportBtn.onClick = [this]
        {
            juce::PopupMenu m; m.addItem (1, "Amp sound (what you heard)"); m.addItem (2, "Clean guitar (dry, for re-amping)");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this] (int r) { if (r > 0 && onExport) onExport (idx, r == 1); });
        };
        clear.onClick = [this]
        {
            if (! confirmClear) { confirmClear = true; clear.setButtonText ("Sure?"); scheduleReset(); return; }
            resetClear(); if (onClear) onClear (idx);
        };
        wave.rig = &r; wave.index = i; wave.viewSecs = std::move (view);
        for (auto* c : std::initializer_list<juce::Component*> { &arm, &name, &wave, &mute, &solo, &vol, &pan, &panLbl, &volLbl, &reamp, &exportBtn, &clear }) addAndMakeVisible (c);
    }
    void scheduleReset()
    {
        juce::Component::SafePointer<TrackStrip> safe (this);
        juce::Timer::callAfterDelay (3000, [safe] { if (safe != nullptr) safe->resetClear(); });
    }
    void resetClear() { confirmClear = false; clear.setButtonText (juce::String::fromUTF8 ("\xE2\x9C\x95")); }
    void update (bool isArmed, bool recording)
    {
        arm.setToggleState (isArmed, juce::dontSendNotification); arm.setEnabled (! recording);
        auto d = rig.tracks[(size_t) idx].get(); wave.data = d;
        reamp.setEnabled (d != nullptr && ! recording); exportBtn.setEnabled (d != nullptr); clear.setEnabled (d != nullptr && ! recording);
        wave.repaint(); repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff0f1013)); g.fillRoundedRectangle (r, 5.f);
        g.setColour (arm.getToggleState() ? juce::Colour (0xff6a1d17) : juce::Colours::black); g.drawRoundedRectangle (r, 5.f, 1.f);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 5);
        arm.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 28)); r.removeFromLeft (6);
        name.setBounds (r.removeFromLeft (70)); r.removeFromLeft (4);
        clear.setBounds (r.removeFromRight (34)); r.removeFromRight (4); exportBtn.setBounds (r.removeFromRight (92)); r.removeFromRight (4);
        reamp.setBounds (r.removeFromRight (64)); r.removeFromRight (8); vol.setBounds (r.removeFromRight (86)); volLbl.setBounds (r.removeFromRight (26)); r.removeFromRight (6);
        pan.setBounds (r.removeFromRight (76)); panLbl.setBounds (r.removeFromRight (26)); r.removeFromRight (6);
        solo.setBounds (r.removeFromRight (30)); r.removeFromRight (4); mute.setBounds (r.removeFromRight (30)); r.removeFromRight (8);
        wave.setBounds (r);
    }
private:
    alt::RigAudio& rig; int idx; bool confirmClear = false;
};
} // namespace altui
