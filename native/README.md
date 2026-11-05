# Alt Guitar Amp Rack - Windows app

A standalone Windows version of the amp, with much lower latency than the browser version because it talks to the sound card directly (WASAPI Exclusive, or ASIO if you build it in).

## Get the .exe (no programming needed)
1. On GitHub click **+ > New repository**, name it `amp-app`, create it.
2. Click **Add file > Upload files** and drag in **everything inside this folder** (including the hidden `.github` folder - if your browser won't drag it, see note below). Commit.
3. Open the **Actions** tab, click **Build Windows app** on the left, then **Run workflow**. Wait about 10 minutes.
4. Click the finished run, scroll to **Artifacts**, download **Alt-Guitar-Amp-Rack-Windows**, unzip, and double-click the .exe.

Note: if `.github` won't upload, create the file in GitHub with **Add file > Create new file**, type `.github/workflows/build-windows.yml` as the name and paste the contents of that file.

Students need to install nothing else. Windows may show a "SmartScreen" warning for an unsigned program: click **More info > Run anyway**.

## First run
1. Click **Audio settings**. Under *Audio device type* choose **Windows Audio (Exclusive Mode)** (or ASIO). Pick your interface for input and output and the smallest buffer that stays clean (64-128 samples).
2. Choose the input channel the guitar is on, play, and set Gain so the left meter peaks in the amber.
3. The top line shows the buffer size and the latency the driver reports. Use **Auto** next to *Comp* so recordings line up with the click.

With a USB interface that has its own ASIO driver, round-trip latency of 5-10 ms is normal. Without one, Exclusive mode usually gives 10-20 ms.

## Optional: ASIO
Steinberg's ASIO SDK cannot be redistributed here. Download it from Steinberg, then build with `-DALT_ASIO=ON -DASIO_SDK_DIR=<path>`. The ASIO SDK and JUCE have their own licences - check them before sharing the app beyond your own classroom.

## Licence note
The app is built with JUCE (AGPLv3 or commercial/personal licence). Distributing the .exe publicly means complying with one of those.
