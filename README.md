# Auto Theremin

*APT Industrial · Model AT-1*

A theremin VST3 for FL Studio, with three ways to play it:

- **Auto**: draw pitch and volume curves on a green-phosphor plotter (nodes, shapes, tension handles, freehand drawing) and it plays them in sync with the song.
- **Live**: play it with a PS5 DualSense (touchpad, gyro tilt or sticks for pitch; a trigger for volume) or the mouse, and record takes straight into the curves.
- **MIDI**: a keyboard or piano roll, gliding between notes.

The sound comes from a heterodyne oscillator model. Two coupled oscillators follow Adler's equation, which gives the theremin's slightly lopsided, reedy tone. It's followed by a valve stage, formant filters, expressive vibrato, Theremini-style pitch correction, a ping-pong tape echo and an 8-line FDN reverb.

Built with JUCE 8 and SDL3. GitHub Actions builds Windows binaries on every push (see the Actions tab). Installation and controls are in `INSTALL.txt`.

Fonts (Monoton, Michroma, Righteous, Share Tech Mono) are under the SIL Open Font License; see `Resources/fonts`.
