#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class AltApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Alt Guitar Amp Rack"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise (const juce::String&) override { win = std::make_unique<Win> (getApplicationName()); }
    void shutdown() override { win.reset(); }
    void systemRequestedQuit() override { quit(); }

    struct Win : juce::DocumentWindow
    {
        Win (const juce::String& n) : DocumentWindow (n, altui::cBg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true); setContentOwned (new MainComponent(), true);
            setResizable (false, false); centreWithSize (getWidth(), getHeight()); setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Win> win;
};
START_JUCE_APPLICATION (AltApp)
