# Ariel's Signature Amp Rack - Windows app

Standalone Windows amp simulator: tweed-style amp, voicing tabs, noise control, a pedalboard (Blues Crunch, Echo Delay, Hall Reverb) and a 4-track recorder with metronome.

## Get the app
Open the **Actions** tab on GitHub, click the newest green **Build Windows app** run, download **Ariel-Signature-Amp-Rack-Windows** under *Artifacts*, right-click the zip > Properties > Unblock, then Extract All. Double-click `Ariel Signature Amp Rack.exe`. Nothing is installed.

## Lowest delay
1. Top bar, **Driver**: pick `ASIO` if it is in the list (the build tries to include it), otherwise `Windows Audio (Exclusive Mode)`.
2. **Buffer**: choose the smallest size that doesn't crackle (64 or 128 samples is typical; 32 on a good interface).
3. Use your interface's own ASIO driver if it has one. If it doesn't, ASIO4ALL or FlexASIO give ASIO access to any sound card.
4. Press **Auto** next to *Comp* so recorded takes line up with the click.
`BUILD-INFO.txt` in the zip says whether ASIO was included.

## Noise samples
If a pickup is noisy: open **Noise control**, mute the strings, press **Save noise sample** (5 s), save the .wav and send it with a note saying which pickup and where you were. It saves your raw guitar signal and the amp output, which is enough to tell hum from buzz from hiss.

## Tuner
Press **Tuner** in the recorder row. The amp is muted while it is open (switch "Mute amp" off to hear it).

## Voicings
Click a tab, or press the **Left / Right arrow keys**. Turning any knob switches to "custom" (no tab lit).

## Licences
Built with JUCE (AGPLv3 or commercial/personal licence). The ASIO SDK is Steinberg's and is not redistributed; the build downloads it directly from Steinberg. Check both licences before sharing the app beyond your own classroom.

## Font
The signature in the title bar uses Sacramento (Copyright 2012 The Sacramento Project Authors), licensed under the SIL Open Font License 1.1 (https://openfontlicense.org).
