// The main window: audio setup, amp, noise control, recorder.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Widgets.h"

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent()
    {
        setLookAndFeel (&look);
        juce::PropertiesFile::Options po; po.applicationName = "AltGuitarAmpRack"; po.filenameSuffix = ".settings"; po.osxLibrarySubFolder = "Application Support";
        po.folderName = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("AltGuitarAmpRack").getFullPathName();
        props.setStorageParameters (po);

        // Prefer the lowest-latency Windows driver type when nothing is saved yet.
        std::unique_ptr<juce::XmlElement> saved (props.getUserSettings()->getXmlValue ("audioDevice"));
        dm.initialise (2, 2, saved.get(), true, {}, nullptr);
        if (saved == nullptr) pickBestDriver();
        dm.addAudioCallback (&rig);

        buildUi();
        loadSettings();
        pushParams(); applyInput();
        setSize (1040, 800);
        startTimerHz (30);
    }
    ~MainComponent() override
    {
        saveSettings();
        stopTimer(); dm.removeAudioCallback (&rig); setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (altui::cBg);
        for (auto r : { ampBox, noiseBox, recBox })
        {
            g.setColour (altui::cPanel); g.fillRoundedRectangle (r.toFloat(), 8.f);
            g.setColour (altui::cLine); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.f, 1.f);
        }
        g.setColour (altui::cAmber); g.setFont (juce::Font (juce::FontOptions (22.f, juce::Font::bold)));
        g.drawText ("ALT GUITAR AMP RACK", 16, 10, 330, 30, juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold))); g.setColour (altui::cDim);
        g.drawText ("ALT CLEAN  -  FENDER-STYLE", ampBox.getX() + 14, ampBox.getY() + 6, 300, 18, juce::Justification::centredLeft);
        g.drawText ("NOISE CONTROL", noiseBox.getX() + 14, noiseBox.getY() + 6, 300, 18, juce::Justification::centredLeft);
        g.drawText ("RECORDER", recBox.getX() + 14, recBox.getY() + 6, 300, 18, juce::Justification::centredLeft);
        if (beatLit > 0) { g.setColour (beatAccent ? altui::cRed : altui::cAmber); g.fillEllipse ((float) bpmFlash.getX(), (float) bpmFlash.getY(), 14.f, 14.f); }
        else { g.setColour (altui::cLine); g.fillEllipse ((float) bpmFlash.getX(), (float) bpmFlash.getY(), 14.f, 14.f); }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 8);
        auto top = r.removeFromTop (44);
        top.removeFromLeft (340);
        audioBtn.setBounds (top.removeFromLeft (120).reduced (0, 6)); top.removeFromLeft (8);
        chanBox.setBounds (top.removeFromLeft (170).reduced (0, 8)); top.removeFromLeft (10);
        gainLbl.setBounds (top.removeFromLeft (38)); gainSl.setBounds (top.removeFromLeft (110)); top.removeFromLeft (10);
        meterIn.setBounds (top.removeFromLeft (120).reduced (0, 14)); top.removeFromLeft (6); meterOut.setBounds (top.removeFromLeft (120).reduced (0, 14));
        r.removeFromTop (4);
        latencyLbl.setBounds (r.removeFromTop (18));
        r.removeFromTop (6);

        ampBox = r.removeFromTop (178); r.removeFromTop (8);
        {
            auto a = ampBox.reduced (12, 24);
            auto row = a.removeFromTop (100);
            const int kw = row.getWidth() / 8;
            for (auto* k : { &kVol, &kTreb, &kMid, &kBass, &kRev, &kTrs, &kTri, &kOut }) k->setBounds (row.removeFromLeft (kw).reduced (4, 0));
            a.removeFromTop (6);
            brightBtn.setBounds (a.removeFromLeft (90)); a.removeFromLeft (8);
            cabOnBtn.setBounds (a.removeFromLeft (90)); a.removeFromLeft (8);
            cabBox.setBounds (a.removeFromLeft (150)); a.removeFromLeft (14);
            micLbl.setBounds (a.removeFromLeft (70)); micSl.setBounds (a.removeFromLeft (150)); a.removeFromLeft (20);
            voicingLbl.setBounds (a.removeFromLeft (60)); voicingBox.setBounds (a.removeFromLeft (190));
        }
        noiseBox = r.removeFromTop (92); r.removeFromTop (8);
        {
            auto a = noiseBox.reduced (12, 24);
            humBtn.setBounds (a.removeFromLeft (100)); a.removeFromLeft (6);
            humHzBox.setBounds (a.removeFromLeft (80)); a.removeFromLeft (10);
            humAmtLbl.setBounds (a.removeFromLeft (58)); humAmtSl.setBounds (a.removeFromLeft (110)); a.removeFromLeft (10);
            humStatus.setBounds (a.removeFromLeft (170)); a.removeFromLeft (14);
            gateBtn.setBounds (a.removeFromLeft (90)); a.removeFromLeft (8);
            gateLbl.setBounds (a.removeFromLeft (30)); gateSl.setBounds (a.removeFromLeft (110)); a.removeFromLeft (8);
            learnBtn.setBounds (a.removeFromLeft (90)); a.removeFromLeft (8);
            learnStatus.setBounds (a);
        }
        recBox = r;
        {
            auto a = recBox.reduced (12, 24);
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
            for (auto& s : strips) { s->setBounds (a.removeFromTop (62)); a.removeFromTop (6); }
        }
        repaint();
    }

private:
    // ---------- state ----------
    altui::AltLook look;
    juce::ApplicationProperties props;
    juce::AudioDeviceManager dm;
    alt::RigAudio rig;
    alt::Params P;
    juce::Rectangle<int> ampBox, noiseBox, recBox, bpmFlash;
    int beatLit = 0; bool beatAccent = false;
    std::vector<juce::int64> taps;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastLearnDone = 0;
    bool loading = false;

    juce::TextButton audioBtn { "Audio settings" };
    juce::ComboBox chanBox; juce::Label gainLbl { {}, "Gain" }; juce::Slider gainSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    altui::LevelMeter meterIn, meterOut; juce::Label latencyLbl;
    altui::Knob kVol { "VOLUME", 0, 10, 4 }, kTreb { "TREBLE", 0, 10, 6 }, kMid { "MIDDLE", 0, 10, 4 }, kBass { "BASS", 0, 10, 4 },
                kRev { "REVERB", 0, 10, 0 }, kTrs { "TREM SPEED", 0, 10, 5 }, kTri { "TREM DEPTH", 0, 10, 0 }, kOut { "OUTPUT", 0, 10, 6 };
    juce::TextButton brightBtn { "Bright" }, cabOnBtn { "Cabinet" }; juce::ComboBox cabBox;
    juce::Label voicingLbl { {}, "Voicing" }; juce::ComboBox voicingBox;
    juce::Label micLbl { {}, "Mic pos" }; juce::Slider micSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton humBtn { "De-hum" }; juce::ComboBox humHzBox; juce::Label humAmtLbl { {}, "Amount" }; juce::Slider humAmtSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label humStatus, gateLbl { {}, "Thr" }, learnStatus; juce::TextButton gateBtn { "Gate" }, learnBtn { "Learn" }; juce::Slider gateSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton playBtn { "Play" }, recBtn { "Record" }, stopBtn { "Stop" }, metBtn { "Metronome" }, countBtn { "Count-in" }, tapBtn { "Tap" }, compAuto { "Auto" };
    juce::Label timeLbl, statusLbl, bpmLbl, clickLbl { {}, "Click" }, compLbl;
    juce::Slider bpmSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, clickSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, compSl { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox beatsBox;
    std::vector<std::unique_ptr<altui::TrackStrip>> strips;

    // ---------- helpers ----------
    static juce::String fmtKnob (double v) { return juce::String (v, 1); }

    void pickBestDriver()
    {
        // Lowest-latency first. ASIO exists only if the app was built with it.
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
    { b.setClickingTogglesState (true); b.setToggleState (on, juce::dontSendNotification); b.onClick = [this, f] { f(); if (! loading) pushParams(); }; }

    void buildUi()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &audioBtn, &chanBox, &gainLbl, &gainSl, &meterIn, &meterOut, &latencyLbl, &kVol, &kTreb, &kMid, &kBass, &kRev, &kTrs, &kTri, &kOut,
                                                                  &brightBtn, &cabOnBtn, &cabBox, &voicingLbl, &voicingBox, &micLbl, &micSl, &humBtn, &humHzBox, &humAmtLbl, &humAmtSl, &humStatus, &gateBtn, &gateLbl, &gateSl,
                                                                  &learnBtn, &learnStatus, &playBtn, &recBtn, &stopBtn, &metBtn, &countBtn, &tapBtn, &compAuto, &timeLbl, &statusLbl, &bpmLbl, &bpmSl,
                                                                  &clickLbl, &clickSl, &compLbl, &compSl, &beatsBox })
            addAndMakeVisible (c);
        for (auto* l : { &gainLbl, &voicingLbl, &micLbl, &humAmtLbl, &gateLbl, &clickLbl, &compLbl, &bpmLbl, &latencyLbl, &humStatus, &learnStatus, &statusLbl }) styleLabel (*l);
        styleLabel (timeLbl, 20.f, altui::cAmber, juce::Justification::centredLeft); timeLbl.setFont (juce::Font (juce::FontOptions (22.f, juce::Font::bold)));
        timeLbl.setText ("0:00.0", juce::dontSendNotification);

        audioBtn.onClick = [this]
        {
            auto* sel = new juce::AudioDeviceSelectorComponent (dm, 0, 2, 0, 2, false, false, true, false);
            sel->setSize (560, 420);
            juce::DialogWindow::LaunchOptions o; o.content.setOwned (sel); o.dialogTitle = "Audio settings"; o.componentToCentreAround = this;
            o.dialogBackgroundColour = altui::cPanel; o.escapeKeyTriggersCloseButton = true; o.useNativeTitleBar = true; o.resizable = false;
            o.launchAsync();
        };
        chanBox.onChange = [this] { applyInput(); };
        gainSl.setRange (-12, 24, 0.5); gainSl.setValue (0, juce::dontSendNotification); gainSl.setDoubleClickReturnValue (true, 0); gainSl.onValueChange = [this] { applyInput(); };
        gainSl.setTitle ("Input gain");

        auto bindKnob = [this] (altui::Knob& k, double& dst) { k.s.onValueChange = [this, &k, &dst] { dst = k.s.getValue(); markCustom(); if (! loading) pushParams(); }; };
        bindKnob (kVol, P.vol); bindKnob (kTreb, P.treb); bindKnob (kMid, P.mid); bindKnob (kBass, P.bass); bindKnob (kRev, P.rev); bindKnob (kTrs, P.trs); bindKnob (kTri, P.tri); bindKnob (kOut, P.outVol);
        P.vol = 4; P.treb = 6; P.mid = 4; P.bass = 4; P.rev = 0; P.trs = 5; P.tri = 0; P.outVol = 6;
        for (auto* k : { &kVol, &kTreb, &kMid, &kBass, &kRev, &kTrs, &kTri, &kOut }) k->setInk (altui::cText, altui::cAmber);

        toggle (brightBtn, true, [this] { P.bright = brightBtn.getToggleState(); });
        toggle (cabOnBtn, true, [this] { P.cabOn = cabOnBtn.getToggleState(); });
        cabBox.addItem ("2x12 open back", 1); cabBox.addItem ("1x12 open back", 2); cabBox.setSelectedId (1, juce::dontSendNotification);
        cabBox.onChange = [this] { P.cab = cabBox.getSelectedId() - 1; if (! loading) pushParams(); };
        micSl.setRange (0, 10, 0.1); micSl.setValue (5, juce::dontSendNotification); micSl.setDoubleClickReturnValue (true, 5);
        micSl.onValueChange = [this] { P.mic = micSl.getValue(); if (! loading) pushParams(); }; micSl.setTitle ("Mic position");

        struct Preset { const char* name; double vol, treb, mid, bass, rev, trs, tri; bool bright; int cab; double mic; };
        static const Preset presets[] = {
            { "Twin sparkle",    4,   5,   5,   4,   2.5, 5,   0, true,  0, 5 },
            { "Warm jazz",       3.5, 2.5, 6,   5.5, 2,   5,   0, false, 1, 4 },
            { "Surf",            4.5, 6,   4,   3,   7,   5.5, 5, true,  0, 6 },
            { "Edge of breakup", 8,   5,   5.5, 4,   2,   5,   0, false, 1, 5 },
            { "Bright & dry",    3,   6.5, 4,   3.5, 0,   5,   0, true,  0, 6 } };
        voicingBox.addItem ("Custom", 1);
        for (int i = 0; i < 5; ++i) voicingBox.addItem (presets[i].name, 2 + i);
        voicingBox.setSelectedId (1, juce::dontSendNotification);
        voicingBox.onChange = [this]
        {
            const int id = voicingBox.getSelectedId(); if (id < 2) return;
            const auto& q = presets[id - 2];
            loading = true;
            kVol.s.setValue (q.vol); kTreb.s.setValue (q.treb); kMid.s.setValue (q.mid); kBass.s.setValue (q.bass); kRev.s.setValue (q.rev); kTrs.s.setValue (q.trs); kTri.s.setValue (q.tri);
            P.bright = q.bright; brightBtn.setToggleState (q.bright, juce::dontSendNotification);
            cabBox.setSelectedId (q.cab + 1, juce::dontSendNotification); P.cab = q.cab; micSl.setValue (q.mic); P.mic = q.mic;
            loading = false; pushParams();
        };
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
    }

    void pushParams() { rig.setParams (P); }
    void markCustom() { if (! loading && voicingBox.getSelectedId() != 1) voicingBox.setSelectedId (1, juce::dontSendNotification); }

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
    int shownChannels = -1;

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
            const double inMs = 1000.0 * dev->getInputLatencyInSamples() / sr, outMs = 1000.0 * dev->getOutputLatencyInSamples() / sr, bufMs = 1000.0 * dev->getCurrentBufferSizeSamples() / sr;
            latencyLbl.setText (dev->getTypeName() + "  |  " + juce::String ((int) sr) + " Hz  |  buffer " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples ("
                                + juce::String (bufMs, 1) + " ms)  |  reported round trip " + juce::String (inMs + outMs, 1) + " ms", juce::dontSendNotification);
            if (compLbl.getText() == "Comp 0 ms" && compSl.getValue() == 0) updateComp();
        }
        else latencyLbl.setText ("No audio device - open Audio settings", juce::dontSendNotification);

        meterIn.setLevel (rig.peakIn.load()); meterOut.setLevel (rig.peakOut.load());

        const int bf = rig.beatFlash.exchange (0);
        if (bf) { beatLit = 4; beatAccent = bf == 2; repaint (bpmFlash.expanded (4)); }
        else if (beatLit > 0) { --beatLit; repaint (bpmFlash.expanded (4)); }

        // finished take
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

        // hum status + learn
        const float hl = rig.getEngine().humLevelDb.load();
        humStatus.setText (P.humOn ? (hl > -100.f ? "Hum removed: " + juce::String (hl, 0) + " dB @ " + juce::String (rig.getEngine().humHzOut.load(), 1) + " Hz" : "No hum detected") : "De-hum off", juce::dontSendNotification);
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
    int lastState = 0;

    // ---------- settings ----------
    void saveSettings()
    {
        auto* s = props.getUserSettings(); if (s == nullptr) return;
        if (auto xml = dm.createStateXml()) s->setValue ("audioDevice", xml.get());
        s->setValue ("vol", P.vol); s->setValue ("treb", P.treb); s->setValue ("mid", P.mid); s->setValue ("bass", P.bass); s->setValue ("rev", P.rev);
        s->setValue ("trs", P.trs); s->setValue ("tri", P.tri); s->setValue ("outVol", P.outVol); s->setValue ("bright", P.bright); s->setValue ("cabOn", P.cabOn);
        s->setValue ("cab", P.cab); s->setValue ("mic", P.mic); s->setValue ("humOn", P.humOn); s->setValue ("humHz", P.humHz); s->setValue ("humAmt", P.humAmt);
        s->setValue ("gateOn", P.gateOn); s->setValue ("gateThr", P.gateThr); s->setValue ("gain", gainSl.getValue()); s->setValue ("chan", chanBox.getSelectedId());
        s->setValue ("bpm", bpmSl.getValue()); s->setValue ("comp", compSl.getValue()); s->setValue ("click", clickSl.getValue());
        s->saveIfNeeded();
    }
    void loadSettings()
    {
        auto* s = props.getUserSettings(); if (s == nullptr) return;
        loading = true;
        auto d = [s] (const char* k, double def) { return s->getDoubleValue (k, def); };
        kVol.s.setValue (d ("vol", 4)); kTreb.s.setValue (d ("treb", 6)); kMid.s.setValue (d ("mid", 4)); kBass.s.setValue (d ("bass", 4)); kRev.s.setValue (d ("rev", 0));
        kTrs.s.setValue (d ("trs", 5)); kTri.s.setValue (d ("tri", 0)); kOut.s.setValue (d ("outVol", 6));
        brightBtn.setToggleState (s->getBoolValue ("bright", true), juce::sendNotification); cabOnBtn.setToggleState (s->getBoolValue ("cabOn", true), juce::sendNotification);
        cabBox.setSelectedId (s->getIntValue ("cab", 0) + 1, juce::sendNotification); micSl.setValue (d ("mic", 5));
        humBtn.setToggleState (s->getBoolValue ("humOn", true), juce::sendNotification); humHzBox.setSelectedId (s->getIntValue ("humHz", 60) == 50 ? 2 : 1, juce::sendNotification);
        humAmtSl.setValue (d ("humAmt", 10)); gateBtn.setToggleState (s->getBoolValue ("gateOn", true), juce::sendNotification); gateSl.setValue (d ("gateThr", -58));
        gainSl.setValue (d ("gain", 0)); bpmSl.setValue (d ("bpm", 100)); compSl.setValue (d ("comp", 0)); clickSl.setValue (d ("click", 0.5));
        rebuildChannels(); if (s->getIntValue ("chan", 0) > 0 && s->getIntValue ("chan", 0) <= chanBox.getNumItems() + 1) chanBox.setSelectedId (s->getIntValue ("chan", 1), juce::dontSendNotification);
        P.bright = brightBtn.getToggleState(); P.cabOn = cabOnBtn.getToggleState(); P.humOn = humBtn.getToggleState(); P.gateOn = gateBtn.getToggleState();
        P.cab = cabBox.getSelectedId() - 1; P.humHz = humHzBox.getSelectedId() == 2 ? 50 : 60;
        loading = false;
        updateComp();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
