# Auto Theremin - Version History

Every version is published automatically as a GitHub Release with the plugin attached (from 11 November 2026), and the APT Industrial updater installs the newest one for you.

## 1.0.4 - 11 November 2026
- The version number is now shown in the plugin window. Click it to read this version history inside the plugin.

## 1.0.3 - 9 November 2026
- FIXED: a note could be dropped when it arrived in an unusually large audio block, and a very short (empty) block could read out of range. MIDI All Notes Off is now always obeyed.

## 1.0.2 - 9 November 2026
- Stop means silence. When a song is playing and you press Stop in FL Studio (or any DAW), the plugin fades out in about 15 ms and goes silent: every note, the sustain, any internal sequencer or auto mode, and delay and reverb tails. The same happens on MIDI All Notes Off / All Sound Off while stopped, on host reset, and when a project loads.
- Playing live while the transport is stopped is unaffected: notes and tails ring out naturally.

## 1.0.0 - 8 November 2026
- First version: a heterodyne theremin with the Auto curve plotter, live playing from a PS5 DualSense controller, MIDI glide mode, and the 1950s sci-fi lab look.

