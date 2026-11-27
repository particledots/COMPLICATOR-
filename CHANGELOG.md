# Changelog

## 0.3.1 — new panel look (built, not yet tested in Logic)

- Panel restyled in the "gold sections" look of Anamnesis 0.3.1: gold frames with titles, flow lines, cream knob caps in
  gold range arcs, blue lamps as the only colour, Marcellus title, Space Grotesk labels (fonts built in).
- Layout: Preset and Quality in their own row; Index and its CV amount and routes in their own section; Sub, Multiply
  mode and PWM route as lamp lists; Patch menus have their names above them. 680 x 708 units, opens at 120%.
- No control, id, order, range, menu list or plugin / manufacturer code changed. Saved projects load as before.
- The fixed artwork is a stored picture; only the scopes and Coarse texts are redrawn 30 times a second.
- ComplicatorCheck: title fits at five sizes (drawn pixels), all texts fit and none touch, no controls overlap, drawing
  cost; the Actions run now uploads panel pictures. 2326 checks, 0 failed.

## 0.3.0 addendum — 22 more factory presets (built and tested in Logic 2026-11-17)

- **22 new factory presets** of my own (not from the manual), appended after the nine existing ones: Glass Bell,
  Tine Piano, Metal Gong, Fold Pluck, Velocity Brass, Reed Organ, Hollow Flute, Pressure Vibrato, Wheel Folder,
  Sync Sweep Lead, Soft Sync Fifth, Stair Pad, Open Fifth Drone, Deep Sub, Wobble Bass, Acid Fold, Sine Kick,
  Laser Zap, Metal Hat, Dub Siren, Tangle, Ring Voice (sidechain in). The Preset menu now has 32 entries (Init + 31).
  The order of existing entries is unchanged, so saved projects keep their preset choice. Level matched (about
  -19 dB RMS), and they never touch Pitch Snap, Scale, Key or Quality.
- No parameter, panel or plugin-layer change; version number still 0.3.0.
- Engine self-test extended to 3285 checks: all 31 presets (level, finite, bounded, every quality; struck sounds are
  level-checked over their first half second) plus one behaviour check per new preset. Plugin-layer check 2305 checks,
  auval and pluginval (strictness 5, AU and VST3) passed in Actions run 44244367907.
- Logic test result (Bryn, 2026-11-17): everything works.

## 0.3.0 — milestone 3 (built and tested in Logic 2026-11-17)

- **Factory presets** (9) and an **Init** entry in the now-live Preset menu; also offered as AU / VST3 programs. Presets
  set their own Final levels and Volume (level matched, about -19 dB RMS) and never touch Pitch Snap, Scale, Key or
  Quality. The choice is saved with the project; older projects show "Custom".
- **Quality menu** live (2x / 4x / 8x, 4x default, 8x marked heavy). All three decimators are prepared up front; a change
  fades out over 4 ms, switches while silent and fades in. Latency stays about 19 samples. No parameter id, order, range
  or menu list changed (65 parameters, all version 1).
- **Soft sync**: Osc B is pulled half way to the peak at each turn, so a fifth, fourth, third and sixth lock into their
  natural pattern. Sound with soft sync off is bit-identical to 0.2.0 at 4x.
- Logic test result (Bryn, Intel Mac, 2026-11-17): every step worked as described: old project unchanged, all presets
  good and level matched, sidechain preset works, preset saved with the project, Init/presets leave Snap/Scale/Key/Quality
  alone, no clicks switching Quality or under heavy automation, 8x shows "heavy", soft-sync fifth stops beating.
  Intel Mac CPU (Logic Pro % CPU in Activity Monitor): baseline 16.5, 2x 16.2, 4x 21.6, 8x 26.7.
- The "M3" tag on the panel is gone (nothing is dimmed). No other panel change.
- New `ComplicatorCheck` console program and a "Plugin-layer check" job in the Actions run: golden control list,
  restoring 0.1.0 / 0.2.0 saved states, presets, menus, panel snapshots.
- Engine self-test extended (891 checks): quality, aliasing per quality, quality changes while playing, soft sync at
  simple ratios, every preset (finite, bounded, level matched, pitch), CPU per quality.

## 0.2.0 — milestone 2 (built and tested in Logic 2026-11-17)

- **Wavefolders** on both oscillators with Symmetry, Wavefold CV amount and the Final outputs. Fader: left = near
  silence, centre = a plain sine, right = rich (gain up to 10x in front of a triangle folder). Band-limited with
  first-order antiderivative anti-aliasing on top of the 4x oversampling.
- **Multiplier**: Ring / Half-wave / Full-wave on the Modulator input, Depth with unity at 72% (clean up to there) and
  a soft clip above it (ceiling 1.25).
- **Index**: the two VCAs on the sines, the Index knob with its CV amount (CV = the Patch row's Index input), and the
  Index routing row (FM A, FM B, Fold A, Fold B, Sym A, Sym B; PWM Off / Both / A to B).
- **Patch row**: 13 inputs, 18 sources (A Sine / Tri / Saw / PWM / Final, B Sine / Tri / Square / PWM / Final, Multiply,
  Envelope, Velocity, Mod wheel, Aftertouch, Sidechain). "Normal" is the module's own connection. Loops use the previous
  oversampled sample. A changed menu or route is cross-faded over 6 ms.
- **Scopes** for Final A and Final B (recorded only while the panel is open), and the Final A, Multiply and Final B mixer
  levels. The Sidechain input bus (declared since 0.1.0) now feeds the Patch source "Sidechain".
- The controls of this milestone are no longer dimmed. No parameter id, order, range or menu list was changed (still
  65 parameters, all version 1): projects saved with 0.1.0 open unchanged.
- CPU: a stage nobody listens to is not run (a Final whose mixer level is 0, that nothing is patched from and that has no
  open panel; the multiplier likewise).
- Oscillators: audio-rate PWM (from the PWM CV inputs) no longer leaves unsmoothed edges on the pulse and the staircase;
  Osc B's staircase starts on the right step (a small start-up step in 0.1.0).
- Logic test result (Bryn, 2026-11-17): every step worked as described; nothing blew up; no clicks or dropouts with heavy
  automation; the resized panel stays in proportion; the Side Chain menu is available. The only surprise: the folders
  are silent until Final A / Final B is raised, because the Final levels start at 0 (kept as is). Intel Mac CPU: the
  Logic meter shows no numbers; Bryn's estimate is 5% or less. Engine CPU on the Linux build machine: about 2.3% of one
  core at 48 kHz with everything on.
- Engine self-test extended to 181 checks: the folder, soft clip and rectifier maths, folder level / silence / richness /
  symmetry / aliasing (against a plain fold), the multiplier (ring, half, full, depth, clipping), every Index route, every
  Patch source (envelope, velocity, wheel, aftertouch, sidechain, and all 17 sources into all 11 inputs), patched hard and
  soft sync, feedback loops (bounded, repeatable), clicks when menus / routes / modes / faders change, heavy random
  automation of everything at 44.1, 48 and 96 kHz, and CPU.

## 0.1.0 — milestone 1 (2026-11-17, tested in Logic)

Tested by Bryn on both Macs (M4 Mac mini, macOS Tahoe, Logic 11.2.2; Intel Mac, macOS 12.7.8, Logic 10.7.8):
install, panel, all outputs, PWM, FM (Lin / Exp / TZ), hard and soft sync, Sub, LFO, Keys, Pitch Snap / Scale / Key,
fast automation (no clicks), save and reopen: all good. The clear change of tone in TZ from about half FM amount is the
expected through-zero sound. CI: engine test 46/46, auval passed, pluginval level 5 passed (AU and VST3).


- First build: repo layout, build workflow (engine self-test, universal AU + VST3, ad-hoc signing, auval, pluginval,
  package with installer and INSTALL.txt, private draft release), same installer as the other particledots plugins.
- The real panel, matching the approved draft. Controls for milestones 2 and 3 are in place, dimmed and labelled.
- Full parameter list fixed (65 parameters), including everything for milestones 2 and 3, and a sidechain input bus.
- Osc A (saw core: sine, triangle, sawtooth, pulse) and Osc B (triangle core: sine, triangle, square with Sub,
  staircase "PWM" triangle), Coarse / Fine / PWM, cross FM (Lin / Exp / through-zero), LFO mode, soft and hard sync,
  Keys (A / A+B / B), pitch bend.
- Pitch Snap (Off / Semitones / Steps), Scale (28 scales) and Key.
- Output mix for the plain waveforms, ADSR envelope, Amp (Drone / Env), Volume.
- 4× oversampling, band-limited waveforms, smoothed controls.
