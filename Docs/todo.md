# TODO

One line per item. Measurements and reasoning go in `findings.md`, design in `design.md`.

## First
- P012 (CT 2026-10-23): resonance tone higher on the instrument - measured: the instrument's filter has drifted up since 10-17 (+0.6 semitone at cutoff 0 to +0.1 at 88; findings 2026-10-23), the law matches 10-17. NEXT: CT runs TUNE FILTER on the instrument, then re-measure cutoff 0/40/64/88 at note 60
- P012 (CT): left alone ~10 s, the instrument's first note sounds as if the filter glides in - what resets or drifts while idle (its slow LFO 1 drives VCF ENV AMT and RESONANCE; glide 10 on the pitch the keytrack follows)
- Filter: the near-cutoff 3rd harmonic (`postsat/`, -46 to -34 dB rising with level) is 10-25 dB short since the post-filter cubic went. Saturation in the stages fits it (D 16: 2 dB rms) but drops the self-oscillation 2-5 semitones (findings 2026-10-23): needs the cutoff law, the resonance curve and the saturation refitted together, the oscillation held on the law as TUNE FILTER holds it
- LEVEL2 (P006, P015): the coupled linear law is fitted (p2Plugin notes §52); CT to check by ear. Left: the engine's slew and loop saturation compress loud fast signals more than the instrument (2 kHz LEVEL2 takes, open triangle) - refit with the filter (see the Filter item)
- P010 (CT): still brighter on the plug-in; osc 1 alone, filter open, different - PARA-4 with LFO 2 on PW1 (amount 94) and PW2, so the width modulation's depth and the paraphonic voices
- P002 (CT): close; perhaps the start of the note (attack) - not certain
- P003: the drive, as in Drive below
- P001: osc 1 and osc 3 alone, filter open, are 1.1-1.4 dB and 3.3 dB louder in the engine at every note - measure the oscillators' level laws across their range (P001 sounds much better since the pulse fixes, CT)
- tools/do-filtercheck fails "LP24 c64 r96 bass loss at 40 Hz" (-13.67 dB, wants -9.1): the target is the compressed -30 dBFS take; the -45 dBFS retake (k 4.01) gives ~-14.0 - confirm from the captures and update it
- Filter envelope short attacks: measure with noise through the filter (p2Plugin notes §43); P004's burst; redo `fenv_attack/` with Filter Velocity set (at 0 the envelope did not open)
- The first ~4 ms of a note over a release: the instrument's first half-cycle is smoother (P004)
- P006: 1-4 kHz 3-7 dB bright (resonance 41 / filter keytrack 95 at cutoff 0?); first 150 ms ~3 dB quiet; 2nd-4th harmonics 4/5/10 dB under over 7.5 s (how osc 2, osc 3 and the noise combine)
- Presets, still to do (design §7): Store Sound to Pulse 2 Slot and Restore Presets to Pulse 2 (flash writes - wait for CT's go-ahead); the rest is built
- The thump: which control leaks (oscillators and noise off, one envelope at a time) - p2Plugin notes §43
- Master Tune on the GLOBAL page: 420-460 Hz, as the instrument's
- Transpose on the GLOBAL page: +-24 semitones, as the instrument's
- Polyphonic mode: a full voice (filter, envelopes, VCA) per note - beyond the instrument, whose PARA-8/PARA-4 share one filter and one VCA; design first (voice count, CPU)

## Drive
In order (agreed 2026-10-20; the shapes of p2Drive notes §6 are in and sound better, not perfect):
1. P003 on NONE is ~10 dB darker above 250 Hz than the instrument, so FUZZ gets a dull input (+13.5 dB at 1-4 kHz against +24.5): find what upstream - the filter near cutoff 24? Likely the same as P001/P014 not brightening at note 48 (First)
2. The quiet-input loss depends on pitch: notes 72/84 lose 2-3 dB less than note 48 (p2Drive notes §5) - fit the multi-frequency EXT captures (`192k/drive/drive_*_f{100,300,1000,3000}.wav`) for a coupling capacitor's frequency dependence, then let the meter or knee follow
3. FUZZ at quiet inputs (-30 dBFS EXT): even harmonics ~15 dB above the instrument's - a structure where the bias sets the coupling loop's operating point without lopsided clipping (e.g. the bias on the coupling path, not the clipper) - p2Drive notes §6
4. TUBE: its fitted bias sits at the fit's bound and it is 8 dB rms off on harmonics - a bias that follows the signal's level (its duty changes with Drive); a fixed bias per Drive went degenerate - p2Drive notes §6
5. Rig: the settling - long repeated quiet notes at a few Drives (FUZZ's gain climbs ~3 dB over 0.75 s; does the previous note matter?) - way 2 of p2Drive notes §4; with it, sines 50 Hz-5 kHz and a two-tone through EXT at stepped levels
6. Rig: osc 3 on DRIVE raises the baseline by about 8 drive units at any level - find why

## Rig
- Build a known test sound over CCs (one osc, semitone/detune centred, filter open, envelopes flat) before every capture
- Read INPUT GAIN off the panel and record it with the EXT captures; sweep the EXT level per step

## Oscillators
- Osc 2 X3-PWM and X1-PWM are not built: list index 3 plays APW (osc 1's shape 3) and 4 a saw - p2Plugin notes §8
- Model the ramp's shape and reset spike against pitch (the pulse's duty is settled - design §2.2)
- How PWM from the mod matrix adds to the width word (the dial is dial << 8, confirmed)

## Filter
- Retake the LP12/BP12/HP12 resonance-92 and LP24 resonance-88 sweeps at −45 dBFS (LP24 confirmed compressed at −30)
- Level sweeps (−40..−6 dBFS) per mode at a few settings, for the nonlinearity
- LP12's stopband: ~2.5 dB high at 1-2.5 kHz at cutoff 0
- The modulation step shape, more closely: a faster LFO (larger steps), and the same test on pitch modulation

## Engine
- Mod matrix: per-target depth at full scale - the amount law itself is measured (p2Plugin notes §51) and the full-scale depth on pitch (128 semitones per unit) and M-ACCU (1); the other targets' full-scale depths are provisional
- Capture to confirm the math sources: M-DELAY's time against amount (16 ms per step?), M-SMOOTH's rate, M-PRODUCT/MIN/MAX on two LFOs
- Capture to confirm: SNG2/RET2 restart from zero, RET3's exception, FNON/FOFF glide, LFO 2's delay (a wait before it starts?)
- Osc 3's LEVEL2 / VCF / DRIVE depths and FB (depths are guesses beyond LEVEL2 at 8 Hz)
- FB alone sings on the instrument from FB 64 (cutoff 64, ~950 Hz, -40 dB); the engine does not - frequency-dependent feedback?
- Amp Velocity: the engine moved P011 1.7 dB more than the instrument (velocity 100, Amp Velocity 95) - check the law away from velocity 64/127
- Arp Pattern (0-14): what it selects
- Arp SOFT and HARD: measure their velocity offsets (the plug-in uses -32 and +64)
- Arp, by program change: does a chord kept by one HOLD sound start again after a non-arp sound, in the next HOLD sound? (the plug-in does so with Anomalies on - p2Plugin notes §45)
- The 1 kHz leak (−96 dBFS with a note sounding): decide whether to model it

## Plug-in
- Link: send the program change on all 16 channels, so the P number follows whatever the Channel setting (only the Pulse 2 listens on its USB port) - offered to CT 2026-10-20, undecided
- Value text in real units (times, rates, semitones) where the laws are measured; the panel shows raw values now, as the instrument does
- SynthLib: maximise the C and reduce the C++ (CT 2026-10-21) - port src/fileBrowser.cpp, bankBrowser.cpp and prefs.cpp to C; the VST3 wrapper and views stay C++/Obj-C++ as the SDK requires

## LV2 / MOD Dwarf (p2Lv2 notes)
- Measure the CPU on a real Dwarf: the table in notes §13 is from qemu instruction counts at an assumed IPC; then confirm or change the 2x default
- Play it on a Dwarf: MIDI from a keyboard, MIDI learn, presets on a footswitch, the arp following MOD's tempo
- The panel (modgui, notes §16) on a real device: the pages, a knob turned from the Dwarf following on its page, the look on a tablet; MOD OS older than 1.12 shows still pointers
- Raise `P2_LV2_MINOR_VERSION` to 2 once it has been played on a device, so MOD lists it as stable rather than testing (notes §1)
- The arp's steps as editable ports (event, time, glide per step) once there is a step editor - now 16 raw bytes, hidden (notes §4)
- Pan, once the engine is stereo - a port now, kept with the sound and doing nothing

## DO NOT RE-TRY
- Four one-pole stages with LP24 at the end (per-stage poles): worse than the free-mix fit, implausible stage ratios, no HP notch (findings, first linear filter fit)
- Fitting an engine constant to one take of a patch with detuned oscillators: their slow beat moves a band by up to 10 dB take to take (P006's LEVEL2 depth of 3, reverted)
- Drive structures already rejected against the level sweep - p2Drive notes §4
