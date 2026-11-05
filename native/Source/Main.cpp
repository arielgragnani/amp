#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

// Holds the main component at its design size and scales it to whatever window size the user picks.
class ScaledHolder : public juce::Component
{
public:
    ScaledHolder() { addAndMakeVisible (inner); inner.setSize (MainComponent::DesignW, MainComponent::DesignH); setSize (MainComponent::DesignW, MainComponent::DesignH); }
    void resized() override
    {
        const float s = std::min ((float) getWidth() / MainComponent::DesignW, (float) getHeight() / MainComponent::DesignH);
        inner.setTransform (juce::AffineTransform::scale (s).translated ((getWidth() - MainComponent::DesignW * s) * 0.5f, (getHeight() - MainComponent::DesignH * s) * 0.5f));
    }
    void paint (juce::Graphics& g) override { g.fillAll (altui::cBg); }
    MainComponent inner;
};

class AltApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return juce::String::fromUTF8 ("Ariel\xE2\x80\x99s Signature Amp Rack"); }
    const juce::String getApplicationVersion() override { return "1.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise (const juce::String&) override { win = std::make_unique<Win> (getApplicationName()); }
    void shutdown() override { win.reset(); }
    void systemRequestedQuit() override { quit(); }

    struct Win : juce::DocumentWindow
    {
        Win (const juce::String& n) : DocumentWindow (n, altui::cBg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true); setContentOwned (new ScaledHolder(), false);
            setResizable (true, true);
            const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            const float s = std::min (1.0f, std::min ((area.getWidth() - 20.f) / MainComponent::DesignW, (area.getHeight() - 70.f) / MainComponent::DesignH));
            const int w = (int) (MainComponent::DesignW * s), h = (int) (MainComponent::DesignH * s);
            setResizeLimits (520, 470, 2400, 2200);
            centreWithSize (w, h + getTitleBarHeight()); setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Win> win;
};
START_JUCE_APPLICATION (AltApp)
