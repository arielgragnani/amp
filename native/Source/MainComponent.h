// The main window: audio setup, voicing tabs, tweed amp, noise control, pedals, recorder.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Amp.h"

class MainComponent : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    static constexpr int DesignW = 1040, DesignH = 940;

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
        if (k == juce::KeyPress::leftKey)  { stepVoicing (-1); return true; }
        if (k == juce::KeyPress::rightKey) { stepVoicing (+1); return true; }
        return false;
    }
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (altui::cBg);
        // header
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff25201a), 0, 0, juce::Colour (0xff0e0c0a), 0, 46, false)); g.fillRect (0, 0, getWidth(), 46);
        g.setColour (juce::Colour (0xff4a3a24)); g.fillRect (0, 45, getWidth(), 1);
        g.setColour (altui::cAmber); g.setFont (juce::Font (juce::FontOptions (22.f, juce::Font::bold | juce::Font::italic)));
        g.drawText (juce::String::fromUTF8 ("Ariel\xE2\x80\x99s Signature Amp Rack"), 16, 8, 330, 30, juce::Justification::centredLeft);

        // amp: tweed cabinet with a dark control plate
        altui::fillTweed (g, ampBox.toFloat(), 10.f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (plate.toFloat().translated (0, 2), 5.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b2e), 0, (float) plate.getY(), juce::Colour (0xff131315), 0, (float) plate.getBottom(), false));
        g.fillRoundedRectangle (plate.toFloat(), 5.f);
        g.setColour (juce::Colour (0xff6c6a63)); g.drawRoundedRectangle (plate.toFloat().reduced (0.5f), 5.f, 1.2f);
        for (float rx : { plate.getX() + 8.f, plate.getRight() - 8.f }) for (float ry : { plate.getY() + 8.f, plate.getBottom() - 8.f }) altui::drawRivet (g, rx, ry);
        g.setColour (juce::Colour (0xffe9e2cf)); g.setFont (juce::Font (juce::FontOptions (17.f, juce::Font::bold | juce::Font::italic)));
        g.drawText (juce::String::fromUTF8 ("Ariel\xE2\x80\x99s Signature"), plate.getX() + 24, plate.getY() + 5, 240, 22, juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold))); g.setColour (juce::Colour (0xffa9a493));
        g.drawText ("ALT CLEAN  -  TWEED / BLACKFACE VOICING  -  1x12 / 2x12 OPEN BACK", plate.getX() + 270, plate.getY() + 7, 520, 18, juce::Justification::centredLeft);
        { const float jx = (float) plate.getRight() - 40.f, jy = (float) plate.getY() + 16.f;
          g.setColour (juce::Colour (0x66ff2a1a)); g.fillEllipse (jx - 10, jy - 10, 20, 20); g.setColour (juce::Colour (0xffe8301e)); g.fillEllipse (jx - 5, jy - 5, 10, 10);
          g.setColour (juce::Colours::black); g.drawEllipse (jx - 5, jy - 5, 10, 10, 1.f); }

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
        auto top = r.removeFromTop (46).reduced (10, 0); top.removeFromLeft (280);
        audioBtn.setBounds (top.removeFromLeft (100).reduced (0, 9)); top.removeFromLeft (6);
        driverBox.setBounds (top.removeFromLeft (170).reduced (0, 9)); top.removeFromLeft (6);
        bufferBox.setBounds (top.removeFromLeft (96).reduced (0, 9)); top.removeFromLeft (6);
        chanBox.setBounds (top.removeFromLeft (100).reduced (0, 9)); top.removeFromLeft (6);
        gainLbl.setBounds (top.removeFromLeft (28)); gainSl.setBounds (top.removeFromLeft (56)); top.removeFromLeft (6);
        meterIn.setBounds (top.removeFromLeft (52).reduced (0, 17)); top.removeFromLeft (4); meterOut.setBounds (top.removeFromLeft (52).reduced (0, 17));
        r.reduce (12, 0);
        latencyLbl.setBounds (r.removeFromTop (22).withTrimmedLeft (4)); r.removeFromTop (2);

        tabs.setBounds (r.removeFromTop (34).withTrimmedLeft (10).withTrimmedRight (10));
        ampBox = r.removeFromTop (208); r.removeFromTop (8);
        plate = ampBox.reduced (26, 14);
        {
            auto a = plate.reduced (10, 4); a.removeFromTop (26);
            auto row = a.removeFromTop (104);
            const int kw = row.getWidth() / 9;
            for (auto* k : { &kGain, &kVol, &kTreb, &kMid, &kBass, &kRev, &kTrs, &kTri, &kOut }) k->setBounds (row.removeFromLeft (kw).reduced (3, 0));
            a.removeFromTop (6);
            auto sw = a.removeFromTop (28).withTrimmedLeft (10);
            brightBtn.setBounds (sw.removeFromLeft (84)); sw.removeFromLeft (8);
            cabOnBtn.setBounds (sw.removeFromLeft (84)); sw.removeFromLeft (8);
            cabBox.setBounds (sw.removeFromLeft (150)); sw.removeFromLeft (20);
            micLbl.setBounds (sw.removeFromLeft (66)); micSl.setBounds (sw.removeFromLeft (170));
        }

        const int nh = noiseOpen ? 92 : 30;
        noiseBox = r.removeFromTop (nh); r.removeFromTop (8);
        noiseHeader.setBounds (noiseBox.getX() + 6, noiseBox.getY() + 2, 260, 26); noiseHeader.open = noiseOpen;
        for (auto* c : std::initializer_list<juce::Component*> { &humBtn, &humHzBox, &humAmtLbl, &humAmtSl, &humStatus, &gateBtn, &gateLbl, &gateSl, &learnBtn, &learnStatus }) c->setVisible (noiseOpen);
        if (noiseOpen)
        {
            auto a = noiseBox.reduced (12, 0); a.removeFromTop (34); a = a.withHeight (40);
            humBtn.setBounds (a.removeFromLeft (100)); a.removeFromLeft (6);
            humHzBox.setBounds (a.removeFromLeft (80)); a.removeFromLeft (10);
            humAmtLbl.setBounds (a.removeFromLeft (58)); humAmtSl.setBounds (a.removeFromLeft (110)); a.removeFromLeft (10);
            humStatus.setBounds (a.removeFromLeft (170)); a.removeFromLeft (14);
            gateBtn.setBounds (a.removeFromLeft (90)); a.removeFromLeft (8);
            gateLbl.setBounds (a.removeFromLeft (30)); gateSl.setBounds (a.removeFromLeft (110)); a.removeFromLeft (8);
            learnBtn.setBounds (a.removeFromLeft (90)); a.removeFromLeft (8);
            learnStatus.setBounds (a);
        }

        board.setBounds (r.removeFromTop (150)); r.removeFromTop (8);

        recBox = r;
        {
            auto a = recBox.reduced (12, 0); a.removeFromTop (24);
            auto row = a.removeFromTop (34);
            playBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (6);
            recBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (6);
            stopBtn.setBounds (row.removeFromLeft (70)); row.removeFromLeft (12);
            timeLbl.setBounds (row.removeFromLeft (90)); row.removeFromLeft (8);
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
        repaint();
    }

private:
    // ---------- voicings ----------
    struct Preset { const char* name; double gain, vol, treb, mid, bass, rev, trs, tri; bool bright; int cab; double mic; };
    static const Preset* presets()
    {
        static const Preset p[] = {
            { "Twin Sparkle",    5, 4,   5,   5,   4,   2.5, 5,   0, true,  0, 5 },
            { "Warm Jazz",       5, 3.5, 2.5, 6,   5.5, 2,   5,   0, false, 1, 4 },
            { "Surf",            5, 4.5, 6,   4,   3,   7,   5.5, 5, true,  0, 6 },
            { "Edge of Breakup", 5, 8,   5,   5.5, 4,   2,   5,   0, false, 1, 5 },
            { "Bright & Dry",    5, 3,   6.5, 4,   3.5, 0,   5,   0, true,  0, 6 },
            { "Fender Crunch",   5, 10,  3,   6,   6,   2,   5,   0, false, 1, 2 } };
        return p;
    }
    static constexpr int NumPresets = 6;

    void applyPreset (int i)
    {
        if (i < 0 || i >= NumPresets) return;
        const auto& q = presets()[i]; loading = true;
        kGain.s.setValue (q.gain); kVol.s.setValue (q.vol); kTreb.s.setValue (q.treb); kMid.s.setValue (q.mid); kBass.s.setValue (q.bass);
        kRev.s.setValue (q.rev); kTrs.s.setValue (q.trs); kTri.s.setValue (q.tri);
        P.bright = q.bright; brightBtn.setToggleState (q.bright, juce::dontSendNotification);
        cabBox.setSelectedId (q.cab + 1, juce::dontSendNotification); P.cab = q.cab; micSl.setValue (q.mic); P.mic = q.mic;
        loading = false; voicing = i; tabs.setSelected (i); pushParams();
    }
    void stepVoicing (int d)
    {
        const int next = voicing < 0 ? (d > 0 ? 0 : NumPresets - 1) : (voicing + d + NumPresets) % NumPresets;
        applyPreset (next);
    }
    void markCustom() { if (! loading && voicing != -1) { voicing = -1; tabs.setSelected (-1); } }

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
    bool loading = false, noiseOpen = true, inDeviceRefresh = false;

    juce::TextButton audioBtn { "Audio settings" };
    juce::ComboBox driverBox, bufferBox, chanBox; juce::Label gainLbl { {}, "In" };
    juce::Slider gainSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    altui::LevelMeter meterIn, meterOut; juce::Label latencyLbl;
    altui::VoicingTabs tabs;
    altui::Knob kGain { "GAIN", 0, 10, 5, 1 }, kVol { "VOLUME", 0, 10, 4, 1 }, kTreb { "TREBLE", 0, 10, 6, 1 }, kMid { "MIDDLE", 0, 10, 4, 1 }, kBass { "BASS", 0, 10, 4, 1 },
                kRev { "REVERB", 0, 10, 0, 1 }, kTrs { "SPEED", 0, 10, 5, 1 }, kTri { "DEPTH", 0, 10, 0, 1 }, kOut { "OUTPUT", 0, 10, 6, 1 };
    juce::TextButton brightBtn { "Bright" }, cabOnBtn { "Cabinet" }; juce::ComboBox cabBox;
    juce::Label micLbl { {}, "Mic pos" }; juce::Slider micSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    altui::FoldHeader noiseHeader;
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

    void toggle (juce::TextButton& b, bool on, std::function<void()> f)
    { b.setClickingTogglesState (true); b.setToggleState (on, juce::dontSendNotification); b.onClick = [this, f] { f(); markCustom(); if (! loading) pushParams(); }; }

    static void noFocus (juce::Component& c) { c.setWantsKeyboardFocus (false); c.setMouseClickGrabsKeyboardFocus (false); for (auto* ch : c.getChildren()) noFocus (*ch); }

    void pedalsChanged()
    {
        auto& d = board.drive; auto& l = board.delay; auto& v = board.reverb;
        P.drvOn = d.foot.getToggleState(); P.drvDrive = d.knobs[0]->s.getValue(); P.drvTone = d.knobs[1]->s.getValue(); P.drvLevel = d.knobs[2]->s.getValue();
        P.dlyOn = l.foot.getToggleState(); P.dlyTime = l.knobs[0]->s.getValue(); P.dlyFb = l.knobs[1]->s.getValue(); P.dlyMix = l.knobs[2]->s.getValue();
        P.rvbOn = v.foot.getToggleState(); P.rvbSize = v.knobs[0]->s.getValue(); P.rvbTone = v.knobs[1]->s.getValue(); P.rvbMix = v.knobs[2]->s.getValue();
        if (! loading) pushParams();
    }

    void buildUi()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &audioBtn, &driverBox, &bufferBox, &chanBox, &gainLbl, &gainSl, &meterIn, &meterOut, &latencyLbl, &tabs,
                                                                  &kGain, &kVol, &kTreb, &kMid, &kBass, &kRev, &kTrs, &kTri, &kOut,
                                                                  &brightBtn, &cabOnBtn, &cabBox, &micLbl, &micSl, &noiseHeader, &humBtn, &humHzBox, &humAmtLbl, &humAmtSl, &humStatus, &gateBtn, &gateLbl, &gateSl,
                                                                  &learnBtn, &learnStatus, &board, &playBtn, &recBtn, &stopBtn, &metBtn, &countBtn, &tapBtn, &compAuto, &timeLbl, &statusLbl, &bpmLbl, &bpmSl,
                                                                  &clickLbl, &clickSl, &compLbl, &compSl, &beatsBox })
            addAndMakeVisible (c);
        for (auto* l : { &gainLbl, &micLbl, &humAmtLbl, &gateLbl, &clickLbl, &compLbl, &bpmLbl, &latencyLbl, &humStatus, &learnStatus, &statusLbl }) styleLabel (*l);
        styleLabel (micLbl, 12.f, juce::Colour (0xffe9e2cf));
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
        bindKnob (kGain, P.gain); bindKnob (kVol, P.vol); bindKnob (kTreb, P.treb); bindKnob (kMid, P.mid); bindKnob (kBass, P.bass); bindKnob (kRev, P.rev); bindKnob (kTrs, P.trs); bindKnob (kTri, P.tri); bindKnob (kOut, P.outVol);
        P.gain = 5; P.vol = 4; P.treb = 6; P.mid = 4; P.bass = 4; P.rev = 0; P.trs = 5; P.tri = 0; P.outVol = 6;
        for (auto* k : { &kGain, &kVol, &kTreb, &kMid, &kBass, &kRev, &kTrs, &kTri, &kOut }) k->setInk (juce::Colour (0xffe9e2cf), altui::cAmber);

        toggle (brightBtn, true, [this] { P.bright = brightBtn.getToggleState(); });
        toggle (cabOnBtn, true, [this] { P.cabOn = cabOnBtn.getToggleState(); });
        cabBox.addItem ("2x12 open back", 1); cabBox.addItem ("1x12 open back", 2); cabBox.setSelectedId (1, juce::dontSendNotification);
        cabBox.onChange = [this] { P.cab = cabBox.getSelectedId() - 1; markCustom(); if (! loading) pushParams(); };
        micSl.setRange (0, 10, 0.1); micSl.setValue (5, juce::dontSendNotification); micSl.setDoubleClickReturnValue (true, 5);
        micSl.onValueChange = [this] { P.mic = micSl.getValue(); markCustom(); if (! loading) pushParams(); }; micSl.setTitle ("Mic position");

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
        learnBtn.setTooltip ("Measures the noise for 2 seconds with the strings muted and sets the gate for you");

        // pedals
        board.drive.add ("DRIVE", 5); board.drive.add ("TONE", 5); board.drive.add ("LEVEL", 5);
        board.delay.add ("TIME", 4); board.delay.add ("FEEDBACK", 3); board.delay.add ("MIX", 3);
        board.reverb.add ("SIZE", 4); board.reverb.add ("TONE", 5); board.reverb.add ("MIX", 3);
        for (auto* p : { &board.drive, &board.delay, &board.reverb }) p->onChange = [this] { pedalsChanged(); };

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
        s->setValue ("gainK", P.gain); s->setValue ("vol", P.vol); s->setValue ("treb", P.treb); s->setValue ("mid", P.mid); s->setValue ("bass", P.bass); s->setValue ("rev", P.rev);
        s->setValue ("trs", P.trs); s->setValue ("tri", P.tri); s->setValue ("outVol", P.outVol); s->setValue ("bright", P.bright); s->setValue ("cabOn", P.cabOn);
        s->setValue ("cab", P.cab); s->setValue ("mic", P.mic); s->setValue ("humOn", P.humOn); s->setValue ("humHz", P.humHz); s->setValue ("humAmt", P.humAmt);
        s->setValue ("gateOn", P.gateOn); s->setValue ("gateThr", P.gateThr); s->setValue ("gain", gainSl.getValue()); s->setValue ("chan", chanBox.getSelectedId());
        s->setValue ("bpm", bpmSl.getValue()); s->setValue ("comp", compSl.getValue()); s->setValue ("click", clickSl.getValue());
        s->setValue ("voicing", voicing); s->setValue ("noiseOpen", noiseOpen);
        s->setValue ("drvOn", P.drvOn); s->setValue ("drvDrive", P.drvDrive); s->setValue ("drvTone", P.drvTone); s->setValue ("drvLevel", P.drvLevel);
        s->setValue ("dlyOn", P.dlyOn); s->setValue ("dlyTime", P.dlyTime); s->setValue ("dlyFb", P.dlyFb); s->setValue ("dlyMix", P.dlyMix);
        s->setValue ("rvbOn", P.rvbOn); s->setValue ("rvbSize", P.rvbSize); s->setValue ("rvbTone", P.rvbTone); s->setValue ("rvbMix", P.rvbMix);
        s->saveIfNeeded();
    }
    void loadSettings()
    {
        auto* s = props.getUserSettings(); if (s == nullptr) return;
        loading = true;
        auto d = [s] (const char* k, double def) { return s->getDoubleValue (k, def); };
        kGain.s.setValue (d ("gainK", 5)); kVol.s.setValue (d ("vol", 4)); kTreb.s.setValue (d ("treb", 6)); kMid.s.setValue (d ("mid", 4)); kBass.s.setValue (d ("bass", 4)); kRev.s.setValue (d ("rev", 0));
        kTrs.s.setValue (d ("trs", 5)); kTri.s.setValue (d ("tri", 0)); kOut.s.setValue (d ("outVol", 6));
        brightBtn.setToggleState (s->getBoolValue ("bright", true), juce::dontSendNotification); cabOnBtn.setToggleState (s->getBoolValue ("cabOn", true), juce::dontSendNotification);
        cabBox.setSelectedId (s->getIntValue ("cab", 0) + 1, juce::dontSendNotification); micSl.setValue (d ("mic", 5));
        humBtn.setToggleState (s->getBoolValue ("humOn", true), juce::dontSendNotification); humHzBox.setSelectedId (s->getIntValue ("humHz", 60) == 50 ? 2 : 1, juce::dontSendNotification);
        humAmtSl.setValue (d ("humAmt", 10)); gateBtn.setToggleState (s->getBoolValue ("gateOn", true), juce::dontSendNotification); gateSl.setValue (d ("gateThr", -58));
        gainSl.setValue (d ("gain", 0)); bpmSl.setValue (d ("bpm", 100)); compSl.setValue (d ("comp", 0)); clickSl.setValue (d ("click", 0.5));
        auto& dv = board.drive; auto& dl = board.delay; auto& rv = board.reverb;
        dv.foot.setToggleState (s->getBoolValue ("drvOn", false), juce::dontSendNotification); dv.knobs[0]->s.setValue (d ("drvDrive", 5)); dv.knobs[1]->s.setValue (d ("drvTone", 5)); dv.knobs[2]->s.setValue (d ("drvLevel", 5));
        dl.foot.setToggleState (s->getBoolValue ("dlyOn", false), juce::dontSendNotification); dl.knobs[0]->s.setValue (d ("dlyTime", 4)); dl.knobs[1]->s.setValue (d ("dlyFb", 3)); dl.knobs[2]->s.setValue (d ("dlyMix", 3));
        rv.foot.setToggleState (s->getBoolValue ("rvbOn", false), juce::dontSendNotification); rv.knobs[0]->s.setValue (d ("rvbSize", 4)); rv.knobs[1]->s.setValue (d ("rvbTone", 5)); rv.knobs[2]->s.setValue (d ("rvbMix", 3));
        noiseOpen = s->getBoolValue ("noiseOpen", true); noiseHeader.open = noiseOpen;
        rebuildChannels(); if (s->getIntValue ("chan", 0) > 0 && s->getIntValue ("chan", 0) <= chanBox.getNumItems() + 1) chanBox.setSelectedId (s->getIntValue ("chan", 1), juce::dontSendNotification);
        P.bright = brightBtn.getToggleState(); P.cabOn = cabOnBtn.getToggleState(); P.humOn = humBtn.getToggleState(); P.gateOn = gateBtn.getToggleState();
        P.cab = cabBox.getSelectedId() - 1; P.humHz = humHzBox.getSelectedId() == 2 ? 50 : 60;
        pedalsChanged();
        voicing = s->getIntValue ("voicing", -1); if (voicing < 0 || voicing >= NumPresets) voicing = -1; tabs.setSelected (voicing);
        loading = false;
        updateComp(); resized();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
