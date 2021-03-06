# TouchSensorClickFilter
Filters out (actually, disables) clicks produced by optical touch sensor

## The problem

I have One Netbook Mix 3 Yoga which has an IR touch sensor and two buttons for mouse-like control. But the sensor itself is able to produce left button clicks. It does it randomly, and I never managed to produce clicks intentionally. It is back to impossible not only to move mouse cursor, but also drag and select (when physical left button is pressed) operations. I've experienced the same problem with Mix 2s, and I heard people have the same problem with other devices.

From my investigation, the produced clicks have a delay between button down and up events of less than 16 ms. But it is not just enough to filter out short clicks: it may produce "button up" event when dragging (with physical button pressed), and it may produce mouse move events (with actually no movement) when it is not touched.

I found no an app to filter out such misbehaviour _and_ to keep touch screen working.

## The solution

The app I wrote does the following:
* Filters out short (<50 ms) clicks.
* Filters out short (<3 pixels) movements when left button is just pressed.
* Sticky drag: you press and hold left button, start moving the cursor, release the button, and click it again when you finished dragging.
* Passes through mouse events, generated from touch screen events and/or coming from other sources.
* Disables the filter in RDP sessions.

Sticky drag is the only solution I've found against "button up" events produced by the sensor.

The filter can be temporarily disabled, or app can be closed from pop-up menu. It is 64-bit application tested under Windows 10 only.

## How to use

It is distributed as an executable file. No installation is required. Download a zip archive from [Releases section](https://github.com/Lurker00/TouchSensorClickFilter/releases), unzip, put in a suitable directory, and run. If you need to launch it automatically, use Windows means for this purpose.

**Note:** The filter works only for the current user under which it was launched. It will not work for elevated apps (started as Administrator), if it wasn't launched as Administrator.

To autostart as Administrator without UAC prompt, create a Scheduled Task with "Run with highest privileges", and then a shortcut for it, or add "At log on" trigger. A good example is [here](https://www.tenforums.com/tutorials/57690-create-elevated-shortcut-without-uac-prompt-windows-10-a.html).

You can check app starts and stops using [Windows Event Viewer (Windows Logs->Application)](https://en.wikipedia.org/wiki/Event_Viewer). In the stopping event it provides the statistics of the past session.

You may provide a feedback in the [dedicated thread at Reddit](https://www.reddit.com/r/GPDPocket/comments/chmer5/one_mix_123_touch_sensor_misbehavior_the_solution/).

## Credits

I've derived [SysTrayDemo](https://www.codeproject.com/Articles/18783/Example-of-a-SysTray-App-in-Win32) example which is distributed under [CPOL](https://www.codeproject.com/info/cpol10.aspx) license.

## Copyright (c) 2019

... is mine, but I dunno which license to publish it under :) If you derive my work, don't forget to mention the primary source (me and this place).

## History

* [0.92](https://github.com/Lurker00/TouchSensorClickFilter/releases/tag/v0.92) - rather cosmetic improvements to simplify use.
* [0.91](https://github.com/Lurker00/TouchSensorClickFilter/releases/tag/v0.91) - first public release.
