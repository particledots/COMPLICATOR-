# COMPLICATOR

An original AU / VST3 instrument for Logic Pro: a complex oscillator with two contrasting cores, inspired by the
Instruō Cš-L. It is not a copy, contains no Instruō code or artwork, and is not affiliated with Instruō. The Cš-L is an
analog module with no published firmware; its manual was used only to understand what the controls do.

Status: **all 3 milestones done (version 0.3.0, tested in Logic on 2026-11-17).** Nothing further is planned; only
cosmetic panel changes may follow. Milestones 1 to 3 were tested in Logic on the Intel Mac (Logic 10.7.8); milestones 1
and 2 also on the M4 Mac mini (Logic 11.2.2).

After milestone 3, 22 more factory presets of my own were added (see "More factory presets" below). Build and engine
tests passed in Actions and Bryn tested them in Logic on 2026-11-17: everything works.

Milestone 3 test result (Bryn, Intel Mac): old project opens unchanged; all 9 presets sound and are level matched;
the sidechain folder preset works; the preset name survives save and reopen; Init and presets leave Pitch Snap, Scale,
Key and Quality alone; switching Quality while a note is held gives no clicks; the Quality menu shows "8x heavy"; a
soft-synced fifth stops beating; hard knob automation at 4x gives no clicks. Intel Mac CPU is in the table below.

Test notes: the Logic CPU meter shows no numbers on the Intel Mac, so its load is measured with Activity Monitor
(see "CPU and the Quality menu"). The Final A / Final B mixer levels start at 0, so the folders are silent until a
Final level is raised; every factory preset sets the Final levels it needs.

## The panel (0.3.1)

The panel is in the "gold sections" look of Anamnesis 0.3.1: black, gold hairline frames grouping the controls, cream knob
caps inside gold range arcs, and blue lamps as the only colour (they mean "on" or "chosen"). Sections: **Preset / Quality**,
**Osc A** and **Osc B** (tuning, FM, Final scope, wavefold), **Pitch and Sync**, **Multiply**, **Index** (Index, CV amount
and the routes), **Patch**, and **Output mix** (the eleven levels, Envelope, Amp, Volume). Gold flow lines show how the
sound moves: Wavefold, Fold CV and Symmetry run into each Final scope; Osc A (modulator) and Osc B (carrier) run into
Multiply; CV amount adds to Index, which feeds the routes; the eleven mix levels run into Amp and then Volume.

Only the look changed: every control, id, order, range, menu list and the plugin and manufacturer codes are the same, and
saved projects load as before. The panel is 680 x 708 units and opens at 120% (resizable from 90% to 180%). The colour
coding of the oscillators and the amber "8x heavy" colour are gone; the menu still says "8x heavy". The fixed artwork is
drawn once into a stored picture; 30 times a second only the two scopes and the two Coarse texts are redrawn. The check
program tests that the title fits at five window sizes (in the drawn pixels), that every text fits and none touches
another, and that the live update is much lighter than drawing the whole panel. Status: built, not yet tested in Logic.

## What is new in milestone 3

- **Factory presets**: the Preset menu is live. "Init" resets every control except Pitch Snap, Scale, Key and Quality.
  The nine factory presets are my own settings that rebuild the example patches of the Cš-L manual. A preset never
  touches Pitch Snap, Scale, Key or Quality, and sets its own Final levels and Volume (all about the same loudness).
  A project remembers which preset was chosen; if you then move a knob the menu still shows it, and a project saved
  before 0.3.0 shows "Custom".

  | Preset | Rebuilds the manual's example |
  |---|---|
  | East Coast Voice | East Coast voice (Osc B with sub, PWM and envelope) |
  | Complete Voice | A complete voice (folder, multiplier as the VCA, envelope) |
  | Synced Growl | Hard sync growl through both folders and the multiplier |
  | Formant Voice | Formant voice (hard sync, Osc A two octaves up, sub two down) |
  | Spooky Theremin | Spooky theremin (Osc A as LFO, Osc B sine) |
  | Wavefold Multiply | Wavefolded oscillator through the multiplier |
  | Racket Voice | Racket (both oscillators FM-ed, PWM routed from the Index) |
  | Five Modules | The five-module patch |
  | Wavefolder (sidechain in) | The folder used on outside audio: needs a Side Chain source |

- **More factory presets (added after milestone 3)**: 22 presets of my own, not from the manual, are appended after
  the nine above, so the Preset menu now has 32 entries (Init + 31) and every saved menu choice still points to the
  same preset. They are level matched to the others and never touch Pitch Snap, Scale, Key or Quality.

  | Preset | What it is | Play it with |
  |---|---|---|
  | Glass Bell | FM bell: bright inharmonic strike that relaxes into a pure tone | notes C5 and up |
  | Tine Piano | FM electric piano | velocity (harder = brighter) |
  | Metal Gong | ring-modulated gong that rings for seconds | low to middle notes |
  | Fold Pluck | the envelope sweeps the wavefolder like a filter | short notes |
  | Velocity Brass | both folders driven by velocity | velocity (soft = dull, hard = bright) |
  | Reed Organ | folded sine with square and triangle | held chords |
  | Hollow Flute | Osc B sine with light vibrato | notes around C5 |
  | Pressure Vibrato | Osc A as LFO bends Osc B | aftertouch (none = steady, full = about ±½ semitone) |
  | Wheel Folder | mod wheel opens Osc A's folder and Symmetry | mod wheel |
  | Sync Sweep Lead | hard sync, the envelope sweeps the timbre | leads |
  | Soft Sync Fifth | soft sync with B a fifth up; locks 2 : 3 and sounds an octave below the note | bass or lead |
  | Stair Pad | a slow LFO moves Osc B's staircase width | held, slow |
  | Open Fifth Drone | Amp is Drone: sounds with no note | always on |
  | Deep Sub | Osc B sine plus its sub square one octave down | low notes |
  | Wobble Bass | LFO of about 5 Hz folds Osc B | low notes |
  | Acid Fold | envelope opens the folder, Symmetry adds even harmonics | low, short notes |
  | Sine Kick | the envelope sweeps Osc A's pitch down about 4 octaves | around C2 |
  | Laser Zap | hard sync with a pitch sweep down about 4 octaves | any note |
  | Metal Hat | Osc B square times a folded high Osc A, short envelope | any note |
  | Dub Siren | Osc A as an LFO ramp sweeps Osc B's pitch | held note |
  | Tangle | the two oscillators frequency-modulate each other through their folded Finals | low to middle notes |
  | Ring Voice (sidechain in) | ring modulator for outside audio; silent until a Side Chain source is chosen | keys tune the carrier |

- **Quality menu** (2x / 4x / 8x oversampling): the parameter was there since 0.1.0 and is unchanged. **4x stays the
  default. 8x is marked "heavy"** in the menu, and its label turns amber when chosen. Changing it while playing fades
  the sound out and in over about 4 ms, with no click, and the plugin's latency stays almost the same (about 19 samples).
- **Soft sync** now pulls Osc B only half way to the peak at each turn. A plain fifth (and a fourth, a third, a sixth)
  now locks into the pattern the interval really has, not twice as long. Projects that use Soft sync sound slightly
  different, by design; everything else is bit-identical to 0.2.0 at 4x.
- **Old projects**: the parameter files are byte-identical to 0.1.0 / 0.2.0. `ComplicatorCheck` (a console program
  built in the Actions run, `check/`) compares the control list with a stored copy, restores 0.1.0 / 0.2.0 saved
  states, and exercises presets, menus and the panel.

### CPU and the Quality menu

Measured on the GitHub Linux machine, % of one core at 48 kHz, a sustained patch (guide only; Intel Macs differ):

| Quality | Oscillators only | Everything, panel open | Everything plus loops |
|---|---|---|---|
| 2x | 1.0 | 1.4 | 1.3 |
| 4x (default) | 1.8 | 2.4 | 2.4 |
| 8x (heavy) | 3.3 | 4.4 | 4.4 |

Measured on the Intel Mac (Activity Monitor, Logic Pro % CPU, 100% = one core; "Five Modules" held, Env amp):
baseline with no note 16.5% (15.5% fully idle), 2x 16.2%, 4x 21.6%, 8x 26.7%. So the plugin costs about 0% at 2x
(within the noise), about 5 points of one core at 4x (the default) and about 10 points at 8x. Safe at 4x; 8x doubles
the cost of 4x and is what the "heavy" mark is for.

Aliasing (non-harmonic energy, high notes): 2x about -53 dB, 4x about -74 dB, 8x about -88 dB.

Reading it on the Intel Mac: open Activity Monitor, CPU tab, find **Logic Pro** (the plugin runs inside it), and read
**% CPU** (100% = one core). Play a held note on a Drone patch at 2x, 4x and 8x, and subtract the figure with
no note playing.

## What is new in milestone 2

Signals inside are plus/minus 1 (the module's plus/minus 5 V). Envelope, velocity, mod wheel and aftertouch are 0 to 1.

- **Wavefolders.** Each oscillator's sine goes through its own folder; what comes out is the **Final** output. The
  Wavefold fader sets a gain in front of the folder: far left is near silence, the centre is exactly a plain sine, far
  right multiplies the sine by 10, so it folds over many times (rich). The **Wavefold CV amount** scales the signal at
  the Fold CV input and adds it to the fader (negative values invert it); the fader is limited to its own range.
  **Symmetry** adds a bias before the folder. By default the bias is a fixed reference (Symmetry at 100% = a sine's
  peak). An Index route or a Patch source replaces the reference, and Symmetry then scales that signal (up to 3 times
  its size). So with the fader at the left and an external signal into Sym A or Sym B, the folder works as an effect on
  that signal alone.
- **Multiplier.** The Carrier input is Osc B's sine, the Modulator input is Osc A's sine, unless patched. **Ring**
  multiplies as it is; **Half** cuts the negative half of the Modulator first; **Full** turns it over. **Depth** is the
  gain on the Modulator: unity (a full-size sine in gives a full-size sine out) at 72%, about 2 o'clock, clean up to
  there; above that the product is soft-clipped, with a ceiling of 1.25.
- **Index.** Two linear VCAs on the sines, both set by Index = the Index knob + Index CV amount times the signal at the
  Index input (Patch row), kept between 0 and 100%. The routing row decides where the Index-scaled sines go (Osc A's
  sine always goes to Osc B's inputs and the other way round): **FM A / FM B** (when off, the other oscillator's sine
  goes straight in, scaled only by the FM knob), **Fold A / Fold B** (into the Fold CV input), **Sym A / Sym B**
  (replaces the reference), **PWM** Off / Both / A to B only (into the PWM CV inputs).
- **Patch row.** 13 menus, one per input: FM A, FM B, PWM A, PWM B, Fold A, Fold B, Sym A, Sym B, Index, Carrier,
  Modulator, Hard sync, Soft sync. 18 choices each: **Normal** (the module's own connection: see above; for Index, Hard
  sync and Soft sync it is the Index CV (nothing) or the Sync switch), **Off** (silence), A Sine / Tri / Saw / PWM /
  Final, B Sine / Tri / Square / PWM / Final, Multiply, Envelope, Velocity, Mod wheel, Aftertouch, Sidechain. Any source
  can go to any input; a patched input replaces the normal connection. Loops (a Final back into an FM input, Multiply
  into its own modulator, and so on) are allowed and use the previous oversampled sample; they stay bounded.
  Changing a menu, an Index route or the multiplier mode is cross-faded over a few milliseconds, so it does not click.
- **Sync through the Patch row.** Osc B's Square into Hard sync and Osc A's Sawtooth into Soft sync are exactly what the
  Sync switch does and run the same exact path. Any other source triggers on a rising edge through 2.5 V (hard) or 2 V
  (soft), found to a fraction of a sample.
- **Scopes** for Final A and Final B, drawn steady over one or two cycles (a slow signal scrolls). They record only while
  the panel is open.
- **Mixer.** Final A, Multiply and Final B now have their levels. They start at 0, like the other new outputs, so raise
  Final A or Final B to hear a folder.
- **Sidechain.** The sidechain input bus is the Patch source "Sidechain" (summed to mono). If Logic offers a Side Chain
  menu for COMPLICATOR, route a track there; if it does not, the source is silent.
- **Band-limiting.** The folders, the multiplier's soft clip and the rectifiers use first-order antiderivative
  anti-aliasing on top of the 4x oversampling: the engine test measures the heavily folded sine at about 86, 78 and 67 dB
  below the signal for notes C4, C5 and C6, which is about 23 dB better than a plain fold at the same oversampling.
- **CPU.** A stage nobody listens to is not run: a Final whose mixer level is 0, that nothing is patched from and that has
  no open panel, and the multiplier likewise. The engine test measures about 2.3% of one core at 48 kHz on the Linux build machine
  with everything on (the same test on the 0.1.0 engine: 1.4%). Not yet measured on the Intel Mac.

## What works (milestone 1)

- **Two oscillators.** Osc A is a sawtooth core (outputs: sine, triangle, sawtooth, PWM pulse). Osc B is a triangle
  core (outputs: sine, triangle, square, and a "PWM" staircase triangle whose top and bottom steps widen with its PWM
  knob). Each has Coarse (±48 semitones), Fine (±1 semitone) and PWM.
- **Keys play A / A+B / B** (the module's Link button). An oscillator that does not follow the keys sits at MIDI note 48
  (about 130.8 Hz) plus its Coarse. One note at a time, last note wins; releasing a note goes back to the one still held.
  Pitch bend ±2 semitones.
- **FM**: each oscillator's sine feeds the other's FM input, as on the module. FM amount knob and a curve switch:
  LIN, EXP, or TZ (linear through-zero, which the module does not have: the frequency can pass through zero and run
  backwards). LIN stops at 0 Hz. In TZ the tone changes clearly from about 45% FM amount upwards: from there the other
  oscillator's sine pushes the frequency below zero for part of each cycle. That is the through-zero sound, not a fault.
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

Nothing more is planned: milestone 3 completes the plan. Cosmetic changes to the panel (knob looks) may follow.

## Folders

| Folder | What it is |
|---|---|
| `engine/` | The audio engine. Plain C++, no plugin code: oscillators, band-limiting, wavefolder / multiplier maths, decimation filter, pitch rules, envelope, Patch row. Reusable for later plugins. |
| `plugin/` | The plugin wrapper: what Logic sees, the list of controls (`Parameters`), and the panel (`PluginEditor`, `Controls`, `Style`). |
| `check/` | `ComplicatorCheck`: a console program that tests the plugin layer (parameters, presets, saved states, panel) without a host. |
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
- Double-click a knob to return it to its default; hold Shift while dragging for finer moves. Drag the bottom-right
  corner to resize the window.
