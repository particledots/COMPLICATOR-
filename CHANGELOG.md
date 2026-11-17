# Changelog

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
