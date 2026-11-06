// The main window: audio setup, voicing tabs, tweed amp, noise control, pedals, recorder.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Amp.h"
#include <BinaryData.h>

class MainComponent : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    static constexpr int DesignW = 1260, DesignH = 975;

    MainComponent()
    {
        setLookAndFeel (&look);
        juce::PropertiesFile::Options po; po.applicationName = "ArielSignatureAmpRack"; po.filenameSuffix = ".settings"; po.osxLibrarySubFolder = "Application Support";
        po.folderName = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ArielSignatureAmpRack").getFullPathName();
        props.setStorageParameters (po);

        std::unique_ptr<juce::XmlElement> saved (props.getUserSettings()->getXmlValue ("audioDevice"));
        dm.initialise (2, 2, saved.get(), true, {}, nullptr);
        if (saved == nullptr) pickBestDriver();
        dm.addAudioCallback (&rig);
        dm.addChangeListener (this);

        buildUi();
        loadSettings();
        pushParams(); applyInput(); refreshDeviceUi();
        setWantsKeyboardFocus (true);
        setSize (DesignW, DesignH);
        startTimerHz (30);
        juce::MessageManager::callAsync ([sp = SafePointer<MainComponent> (this)] { if (sp != nullptr) sp->grabKeyboardFocus(); });
    }
    ~MainComponent() override
    {
        saveSettings();
        stopTimer(); dm.removeChangeListener (this); dm.removeAudioCallback (&rig); setLookAndFeel (nullptr);
    }

    // ---------- keyboard: Left / Right step through the voicing tabs ----------
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (tunerOpen) return false;
        if (k == juce::KeyPress::leftKey)  { stepVoicing (-1); return true; }
        if (k == juce::KeyPress::rightKey) { stepVoicing (+1); return true; }
        return false;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (! tunerOpen && ampBox.contains (e.getPosition()) && ! plate.contains (e.getPosition())) setCabinetView (true);
    }
    juce::MouseCursor getMouseCursor() override
    {
        const auto p = getMouseXYRelative();
        return (! tunerOpen && ampBox.contains (p) && ! plate.contains (p)) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor;
    }
    void setCabinetView (bool on)
    {
        cabView.setVisible (on); if (on) cabView.toFront (false);
        cabView.setState (P.cab, P.micType, P.mic, P.micDist); grabKeyboardFocus();
    }

    // the signature font (Sacramento, SIL Open Font License) is built into the app
    static juce::Font scriptFont (float h)
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::SacramentoRegular_ttf, (size_t) BinaryData::SacramentoRegular_ttfSize);
        return juce::Font (juce::FontOptions (tf).withHeight (h));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (altui::cBg);
        // header
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff25201a), 0, 0, juce::Colour (0xff0e0c0a), 0, 46, false)); g.fillRect (0, 0, getWidth(), 46);
        g.setColour (juce::Colour (0xff4a3a24)); g.fillRect (0, 45, getWidth(), 1);

        // amp: covered cabinet (tint follows the voicing) with a dark control plate
        altui::fillTweed (g, ampBox.toFloat(), 10.f, voicing < 0 ? 6 : voicing);
        altui::drawPlate (g, plate.toFloat());
        for (float rx : { plate.getX() + 9.f, plate.getRight() - 9.f }) for (float ry : { plate.getY() + 9.f, plate.getBottom() - 9.f }) altui::drawRivet (g, rx, ry);
        // signature, slightly tilted, where the power lamp would be
        { g.saveState(); const float cx = (float) plate.getRight() - 140.f, cy = (float) plate.getBottom() - 34.f;
          g.addTransform (juce::AffineTransform::rotation (-0.08f, cx, cy));
          g.setFont (scriptFont (40.f)); g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawText ("by Ariel Gragnani", (int) cx - 170 + 1, (int) cy - 26 + 2, 340, 52, juce::Justification::centred);
          g.setColour (juce::Colour (0xfff0e4c4)); g.drawText ("by Ariel Gragnani", (int) cx - 170, (int) cy - 26, 340, 52, juce::Justification::centred);
          g.restoreState(); }
        g.setColour (juce::Colour (0x88f0e4c4)); g.setFont (juce::Font (juce::FontOptions (11.f)));
        g.drawText ("click the tweed for the cabinet and microphone", ampBox.getX(), ampBox.getBottom() - 19, ampBox.getWidth(), 16, juce::Justification::centred);

        // other panels
        for (auto r : { noiseBox, recBox })
        {
            g.setColour (altui::cPanel); g.fillRoundedRectangle (r.toFloat(), 8.f);
            g.setColour (altui::cLine); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.f, 1.f);
        }
        g.setColour (altui::cDim); g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
        g.drawText ("RECORDER", recBox.getX() + 14, recBox.getY() + 4, 300, 18, juce::Justification::centredLeft);
        g.setColour (beatLit > 0 ? (beatAccent ? altui::cRed : altui::cAmber) : altui::cLine);
        g.fillEllipse ((float) bpmFlash.getX(), (float) bpmFlash.getY(), 14.f, 14.f);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (46).reduced (10, 0);
        audioBtn.setBounds (top.removeFromLeft (110).reduced (0, 9)); top.removeFromLeft (6);
        driverBox.setBounds (top.removeFromLeft (190).reduced (0, 9)); top.removeFromLeft (6);
        bufferBox.setBounds (top.removeFromLeft (110).reduced (0, 9)); top.removeFromLeft (6);
        chanBox.setBounds (top.removeFromLeft (120).reduced (0, 9)); top.removeFromLeft (6);
        gainLbl.setBounds (top.removeFromLeft (28)); gainSl.setBounds (top.removeFromLeft (110)); top.removeFromLeft (6);
        meterIn.setBounds (top.removeFromLeft (80).reduced (0, 17)); top.removeFromLeft (4); meterOut.setBounds (top.removeFromLeft (80).reduced (0, 17));
        r.reduce (12, 0);
        latencyLbl.setBounds (r.removeFromTop (22).withTrimmedLeft (4)); r.removeFromTop (2);

        tabs.setBounds (r.removeFromTop (34).withTrimmedLeft (10).withTrimmedRight (10));
        ampBox = r.removeFromTop (208); r.removeFromTop (8);
        plate = ampBox.reduced (34, 20);
        {
            auto a = plate.reduced (10, 6);
            auto row = a.removeFromTop (106);
            const int kw = row.getWidth() / 5;
            for (auto* k : { &kGain, &kTreb, &kMid, &kBass, &kVol }) k->setBounds (row.removeFromLeft (kw).reduced (2, 0));
            a.removeFromTop (4);
            auto sw = a.removeFromTop (46).withTrimmedLeft (24);
            brightBtn.setBounds (sw.removeFromLeft (84)); sw.removeFromLeft (16);
            satBtn.setBounds (sw.removeFromLeft (104));
        }

        const int nh = noiseOpen ? 100 : 30;
        noiseBox = r.removeFromTop (nh); r.removeFromTop (8);
        noiseHeader.setBounds (noiseBox.getX() + 6, noiseBox.getY() + 2, 260, 26); noiseHeader.open = noiseOpen;
        for (auto* c : std::initializer_list<juce::Component*> { &humBtn, &humHzBox, &humAmtLbl, &humAmtSl, &humStatus, &gateBtn, &gateLbl, &gateSl, &learnBtn, &learnStatus, &noiseSampleBtn }) c->setVisible (noiseOpen);
        if (noiseOpen)
        {
            auto a = noiseBox.reduced (12, 0); a.removeFromTop (30);
            auto r1 = a.removeFromTop (34).reduced (0, 2), r2 = a.removeFromTop (32).reduced (0, 2);
            humBtn.setBounds (r1.removeFromLeft (100)); r1.removeFromLeft (6);
            humHzBox.setBounds (r1.removeFromLeft (80)); r1.removeFromLeft (10);
            humAmtLbl.setBounds (r1.removeFromLeft (58)); humAmtSl.setBounds (r1.removeFromLeft (130)); r1.removeFromLeft (30);
            gateBtn.setBounds (r1.removeFromLeft (90)); r1.removeFromLeft (8);
            gateLbl.setBounds (r1.removeFromLeft (30)); gateSl.setBounds (r1.removeFromLeft (150)); r1.removeFromLeft (20);
            learnBtn.setBounds (r1.removeFromLeft (90));
            humStatus.setBounds (r2.removeFromLeft (250)); r2.removeFromLeft (10);
            noiseSampleBtn.setBounds (r2.removeFromRight (170)); r2.removeFromRight (10);
            learnStatus.setBounds (r2);
        }

        board.setBounds (r.removeFromTop (195)); r.removeFromTop (8);

        recBox = r;
        {
            auto a = recBox.reduced (12, 0); a.removeFromTop (24);
            auto row = a.removeFromTop (34);
            playBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (6);
            recBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (6);
            stopBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (12);
            timeLbl.setBounds (row.removeFromLeft (90)); row.removeFromLeft (8);
            tunerBtn.setBounds (row.removeFromRight (80)); row.removeFromRight (8);
            mixBtn.setBounds (row.removeFromRight (100)); row.removeFromRight (8);
            statusLbl.setBounds (row);
            a.removeFromTop (6);
            row = a.removeFromTop (30);
            metBtn.setBounds (row.removeFromLeft (80)); row.removeFromLeft (6);
            countBtn.setBounds (row.removeFromLeft (84)); row.removeFromLeft (10);
            bpmFlash = row.removeFromLeft (14).withSizeKeepingCentre (14, 14); row.removeFromLeft (6);
            bpmLbl.setBounds (row.removeFromLeft (40)); bpmSl.setBounds (row.removeFromLeft (130)); row.removeFromLeft (6);
            tapBtn.setBounds (row.removeFromLeft (50)); row.removeFromLeft (10);
            beatsBox.setBounds (row.removeFromLeft (92)); row.removeFromLeft (10);
            clickLbl.setBounds (row.removeFromLeft (46)); clickSl.setBounds (row.removeFromLeft (90)); row.removeFromLeft (10);
            compLbl.setBounds (row.removeFromLeft (84)); compSl.setBounds (row.removeFromLeft (110)); row.removeFromLeft (6);
            compAuto.setBounds (row.removeFromLeft (56));
            a.removeFromTop (8);
            const int sh = juce::jlimit (50, 72, (a.getHeight() - 6 * 3) / 4);
            for (auto& s : strips) { s->setBounds (a.removeFromTop (sh)); a.removeFromTop (6); }
        }
        cabView.setBounds (tabs.getX(), tabs.getY(), tabs.getWidth(), noiseBox.getBottom() - tabs.getY());
        tunerPanel.setBounds (tabs.getX(), tabs.getY(), tabs.getWidth(), noiseBox.getBottom() - tabs.getY());
        repaint();
    }

private:
    // ---------- voicings ----------
    struct Preset { const char* name; double gain, vol, treb, mid, bass, rev, trs, tri; bool bright; int cab; double mic; int model; };
    static const Preset* presets()
    {
        static const Preset p[] = {
            // fields: name, gain, volume, treble, middle, bass, (unused x3), bright, cabinet, mic position, amp model
            { "Fender Crunch", 8,   6, 3,   6,   6,   0, 5, 0, false, 1, 2, 0 },
            { "Twin Sparkle",  3.2, 6, 5,   5,   4,   0, 5, 0, true,  0, 5, 0 },
            { "Warm Jazz",     2.8, 6, 2.5, 6,   5.5, 0, 5, 0, false, 1, 4, 0 },
            { "Blues Junior",  3.5, 6, 5,   5,   5,   0, 5, 0, false, 1, 3, 1 },
            { "Lead Stack",    6,   6, 6,   6.5, 4,   0, 5, 0, false, 2, 3, 2 },
            { "British Classic", 4, 6, 6,   3,   5,   0, 5, 0, true,  0, 3, 3 } };
        return p;
    }
    static constexpr int NumPresets = 6;
    void setModel (int mdl) { P.model = mdl; kMid.name.setText (mdl == 3 ? "CUT" : "MIDDLE", juce::dontSendNotification); }

    void applyPreset (int i)
    {
        if (i < 0 || i >= NumPresets) return;
        const auto& q = presets()[i]; loading = true;
        const auto sy = juce::sendNotificationSync;   // synchronous, so these changes do not count as hand edits (which would drop the selected tab)
        kGain.s.setValue (q.gain, sy); kVol.s.setValue (q.vol, sy); kTreb.s.setValue (q.treb, sy); kMid.s.setValue (q.mid, sy); kBass.s.setValue (q.bass, sy);
        P.bright = q.bright; brightBtn.setToggleState (q.bright, juce::dontSendNotification); P.sat = false; satBtn.setToggleState (false, juce::dontSendNotification);
        setModel (q.model);
        P.cab = q.cab; P.mic = q.mic; cabView.setState (P.cab, P.micType, P.mic, P.micDist);
        loading = false; voicing = i; tabs.setSelected (i); repaint (ampBox); pushParams();
    }
    void stepVoicing (int d)
    {
        const int next = voicing < 0 ? (d > 0 ? 0 : NumPresets - 1) : (voicing + d + NumPresets) % NumPresets;
        applyPreset (next);
    }
    void markCustom() { if (! loading && voicing != -1) { voicing = -1; tabs.setSelected (-1); repaint (ampBox); } }

    // ---------- state ----------
    altui::AltLook look;
    juce::ApplicationProperties props;
    juce::AudioDeviceManager dm;
    alt::RigAudio rig;
    alt::Params P;
    juce::Rectangle<int> ampBox, plate, noiseBox, recBox, bpmFlash;
    int beatLit = 0; bool beatAccent = false; int voicing = -1;
    std::vector<juce::int64> taps;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastLearnDone = 0, lastState = 0, shownChannels = -1;
    bool loading = false, noiseOpen = false, inDeviceRefresh = false, tunerOpen = false;
    int tunerTick = 0;

    juce::TextButton audioBtn { "Audio settings" };
    juce::ComboBox driverBox, bufferBox, chanBox; juce::Label gainLbl { {}, "In" };
    juce::Slider gainSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    altui::LevelMeter meterIn, meterOut; juce::Label latencyLbl;
    altui::VoicingTabs tabs;
    altui::Knob kGain { "GAIN", 0, 10, 6, 1 }, kVol { "VOLUME", 0, 10, 6, 1 }, kTreb { "TREBLE", 0, 10, 6, 1 }, kMid { "MIDDLE", 0, 10, 4, 1 }, kBass { "BASS", 0, 10, 4, 1 };
    altui::ToggleSwitch brightBtn { "Bright" }, satBtn { "Saturation" }; altui::CabinetView cabView; juce::TextButton mixBtn { "Export mix" };
    altui::FoldHeader noiseHeader;
    juce::TextButton noiseSampleBtn { "Save noise sample" }, tunerBtn { "Tuner" };
    altui::TunerPanel tunerPanel;
    juce::TextButton humBtn { "De-hum" }; juce::ComboBox humHzBox; juce::Label humAmtLbl { {}, "Amount" }; juce::Slider humAmtSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label humStatus, gateLbl { {}, "Thr" }, learnStatus; juce::TextButton gateBtn { "Gate" }, learnBtn { "Learn" }; juce::Slider gateSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    altui::PedalBoard board;
    juce::TextButton playBtn { "Play" }, recBtn { "Record" }, stopBtn { "Stop" }, metBtn { "Metronome" }, countBtn { "Count-in" }, tapBtn { "Tap" }, compAuto { "Auto" };
    juce::Label timeLbl, statusLbl, bpmLbl, clickLbl { {}, "Click" }, compLbl;
    juce::Slider bpmSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, clickSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, compSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox beatsBox;
    std::vector<std::unique_ptr<altui::TrackStrip>> strips;

    // ---------- helpers ----------
    void pickBestDriver()
    {
        for (auto* wanted : { "ASIO", "Windows Audio (Exclusive Mode)", "Windows Audio (Low Latency Mode)", "Windows Audio" })
            for (auto* t : dm.getAvailableDeviceTypes())
                if (t->getTypeName() == wanted && ! t->getDeviceNames (true).isEmpty() && ! t->getDeviceNames (false).isEmpty())
                {
                    dm.setCurrentAudioDeviceType (t->getTypeName(), true);
                    auto setup = dm.getAudioDeviceSetup(); setup.bufferSize = 0; dm.setAudioDeviceSetup (setup, true);
                    return;
                }
    }

    void styleLabel (juce::Label& l, float sz = 13.f, juce::Colour c = altui::cDim, juce::Justification j = juce::Justification::centredLeft)
    { l.setFont (juce::Font (juce::FontOptions (sz))); l.setColour (juce::Label::textColourId, c); l.setJustificationType (j); }

    void toggle (juce::Button& b, bool on, std::function<void()> f)
    { b.setClickingTogglesState (true); b.setToggleState (on, juce::dontSendNotification); b.onClick = [this, f] { f(); markCustom(); if (! loading) pushParams(); }; }

    void startNoiseSample() { statusLbl.setText ("Recording 5 seconds of noise - don't play...", juce::dontSendNotification); rig.startDiag (5.0); }
    static void noFocus (juce::Component& c) { c.setWantsKeyboardFocus (false); c.setMouseClickGrabsKeyboardFocus (false); for (auto* ch : c.getChildren()) noFocus (*ch); }

    void pedalsChanged()
    {
        auto& c = board.comp; auto& d = board.drive; auto& l = board.delay; auto& v = board.reverb;
        P.cmpOn = c.foot.getToggleState(); P.cmpSustain = c.knobs[0]->s.getValue(); P.cmpAttack = c.knobs[1]->s.getValue(); P.cmpRelease = c.knobs[2]->s.getValue(); P.cmpLevel = c.knobs[3]->s.getValue();
        P.drvOn = d.foot.getToggleState(); P.drvDrive = d.knobs[0]->s.getValue(); P.drvTone = d.knobs[1]->s.getValue(); P.drvLevel = d.knobs[2]->s.getValue();
        P.dlyOn = l.foot.getToggleState(); P.dlyTime = l.knobs[0]->s.getValue(); P.dlyFb = l.knobs[1]->s.getValue(); P.dlyMix = l.knobs[2]->s.getValue(); P.dlyTone = l.knobs[3]->s.getValue(); P.dlyType = l.typeIdx;
        { auto& h = board.chorus; P.choOn = h.foot.getToggleState(); P.choRate = h.knobs[0]->s.getValue(); P.choDepth = h.knobs[1]->s.getValue(); P.choMix = h.knobs[2]->s.getValue(); }
        for (int i = 0; i < 5; ++i) P.order[i] = board.ord[(size_t) i];
        P.rvbOn = v.foot.getToggleState(); P.rvbSize = v.knobs[0]->s.getValue(); P.rvbTone = v.knobs[1]->s.getValue(); P.rvbMix = v.knobs[2]->s.getValue(); P.rvbType = v.typeIdx;
        if (! loading) pushParams();
    }

    void buildUi()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &audioBtn, &driverBox, &bufferBox, &chanBox, &gainLbl, &gainSl, &meterIn, &meterOut, &latencyLbl, &tabs,
                                                                  &kGain, &kVol, &kTreb, &kMid, &kBass, &mixBtn,
                                                                  &brightBtn, &satBtn, &noiseHeader, &humBtn, &humHzBox, &humAmtLbl, &humAmtSl, &humStatus, &gateBtn, &gateLbl, &gateSl,
                                                                  &learnBtn, &learnStatus, &noiseSampleBtn, &tunerBtn, &board, &playBtn, &recBtn, &stopBtn, &metBtn, &countBtn, &tapBtn, &compAuto, &timeLbl, &statusLbl, &bpmLbl, &bpmSl,
                                                                  &clickLbl, &clickSl, &compLbl, &compSl, &beatsBox })
            addAndMakeVisible (c);
        for (auto* l : { &gainLbl, &humAmtLbl, &gateLbl, &clickLbl, &compLbl, &bpmLbl, &latencyLbl, &humStatus, &learnStatus, &statusLbl }) styleLabel (*l);
        styleLabel (timeLbl, 20.f, altui::cAmber, juce::Justification::centredLeft); timeLbl.setFont (juce::Font (juce::FontOptions (22.f, juce::Font::bold)));
        timeLbl.setText ("0:00.0", juce::dontSendNotification);

        // device bar
        audioBtn.onClick = [this]
        {
            auto* sel = new juce::AudioDeviceSelectorComponent (dm, 0, 2, 0, 2, false, false, true, false);
            sel->setSize (560, 420);
            juce::DialogWindow::LaunchOptions o; o.content.setOwned (sel); o.dialogTitle = "Audio settings"; o.componentToCentreAround = this;
            o.dialogBackgroundColour = altui::cPanel; o.escapeKeyTriggersCloseButton = true; o.useNativeTitleBar = true; o.resizable = false;
            o.launchAsync();
        };
        driverBox.setTooltip ("Audio driver. ASIO or Exclusive Mode give the lowest delay.");
        driverBox.onChange = [this]
        {
            if (inDeviceRefresh) return; const auto name = driverBox.getText();
            dm.setCurrentAudioDeviceType (name, true);
            auto setup = dm.getAudioDeviceSetup(); setup.bufferSize = 0; dm.setAudioDeviceSetup (setup, true);
        };
        bufferBox.setTextWhenNothingSelected ("Buffer"); driverBox.setTextWhenNothingSelected ("Driver"); chanBox.setTextWhenNothingSelected ("Input");
        bufferBox.setTooltip ("Smaller buffer = less delay. If you hear crackles, pick the next bigger one.");
        bufferBox.onChange = [this]
        {
            if (inDeviceRefresh) return; const int sz = bufferBox.getSelectedId(); if (sz <= 0) return;
            auto setup = dm.getAudioDeviceSetup(); setup.bufferSize = sz; dm.setAudioDeviceSetup (setup, true);
        };
        chanBox.onChange = [this] { applyInput(); };
        gainSl.setRange (-12, 24, 0.5); gainSl.setValue (0, juce::dontSendNotification); gainSl.setDoubleClickReturnValue (true, 0); gainSl.onValueChange = [this] { applyInput(); };
        gainSl.setTitle ("Input level"); gainSl.setTooltip ("Input level from your interface (not the amp's Gain knob)");

        // voicing tabs
        { juce::StringArray n; for (int i = 0; i < NumPresets; ++i) n.add (presets()[i].name); tabs.setNames (n); tabs.onSelect = [this] (int i) { applyPreset (i); }; }

        // amp knobs
        auto bindKnob = [this] (altui::Knob& k, double& dst) { k.s.onValueChange = [this, &k, &dst] { dst = k.s.getValue(); markCustom(); if (! loading) pushParams(); }; };
        bindKnob (kGain, P.gain); bindKnob (kVol, P.outVol); bindKnob (kTreb, P.treb); bindKnob (kMid, P.mid); bindKnob (kBass, P.bass);;
        P.gain = 6; P.vol = 4; P.treb = 6; P.mid = 4; P.bass = 4; P.rev = 0; P.trs = 5; P.tri = 0; P.outVol = 6; P.cabOn = true;   // spring reverb, tremolo and output trim are gone; the cabinet is always in the signal path
        for (auto* k : { &kGain, &kVol, &kTreb, &kMid, &kBass }) k->setInk (juce::Colour (0xffe9e2cf), altui::cAmber);

        toggle (brightBtn, true, [this] { P.bright = brightBtn.getToggleState(); });
        toggle (satBtn, false, [this] { P.sat = satBtn.getToggleState(); });
        addChildComponent (cabView);
        cabView.onBack = [this] { setCabinetView (false); };
        cabView.onChange = [this] (int cab, int mt, double pos, double dist)
        { P.cab = cab; P.micType = mt; P.mic = pos; P.micDist = dist; markCustom(); if (! loading) pushParams(); };
        mixBtn.onClick = [this] { exportMix(); };

        // noise control (collapsible)
        noiseHeader.text = "NOISE CONTROL"; noiseHeader.open = noiseOpen; noiseHeader.onToggle = [this] { noiseOpen = noiseHeader.open; resized(); };
        toggle (humBtn, true, [this] { P.humOn = humBtn.getToggleState(); });
        humHzBox.addItem ("60 Hz", 1); humHzBox.addItem ("50 Hz", 2); humHzBox.setSelectedId (1, juce::dontSendNotification);
        humHzBox.onChange = [this] { P.humHz = humHzBox.getSelectedId() == 2 ? 50 : 60; if (! loading) pushParams(); };
        humAmtSl.setRange (0, 10, 0.1); humAmtSl.setValue (10, juce::dontSendNotification); humAmtSl.setDoubleClickReturnValue (true, 10);
        humAmtSl.onValueChange = [this] { P.humAmt = humAmtSl.getValue(); if (! loading) pushParams(); }; humAmtSl.setTitle ("De-hum amount");
        toggle (gateBtn, true, [this] { P.gateOn = gateBtn.getToggleState(); });
        gateSl.setRange (-90, -20, 1); gateSl.setValue (-58, juce::dontSendNotification); gateSl.setDoubleClickReturnValue (true, -58);
        gateSl.onValueChange = [this] { P.gateThr = gateSl.getValue(); if (! loading) pushParams(); }; gateSl.setTitle ("Gate threshold");
        learnBtn.onClick = [this] { learnStatus.setText ("Mute the strings...", juce::dontSendNotification); rig.requestLearn(); };
        noiseSampleBtn.onClick = [this] { startNoiseSample(); };
        noiseSampleBtn.setTooltip ("Records 5 seconds of your raw guitar signal (play nothing, strings muted) and saves it so it can be sent for analysis");
        tunerBtn.onClick = [this] { setTuner (true); };
        tunerPanel.closeBtn.onClick = [this] { setTuner (false); }; addChildComponent (tunerPanel);
        learnBtn.setTooltip ("Measures the noise for 2 seconds with the strings muted and sets the gate for you");

        // pedals
        board.comp.add ("SUSTAIN", 5); board.comp.add ("ATTACK", 4); board.comp.add ("RELEASE", 5); board.comp.add ("LEVEL", 5);
        board.drive.add ("DRIVE", 5); board.drive.add ("TONE", 5); board.drive.add ("LEVEL", 5);
        board.chorus.add ("SPEED", 4); board.chorus.add ("DEPTH", 5); board.chorus.add ("MIX", 5);
        board.delay.add ("TIME", 4); board.delay.add ("REPEAT", 3); board.delay.add ("MIX", 3); board.delay.add ("TONE", 5);
        board.delay.typeNames = juce::StringArray { "Mono", "Stereo", "Ping-pong", "Tape" }; board.delay.typeIdx = 1;
        board.reverb.add ("SIZE", 4); board.reverb.add ("TONE", 5); board.reverb.add ("MIX", 3);
        board.reverb.typeNames = juce::StringArray { "Hall", "Plate", "Spring", "Room" };
        for (auto* p : board.all()) p->onChange = [this] { pedalsChanged(); };
        board.onOrder = [this] { pedalsChanged(); };

        // recorder
        playBtn.onClick = [this] { rig.cmd = 1; }; recBtn.onClick = [this] { rig.cmd = 2; }; stopBtn.onClick = [this] { rig.cmd = 3; };
        recBtn.setColour (juce::TextButton::buttonOnColourId, altui::cRed);
        metBtn.setClickingTogglesState (true); metBtn.onClick = [this] { rig.metOn = metBtn.getToggleState(); };
        countBtn.setClickingTogglesState (true); countBtn.setToggleState (true, juce::dontSendNotification); countBtn.onClick = [this] { rig.countIn = countBtn.getToggleState(); };
        bpmSl.setRange (40, 220, 1); bpmSl.setValue (100, juce::dontSendNotification); bpmSl.setDoubleClickReturnValue (true, 100);
        bpmSl.onValueChange = [this] { rig.bpm = (int) bpmSl.getValue(); bpmLbl.setText (juce::String ((int) bpmSl.getValue()), juce::dontSendNotification); };
        bpmLbl.setText ("100", juce::dontSendNotification); bpmSl.setTitle ("Tempo");
        tapBtn.onClick = [this]
        {
            const auto now = juce::Time::currentTimeMillis();
            if (! taps.empty() && now - taps.back() > 2000) taps.clear();
            taps.push_back (now); if (taps.size() > 6) taps.erase (taps.begin());
            if (taps.size() >= 2) bpmSl.setValue (juce::jlimit (40.0, 220.0, 60000.0 * (double) (taps.size() - 1) / (double) (taps.back() - taps.front())));
        };
        beatsBox.addItem ("2 beats", 2); beatsBox.addItem ("3 beats", 3); beatsBox.addItem ("4 beats", 4); beatsBox.addItem ("6 beats", 6); beatsBox.setSelectedId (4, juce::dontSendNotification);
        beatsBox.onChange = [this] { rig.beats = beatsBox.getSelectedId(); };
        clickSl.setRange (0, 1, 0.01); clickSl.setValue (0.5, juce::dontSendNotification); clickSl.onValueChange = [this] { rig.clickVol = (float) clickSl.getValue(); }; clickSl.setTitle ("Click volume");
        compSl.setRange (0, 150, 1); compSl.setValue (0, juce::dontSendNotification); compSl.setTitle ("Latency compensation");
        compSl.onValueChange = [this] { updateComp(); };
        compAuto.setTooltip ("Use the soundcard's reported round-trip latency"); compAuto.onClick = [this] { compSl.setValue (std::round (reportedLatencyMs())); };
        compLbl.setText ("Comp 0 ms", juce::dontSendNotification);

        auto getView = [this] { return viewSecs(); };
        for (int i = 0; i < alt::NumTracks; ++i)
        {
            auto s = std::make_unique<altui::TrackStrip> (rig, i, getView);
            s->wave.playheadSecs = [this] { return playheadSecs(); };
            s->wave.isLive = [this, i] { return rig.state.load() == (int) alt::Transport::Rec && rig.armed.load() == i; };
            s->onArm = [this] (int k) { if (rig.state.load() == 0) { rig.armed = k; refreshStrips(); } };
            s->onClear = [this] (int k) { rig.clearTrack (k); refreshStrips(); };
            s->onExport = [this] (int k, bool wet) { exportTrack (k, wet); };
            addAndMakeVisible (*s); strips.push_back (std::move (s));
        }
        rig.armed = 0; refreshStrips();
        for (auto* c : getChildren()) noFocus (*c);
    }

    void pushParams() { rig.setParams (P); }

    void applyInput()
    {
        rig.setInputGainDb (gainSl.getValue());
        const int id = chanBox.getSelectedId();
        rig.setInputChannel (id <= 1 ? -1 : id - 2);
    }

    void rebuildChannels()
    {
        auto* dev = dm.getCurrentAudioDevice(); if (dev == nullptr) return;
        const auto names = dev->getInputChannelNames(); const auto active = dev->getActiveInputChannels();
        const int prev = chanBox.getSelectedId();
        chanBox.clear (juce::dontSendNotification); chanBox.addItem ("All inputs (mix)", 1);
        int n = 0; for (int i = 0; i < names.size(); ++i) if (active[i]) { chanBox.addItem (names[i], 2 + n); ++n; }
        chanBox.setSelectedId (prev > 0 && prev < 2 + n + 1 ? prev : (n >= 1 ? 2 : 1), juce::dontSendNotification);
        shownChannels = n; applyInput();
    }

    // driver and buffer-size menus follow whatever the audio device is doing
    void refreshDeviceUi()
    {
        inDeviceRefresh = true;
        driverBox.clear (juce::dontSendNotification);
        int id = 1, cur = 0; const auto curType = dm.getCurrentAudioDeviceType();
        for (auto* t : dm.getAvailableDeviceTypes()) { driverBox.addItem (t->getTypeName(), id); if (t->getTypeName() == curType) cur = id; ++id; }
        driverBox.setSelectedId (cur, juce::dontSendNotification);
        bufferBox.clear (juce::dontSendNotification);
        if (auto* dev = dm.getCurrentAudioDevice())
        {
            const double sr = dev->getCurrentSampleRate(); const int curBuf = dev->getCurrentBufferSizeSamples(); bool found = false;
            for (int b : dev->getAvailableBufferSizes()) { bufferBox.addItem (juce::String (b) + " (" + juce::String (1000.0 * b / sr, 1) + " ms)", b); found = found || b == curBuf; }
            if (! found) bufferBox.addItem (juce::String (curBuf) + " (" + juce::String (1000.0 * curBuf / sr, 1) + " ms)", curBuf);
            bufferBox.setSelectedId (curBuf, juce::dontSendNotification);
        }
        inDeviceRefresh = false; shownChannels = -1;
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshDeviceUi(); }

    double reportedLatencyMs()
    {
        auto* d = dm.getCurrentAudioDevice(); if (d == nullptr) return 0;
        return 1000.0 * (d->getInputLatencyInSamples() + d->getOutputLatencyInSamples()) / d->getCurrentSampleRate();
    }
    void updateComp()
    {
        const double sr = rig.sampleRate(); rig.compSamples = (int) std::lround (compSl.getValue() * sr / 1000.0);
        compLbl.setText ("Comp " + juce::String ((int) compSl.getValue()) + " ms", juce::dontSendNotification);
    }
    double viewSecs() { return std::max (8.0, rig.longestTrack() / std::max (1.0, rig.sampleRate()) + 0.5); }
    double playheadSecs()
    {
        if (rig.state.load() == 0) return -1;
        return std::max (0.0, rig.posOut.load() / rig.sampleRate());
    }
    void refreshStrips()
    {
        const bool rec = rig.state.load() == (int) alt::Transport::Rec;
        for (int i = 0; i < alt::NumTracks; ++i) strips[(size_t) i]->update (rig.armed.load() == i, rec);
    }

    void exportTrack (int idx, bool wet)
    {
        auto d = rig.tracks[(size_t) idx].get(); if (! d) return;
        const juce::String nm = "Track" + juce::String (idx + 1) + (wet ? "-amp" : "-dry") + ".wav";
        chooser = std::make_unique<juce::FileChooser> ("Save WAV", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (nm), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, d, wet] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            f = f.withFileExtension ("wav"); f.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
            bool ok = false;
            if (os != nullptr)
            {
                juce::WavAudioFormat wav;
                const unsigned ch = wet ? 2u : 1u;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), rig.sampleRate(), ch, 24, {}, 0));
                if (w != nullptr)
                {
                    os.release();
                    const int n = (int) d->length();
                    juce::AudioBuffer<float> buf ((int) ch, n);
                    if (wet) { buf.copyFrom (0, 0, d->wl.data(), n); buf.copyFrom (1, 0, d->wr.data(), n); }
                    else buf.copyFrom (0, 0, d->dry.data(), n);
                    ok = w->writeFromAudioSampleBuffer (buf, 0, n);
                }
            }
            statusLbl.setText (ok ? "Saved " + f.getFileName() : "Could not save the file", juce::dontSendNotification);
        });
    }

    void exportMix()
    {
        std::vector<std::shared_ptr<alt::TrackData>> ds; bool solo = false;
        for (auto& t : rig.tracks) solo = solo || t.solo.load();
        std::vector<float> vl, vr; int64_t n = 0;
        for (int i = 0; i < alt::NumTracks; ++i)
        {
            auto d = rig.tracks[(size_t) i].get(); auto& ts = rig.tracks[(size_t) i];
            if (! d || ts.mute.load() || (solo && ! ts.solo.load())) continue;
            const int len = (int) d->length(); if (len > n) { n = len; vl.resize ((size_t) n, 0.f); vr.resize ((size_t) n, 0.f); }
            const float g = ts.vol.load(), pn = ts.pan.load(), gl = pn > 0 ? 1.f - pn : 1.f, gr = pn < 0 ? 1.f + pn : 1.f;
            for (int k = 0; k < len; ++k) { vl[(size_t) k] += d->wl[(size_t) k] * g * gl; vr[(size_t) k] += d->wr[(size_t) k] * g * gr; }
        }
        if (n == 0) { statusLbl.setText ("Nothing to export - record a track first", juce::dontSendNotification); return; }
        float pk = 0.f; for (int64_t k = 0; k < n; ++k) pk = std::max ({ pk, std::fabs (vl[(size_t) k]), std::fabs (vr[(size_t) k]) });
        if (pk > 0.98f) { const float s = 0.98f / pk; for (auto& x : vl) x *= s; for (auto& x : vr) x *= s; }
        auto data = std::make_shared<std::pair<std::vector<float>, std::vector<float>>> (std::move (vl), std::move (vr));
        chooser = std::make_unique<juce::FileChooser> ("Save mix", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Mix.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, data, n] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            f = f.withFileExtension ("wav"); f.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream()); bool ok = false;
            if (os != nullptr)
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), rig.sampleRate(), 2u, 24, {}, 0));
                if (w != nullptr)
                {
                    os.release();
                    juce::AudioBuffer<float> buf (2, (int) n); buf.copyFrom (0, 0, data->first.data(), (int) n); buf.copyFrom (1, 0, data->second.data(), (int) n);
                    ok = w->writeFromAudioSampleBuffer (buf, 0, (int) n);
                }
            }
            statusLbl.setText (ok ? "Saved " + f.getFileName() : "Could not save the file", juce::dontSendNotification);
        });
    }

    void setTuner (bool on)
    {
        tunerOpen = on; tunerPanel.setVisible (on); tunerPanel.toFront (false);
        rig.muteOut = on && tunerPanel.muteBtn.getToggleState(); if (on) grabKeyboardFocus();
    }

    void saveNoiseSample()
    {
        auto raw = std::make_shared<std::vector<float>> (rig.diagRaw), wet = std::make_shared<std::vector<float>> (rig.diagWet); const double sr = rig.sampleRate();
        chooser = std::make_unique<juce::FileChooser> ("Save noise sample", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("noise-sample.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, raw, wet, sr] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) { statusLbl.setText ("Noise sample not saved", juce::dontSendNotification); return; }
            f = f.withFileExtension ("wav"); f.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream()); bool ok = false;
            if (os != nullptr)
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2u, 24, {}, 0));
                if (w != nullptr)
                {
                    os.release(); const int n = (int) raw->size(); juce::AudioBuffer<float> buf (2, n);
                    buf.copyFrom (0, 0, raw->data(), n); buf.copyFrom (1, 0, wet->data(), n); ok = w->writeFromAudioSampleBuffer (buf, 0, n);
                }
            }
            statusLbl.setText (ok ? "Saved " + f.getFileName() + " - attach it in the chat" : "Could not save the file", juce::dontSendNotification);
        });
    }

    // ---------- timer ----------
    void timerCallback() override
    {
        auto* dev = dm.getCurrentAudioDevice();
        if (dev != nullptr)
        {
            const int n = dev->getActiveInputChannels().countNumberOfSetBits();
            if (n != shownChannels) rebuildChannels();
            const double sr = dev->getCurrentSampleRate();
            const double inMs = 1000.0 * dev->getInputLatencyInSamples() / sr, outMs = 1000.0 * dev->getOutputLatencyInSamples() / sr;
            latencyLbl.setText (juce::String ((int) sr) + " Hz   |   buffer " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples   |   reported round trip " + juce::String (inMs + outMs, 1)
                                + " ms   |   Left / Right arrows switch voicing", juce::dontSendNotification);
            if (compLbl.getText() == "Comp 0 ms" && compSl.getValue() == 0) updateComp();
        }
        else latencyLbl.setText ("No audio device - open Audio settings", juce::dontSendNotification);

        if (rig.diagDone.exchange (false)) saveNoiseSample();
        if (tunerOpen)
        {
            rig.muteOut = tunerPanel.muteBtn.getToggleState();
            if ((++tunerTick & 1) == 0) { std::vector<float> w (8192); rig.copyTunerWindow (w.data(), 8192); tunerPanel.setReading (altui::detectPitch (w.data(), 8192, rig.sampleRate())); }
        }
        meterIn.setLevel (rig.peakIn.load()); meterOut.setLevel (rig.peakOut.load());

        const int bf = rig.beatFlash.exchange (0);
        if (bf) { beatLit = 4; beatAccent = bf == 2; repaint (bpmFlash.expanded (4)); }
        else if (beatLit > 0) { --beatLit; repaint (bpmFlash.expanded (4)); }

        if (rig.takeReady.load() >= 0)
        {
            const bool empty = rig.takeEmpty.load();
            rig.commitTake();
            statusLbl.setText (empty ? "Take too short - not kept" : "Take saved on track " + juce::String (rig.armed.load() + 1), juce::dontSendNotification);
            refreshStrips();
        }
        rig.collectGarbage();

        const int st = rig.state.load();
        recBtn.setToggleState (st == (int) alt::Transport::Rec, juce::dontSendNotification);
        playBtn.setToggleState (st == (int) alt::Transport::Play, juce::dontSendNotification);
        if (st != lastState) { lastState = st; refreshStrips(); if (st == (int) alt::Transport::Rec) statusLbl.setText ("Recording on track " + juce::String (rig.armed.load() + 1), juce::dontSendNotification); }
        if (st != 0)
        {
            const double t = rig.posOut.load() / rig.sampleRate();
            timeLbl.setText ((t < 0 ? "-" : "") + juce::String ((int) (std::abs (t) / 60)) + ":" + juce::String (std::fmod (std::abs (t), 60.0), 1).paddedLeft ('0', 4), juce::dontSendNotification);
        }
        for (auto& s : strips) s->wave.repaint();

        if (noiseOpen)
        {
            const float hl = rig.getEngine().humLevelDb.load();
            humStatus.setText (P.humOn ? (hl > -100.f ? "Hum removed: " + juce::String (hl, 0) + " dB @ " + juce::String (rig.getEngine().humHzOut.load(), 1) + " Hz" : "No hum detected") : "De-hum off", juce::dontSendNotification);
        }
        auto& e = rig.getEngine();
        const int ld = e.learnDone.load();
        if (ld != lastLearnDone)
        {
            lastLearnDone = ld;
            if (e.learnOk.load())
            {
                gateSl.setValue (std::round (e.learnThr.load()));
                learnStatus.setText ("Noise floor " + juce::String (e.learnLevel.load(), 0) + " dB - gate set", juce::dontSendNotification);
                if (e.learnMains.load()) { humHzBox.setSelectedId (e.humHzOut.load() < 55.f ? 2 : 1); }
            }
            else learnStatus.setText ("Too loud to learn - mute the strings and try again", juce::dontSendNotification);
        }
        else if (e.learning.load()) learnStatus.setText ("Learning... keep the strings muted", juce::dontSendNotification);
    }

    // ---------- settings ----------
    void saveSettings()
    {
        auto* s = props.getUserSettings(); if (s == nullptr) return;
        if (auto xml = dm.createStateXml()) s->setValue ("audioDevice", xml.get());
        s->setValue ("gainK2", P.gain); s->setValue ("volK2", P.outVol); s->setValue ("treb", P.treb); s->setValue ("mid", P.mid); s->setValue ("bass", P.bass); s->setValue ("rev", P.rev);
        s->setValue ("bright", P.bright); s->setValue ("sat", P.sat);
        s->setValue ("cab", P.cab); s->setValue ("mic", P.mic); s->setValue ("micType", P.micType); s->setValue ("micDist", P.micDist); s->setValue ("humOn", P.humOn); s->setValue ("humHz", P.humHz); s->setValue ("humAmt", P.humAmt);
        s->setValue ("gateOn", P.gateOn); s->setValue ("gateThr", P.gateThr); s->setValue ("gain", gainSl.getValue()); s->setValue ("chan", chanBox.getSelectedId());
        s->setValue ("bpm", bpmSl.getValue()); s->setValue ("comp", compSl.getValue()); s->setValue ("click", clickSl.getValue());
        s->setValue ("voicing2", voicing); s->setValue ("ampModel", P.model); s->setValue ("noiseOpen2", noiseOpen);
        s->setValue ("cmpOn", P.cmpOn); s->setValue ("cmpSustain", P.cmpSustain); s->setValue ("cmpAttack", P.cmpAttack); s->setValue ("cmpLevel", P.cmpLevel); s->setValue ("cmpRelease", P.cmpRelease);
        s->setValue ("drvOn", P.drvOn); s->setValue ("drvDrive", P.drvDrive); s->setValue ("drvTone", P.drvTone); s->setValue ("drvLevel", P.drvLevel);
        s->setValue ("dlyOn", P.dlyOn); s->setValue ("dlyTime", P.dlyTime); s->setValue ("dlyFb", P.dlyFb); s->setValue ("dlyMix", P.dlyMix); s->setValue ("dlyTone", P.dlyTone); s->setValue ("dlyType", P.dlyType);
        s->setValue ("choOn", P.choOn); s->setValue ("choRate", P.choRate); s->setValue ("choDepth", P.choDepth); s->setValue ("choMix", P.choMix);
        { juce::String o; for (int i = 0; i < 5; ++i) o << board.ord[(size_t) i]; s->setValue ("pedalOrder", o); }
        s->setValue ("rvbOn", P.rvbOn); s->setValue ("rvbSize", P.rvbSize); s->setValue ("rvbTone", P.rvbTone); s->setValue ("rvbMix", P.rvbMix); s->setValue ("rvbType", P.rvbType);
        s->saveIfNeeded();
    }
    void loadSettings()
    {
        auto* s = props.getUserSettings(); if (s == nullptr) return;
        loading = true;
        auto d = [s] (const char* k, double def) { return s->getDoubleValue (k, def); };
        kGain.s.setValue (d ("gainK2", 6)); kVol.s.setValue (d ("volK2", 6)); kTreb.s.setValue (d ("treb", 6)); kMid.s.setValue (d ("mid", 4)); kBass.s.setValue (d ("bass", 4));
        brightBtn.setToggleState (s->getBoolValue ("bright", true), juce::dontSendNotification); satBtn.setToggleState (s->getBoolValue ("sat", false), juce::dontSendNotification);
        P.cab = juce::jlimit (0, 2, s->getIntValue ("cab", 1)); P.mic = d ("mic", 5); P.micType = juce::jlimit (0, 4, s->getIntValue ("micType", 0)); P.micDist = d ("micDist", 3); cabView.setState (P.cab, P.micType, P.mic, P.micDist);
        humBtn.setToggleState (s->getBoolValue ("humOn", true), juce::dontSendNotification); humHzBox.setSelectedId (s->getIntValue ("humHz", 60) == 50 ? 2 : 1, juce::dontSendNotification);
        humAmtSl.setValue (d ("humAmt", 10)); gateBtn.setToggleState (s->getBoolValue ("gateOn", true), juce::dontSendNotification); gateSl.setValue (d ("gateThr", -58));
        gainSl.setValue (d ("gain", 0)); bpmSl.setValue (d ("bpm", 100)); compSl.setValue (d ("comp", 0)); clickSl.setValue (d ("click", 0.5));
        { auto& cp = board.comp; cp.foot.setToggleState (s->getBoolValue ("cmpOn", false), juce::dontSendNotification); cp.knobs[0]->s.setValue (d ("cmpSustain", 5)); cp.knobs[1]->s.setValue (d ("cmpAttack", 4)); cp.knobs[2]->s.setValue (d ("cmpRelease", 5)); cp.knobs[3]->s.setValue (d ("cmpLevel", 5)); }
        { auto& ch = board.chorus; ch.foot.setToggleState (s->getBoolValue ("choOn", false), juce::dontSendNotification); ch.knobs[0]->s.setValue (d ("choRate", 4)); ch.knobs[1]->s.setValue (d ("choDepth", 5)); ch.knobs[2]->s.setValue (d ("choMix", 5)); }
        { const auto o = s->getValue ("pedalOrder", "01234"); bool okOrd = o.length() == 5; int seen = 0; for (int i = 0; okOrd && i < 5; ++i) { const int v = o[i] - '0'; if (v < 0 || v > 4 || (seen & (1 << v))) okOrd = false; else seen |= 1 << v; }
          if (okOrd) for (int i = 0; i < 5; ++i) board.ord[(size_t) i] = o[i] - '0'; board.layoutPedals(); }
        auto& dv = board.drive; auto& dl = board.delay; auto& rv = board.reverb;
        dv.foot.setToggleState (s->getBoolValue ("drvOn", false), juce::dontSendNotification); dv.knobs[0]->s.setValue (d ("drvDrive", 5)); dv.knobs[1]->s.setValue (d ("drvTone", 5)); dv.knobs[2]->s.setValue (d ("drvLevel", 5));
        dl.foot.setToggleState (s->getBoolValue ("dlyOn", false), juce::dontSendNotification); dl.knobs[0]->s.setValue (d ("dlyTime", 4)); dl.knobs[1]->s.setValue (d ("dlyFb", 3)); dl.knobs[2]->s.setValue (d ("dlyMix", 3)); dl.knobs[3]->s.setValue (d ("dlyTone", 5)); dl.typeIdx = juce::jlimit (0, 3, s->getIntValue ("dlyType", 1));
        rv.foot.setToggleState (s->getBoolValue ("rvbOn", false), juce::dontSendNotification); rv.knobs[0]->s.setValue (d ("rvbSize", 4)); rv.knobs[1]->s.setValue (d ("rvbTone", 5)); rv.knobs[2]->s.setValue (d ("rvbMix", 3)); rv.typeIdx = juce::jlimit (0, 3, s->getIntValue ("rvbType", 0));
        noiseOpen = s->getBoolValue ("noiseOpen2", false); noiseHeader.open = noiseOpen;
        rebuildChannels(); if (s->getIntValue ("chan", 0) > 0 && s->getIntValue ("chan", 0) <= chanBox.getNumItems() + 1) chanBox.setSelectedId (s->getIntValue ("chan", 1), juce::dontSendNotification);
        P.bright = brightBtn.getToggleState(); P.sat = satBtn.getToggleState(); P.humOn = humBtn.getToggleState(); P.gateOn = gateBtn.getToggleState();
        P.humHz = humHzBox.getSelectedId() == 2 ? 50 : 60;
        pedalsChanged();
        setModel (juce::jlimit (0, 3, s->getIntValue ("ampModel", 0))); voicing = s->getIntValue ("voicing2", -1); if (voicing < 0 || voicing >= NumPresets) voicing = -1; tabs.setSelected (voicing);
        loading = false;
        updateComp(); resized();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
