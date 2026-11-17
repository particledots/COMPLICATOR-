# COMPLICATOR

An original AU / VST3 instrument for Logic Pro: a complex oscillator with two contrasting cores, inspired by the
Instruō Cš-L. It is not a copy, contains no Instruō code or artwork, and is not affiliated with Instruō. The Cš-L is an
analog module with no published firmware; its manual was used only to understand what the controls do.

Status: **milestone 1 of 3 (waiting for a test in Logic).** Version 0.1.0.

## What works now (milestone 1)

- **Two oscillators.** Osc A is a sawtooth core (outputs: sine, triangle, sawtooth, PWM pulse). Osc B is a triangle
  core (outputs: sine, triangle, square, and a "PWM" staircase triangle whose top and bottom steps widen with its PWM
  knob). Each has Coarse (±48 semitones), Fine (±1 semitone) and PWM.
- **Keys play A / A+B / B** (the module's Link button). An oscillator that does not follow the keys sits at MIDI note 48
  (about 130.8 Hz) plus its Coarse. One note at a time, last note wins; releasing a note goes back to the one still held.
  Pitch bend ±2 semitones.
- **FM**: each oscillator's sine feeds the other's FM input, as on the module. FM amount knob and a curve switch:
  LIN, EXP, or TZ (linear through-zero, which the module does not have: the frequency can pass through zero and run
  backwards). LIN stops at 0 Hz.
- **LFO** (Osc A): drops Osc A by 7 octaves; it still follows the keys and hard sync still restarts it.
- **Sub** (Osc B): the square wave at the oscillator's pitch, one octave down or two octaves down.
- **Sync**: Off; Soft (Osc A's ramp turns Osc B round: an Osc B tuned a little flat of Osc A, or of a whole multiple
  of it, locks and stops beating); Hard (Osc B's square, or sub square, restarts Osc A).
- **Pitch Snap** (Off / Semitones / Steps), **Scale** (Off + the same 28 scales as the other particledots plugins) and
  **Key** (C to B). They act on the Coarse knobs only; Fine is never moved. Steps lands on octaves and fifths
  (0, ±7, ±12, ±19, ±24, ±31, ±36, ±43, ±48). With a scale on, Coarse only lands where the oscillator's note is in the
  scale in that key; the note you play itself, and its octaves, are always allowed, so Coarse 0 always plays exactly the
  note you played. With both oscillators on scale notes, the interval between them is a scale interval too. The small
  number under each Coarse knob shows the offset really played. All Off by default.
- **Output mix**: a level for each output, then **Amp** (Drone: always sounding, like the module; Env: an ADSR
  envelope opened by each note, the default) and **Volume**.
- Everything runs at 4× the project's sample rate, with band-limited waveforms (every jump and corner is smoothed) and
  a steep filter on the way back down. Every knob is smoothed. The engine self-test measures aliasing at 69 to 92 dB
  below the signal for a high note, and checks pitch, sync, sub, the Snap/Scale rules and heavy random automation.

## Coming

- **Milestone 2**: the wavefolders with Symmetry and the Final outputs, the multiplier (Ring / Half-wave / Full-wave,
  Depth), Index with its routing row, the Patch row (what feeds each input: any output, the multiplier, the envelope,
  velocity, mod wheel, aftertouch, or the sidechain), and the scopes.
- **Milestone 3**: factory presets, a Quality menu (2× / 4× / 8×), CPU checks on the Intel Mac, pluginval clean, polish.

These controls are already on the panel, dimmed and marked M2 or M3, and already stored in projects: every parameter
was fixed in the first build, so projects saved now will open unchanged later.

## Folders

| Folder | What it is |
|---|---|
| `engine/` | The audio engine. Plain C++, no plugin code: oscillators, band-limiting, decimation filter, pitch rules, envelope. Reusable for later plugins. |
| `plugin/` | The plugin wrapper: what Logic sees, the list of controls (`Parameters`), and the panel (`PluginEditor`, `Controls`, `Style`). |
| `tools/install/` | The same installer as the other particledots plugins (unchanged). |
| `.github/workflows/` | The automatic build. |

The wrapper hands the engine the control values, the notes and pitch bend; the engine hands audio back.
Nothing in `engine/` knows about knobs or Logic.

## Getting the plugin

1. Open the **Actions** tab of this repo and click the latest **Build COMPLICATOR** run.
2. At the bottom, under **Artifacts**, download **COMPLICATOR-macOS** and unzip it (twice: the download is a zip
   containing `COMPLICATOR-macOS.zip`).
3. Inside the `COMPLICATOR-macOS` folder are `COMPLICATOR.component`, `COMPLICATOR.vst3`, `INSTALL.txt`, `auval.log`,
   the pluginval logs and `Install-Unofficial-Plugin.command`.
4. In Terminal, once, clear the download block: `xattr -dr com.apple.quarantine ~/Downloads/COMPLICATOR-macOS/*`
   (adjust the path if you unzipped elsewhere).
5. **Easy install (Logic, Audio Unit):** double-click `Install-Unofficial-Plugin.command`. It asks for your Mac
   password once, installs the AU for all users and restarts the AU registrar.
   Or by hand: copy `COMPLICATOR.component` to `~/Library/Audio/Plug-Ins/Components/` and `COMPLICATOR.vst3` to
   `~/Library/Audio/Plug-Ins/VST3/`.
6. Restart Logic. If it was already scanned, Logic Pro > Settings > Plug-In Manager > Reset & Rescan Selection.

COMPLICATOR is an instrument: on a Software Instrument track it appears under **AU Instruments > particledots >
COMPLICATOR**. The same package also sits in a private draft release of this repo (only people with access see it).

## Notes

- Built as a universal binary (Apple Silicon and Intel), macOS 11 or newer. Tested targets: M4 Mac mini (macOS Tahoe,
  Logic Pro 11.2.2) and an Intel Mac (macOS 12.7.8, Logic Pro 10.7.8).
- Signed ad-hoc only (no Apple Developer account), hence the quarantine step above.
- Fixed identifiers (never to be changed): name COMPLICATOR, bundle id `com.particledots.complicator`, manufacturer
  code `Pdot`, plugin code `Cmpl`, AU type instrument (`aumu`). Internal name `Complicator`.
- The sidechain input is declared but unused until milestone 2.
- Double-click a knob to return it to its default; hold Shift while dragging for finer moves. Drag the bottom-right
  corner to resize the window.
