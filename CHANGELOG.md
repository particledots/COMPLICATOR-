# Changelog

## 0.1.0 — milestone 1 (2026-11-17, waiting for a test in Logic)

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
