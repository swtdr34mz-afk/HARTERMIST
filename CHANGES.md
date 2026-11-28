# HARTERMIST – changes

The newest first. To release a new version: raise the number in VERSION and put its notes here under
"## <the new number>". After the merge into main the release is made by itself (auto-release.yml), the
downloads are built into it, and the website and the Discord post follow.

## 1.0.3
New: HARTERMIST for Linux - VST3, LV2 and a standalone app (64-bit, built on Ubuntu 22.04). Checked with pluginval like the Windows version.

## 1.0.2
The window opens as large as you left it - also after closing and reopening the DAW (before, it came back at the smallest size). A new instance starts at the size you used last.
Saving your own sounds fixed: the save window now opens inside the plugin (before, it could open hidden behind the DAW's plugin window, so nothing got saved). If Windows keeps apps out of the Documents folder, your sounds are saved in AppData instead. A failed save now says so.

## 1.0.1
HARTERMIST 1.0.1

- Right-click any knob, slider or button: the host's own menu for that parameter (MIDI learn, automation, ...). A right click never changes a value, a right drag moves nothing.
- CC74 as a mod matrix source is centred on 64: at 64 it moves nothing, under 64 down, over 64 up (FL Studio and controllers such as the Launchkey send CC74 = 64 with their notes). CC121 sets it back to 64.
