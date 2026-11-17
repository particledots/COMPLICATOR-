# Changelog

## 0.2.0 — milestone 2 (built 2026-11-17, waiting for the Logic test)

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
