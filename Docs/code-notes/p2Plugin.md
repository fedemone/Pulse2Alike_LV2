# p2Plugin.c - notes

## 1. The parameters
Ranges follow the instrument's, 0..127 dials where it has them, so a captured setting can be typed in. The
ids are saved by projects: append, never insert.

## 2. Width from the dial
The dial value shifted into the width word's top byte, confirmed against the instrument across PW 16-127
(findings 2026-10-17).

## 3. Note priority
Last note, with a stack so releasing the top note returns to the one held under it. The instrument's own
priority and retrigger behaviour (GLIDE MODE, ENVF/ENVA TRIGGER) are not yet modelled.

## 4. Oscillators into the filter
The DCO mix (±1 at full level) is scaled by 0.3 before the filter, which sets how hard the oscillators drive
its saturation. Provisional, until the level captures (osc levels 127/64/32) say how hard the instrument's
oscillators drive it.

## 5. Oversampling
The voice (oscillators and filter) runs at 4× the host rate, and comes down through a 64-tap Blackman-windowed
sinc that cuts at 0.45 of the host rate. Fixed for now; selectable later for CPU (todo).

## 6. The gate is the VCA
The oscillators and the filter run whether or not a note is held, as on the instrument. The gate multiplies
the filter's output, so a self-oscillating filter is silent until a note opens it.

## 7. Cutoff and resonance glide
Both are continuous parameters, not 127 steps, and the filter follows them through a 3 ms one-pole glide
updated once per host sample. Stepping a whole semitone per CC value, applied instantly, zippered audibly (CT,
2026-10-17). The instrument itself moves its cutoff smoothly between MIDI steps, so a glide is the faithful
choice as well as the clean one.

## 8. Shapes not built yet
The shape lists are the instrument's (10 for osc 1, 5 for osc 2), but the DCO plays only PWM, SAW and TRI so
far, plus APW. Any other entry plays a saw until its mode is built (unison, X1/X3-PWM); PARA-8/4 are voices (§12).

## 9. Pitch offsets and levels, as measured
Semitone and detune are offsets to the pitch before the period law, detune 2/256 of a semitone per step, which
is why it inherits the law's mid-semitone error exactly as the captures show. The level law,
`((CC − 12.6) / 114.4)^0.864` and silent below that, was read at the output, so it may still hold some of the
filter's compression (findings). Osc 3's SQR is the DCO's pulse at its widest width, pending a capture.
Noise is white (xorshift), through the same level law, also pending a capture.

## 10. The control tick
Envelopes and the cutoff target are worked out on a control tick: every 1 ms with Anomalies on, which is the
instrument's measured update rate, and every host sample with it off. The VCA and the cutoff then follow their
targets through a one-pole with a 0.3 ms time constant, the measured smoothing of each step (findings,
"cutoff modulation"). So with Anomalies on, modulation reaches the filter as the instrument's softened 1 kHz
staircase. The cutoff target is the knob (after its 3 ms glide), plus the filter envelope × amount, plus
keytrack × (note − 60). The amount's range (±127 steps at full) and the keytrack reference note are
provisional, pending the filter-envelope session.

## 11. Triggering
SNG1 and SNG2 trigger the envelope only on a fresh note (no other key held), and are legato otherwise. RET1,
RET2 and RET3 trigger on every note. SNG2 and RET2 restart the attack from zero; the others start it from
wherever the envelope is. The SNG/RET split is measured (findings, "triggers"). The from-zero variants and
RET3's exception are not yet confirmed by capture, so RET3 is treated as RET1 for now (todo).

## 12. Paraphonic voices
PARA-8 and PARA-4 (osc 1 shapes 8 and 9) run 8 or 4 voices into the one filter, VCA and pair of envelopes, as
measured (findings, "paraphonic modes"). A PARA-8 voice is one pulse at osc 1's pitch offsets and width. A PARA-4
voice adds a second pulse at osc 2's, scaled by osc 2's level relative to osc 1's. A held note again reuses its
voice. A new note takes a free voice, else steals the most recent, which is what the captures show. Voices fade
in and out over a provisional 20 ms, until OSC1/OSC2 ENVA FADE is measured. The envelopes keep the mono trigger
rules (the first note from silence, or every note in RET modes).

## 13. The mod matrix
Eight slots, each source × (amount − 64)·|amount − 64| / 32, summed per target on the control tick, in the
target's own units. Measured on three targets, all the same law: cutoff (steps), pitch (semitones) and osc
level (dial steps, then the level law; findings, "mod matrix scaling"). The same law is used for the other
targets (PW, resonance, volume, LFO speed, env amount) until they are measured. Sources built: LFO 1, LFO 2, LFO 2 × amp env, both envelopes, velocity,
keytrack and pitch follow ((note − 64)/64). The controller sources (mod wheel, pressure, bend, breath, CTRL X)
and the math sources read 0 until they are wired.

## 14. LFO 2
LFO 2 has no shape setting; it is taken as a triangle. Its delay is a fade-in from the first note, over the
attack time law (provisional, unmeasured).

## 15. Volume, velocity and the VCA
Measured (findings, "velocity and volume"): the level is one curve of envelope × effective volume, where
effective volume = volume + velocity × (amp velocity − 64)/64. The volume law is that curve. It also predicts
the sustain table (measured at volume 40) to about 1 dB, which is why one curve serves both. Filter velocity
adds (value − 64) × velocity / 128 cutoff steps.

## 16. Loading a patch
`p2_load_patch()` reads the `.syx` (`p2Patch.c`) over the current values and hands every parameter to the host
through `synthlib_plugin_param_edited()`, so the host sees the change (automation, undo) and the processor
receives it the normal way. The plug-in's own settings (oversampling, Anomalies) are left alone.

## 17. Glide
Measured (findings, "glide"): every millisecond the pitch moves towards the note by x = distance × k(rate) / 2^17
(1/256-semitone units), rounded as floor(x) + ceil(x), so a glide never stalls short of the note. It moves at
least one unit a millisecond (3.9 semitones a second), which is all the slow rates ever do over an octave.
k(rate) × 2^17 = 2^(17.129 − 0.12697 × (rate + 10)) up to rate 74, then 265.16 − 2.0916 × (rate + 10). Against
the captured glides (48 → 60 at rates 48-112) this law is within 0.01-0.06 semitone rms. OFF jumps, NORM always
glides, and FNGR glides only on a legato note. FNON and FOFF are taken as FNGR until what separates them is
measured.

## 18. The math destinations and sources
M-ACCU, M-PRODUCT, M-DELAY, M-SMOOTH, M-MIN and M-MAX are mod targets (25-30) whose results are sources (17-22).
The slots run in order, so a slot sees what earlier slots fed in this tick.
- **M-ACCU, M-PRODUCT, M-MIN, M-MAX:** the sum, product, lowest and highest of their inputs, where an input is
  source × ±weight(|amount − 64|) / 256 (§51), clipped to ±1.
- **M-DELAY:** a slot writes its source, inverted for amounts below 64, |amount − 64| × 16 ms ahead into a
  1024 ms line; the output is what arrives now.
- **M-SMOOTH:** a lag on the slot's source, moving (k + k²/256)/2048 of the way per millisecond, k = |amount − 64|.

K-HIGHEST and K-LOWEST are the highest and lowest held keys, (note − 64)/64. MOD1 AMOUNT shifts slot 1's amount.
These follow the instrument's behaviour as far as studied. M-DELAY's timing and M-SMOOTH's rates are to be
confirmed by capture (todo).

## 19. Output gain and Master
The final gain is 3.926: the instrument's own level, as captured through the rig (Fireface inputs at +4 dBu, the
same path the instrument is heard through). The same saw at Volume 40 measured −14.1 dBFS on the instrument and
−22.0 from the engine at the previous 1.58 (which was itself +10 dB on the first version's 0.5), so +7.9 dB. Master Volume (default 127) is the plug-in's own
volume after everything else, with a squared taper (−12 dB at 64, −6 dB at 90). The sound's Volume stays as the
instrument's.

## 20. Program change
Bank select (CC 32) and program change pick a sound as the instrument does, as measured: P(100 × (bank mod 5) +
min(program, 99) + 1). Bank 1 program 0 is P101, programs 99 and 100 are both P100, and bank 2 program 5 is P206.
A program change to the sound already loaded (by program change, or by opening its library file) is ignored, so
an edited sound is not thrown away by a host re-sending the same program.
The presets are read once per process into a cache (p2Patch notes §2, §6). A program change only copies a cached
sound, on the audio thread with no file access, then tells the host of each parameter it changed. Program and Bank
are hidden and never saved, so reopening a project restores its sound as edited rather than reloading the program
(the preset number it came from is saved, §48).

## 21. Unison
Measured (findings, "unison"): UNISON and UNIAPW are eight pulse (or APW) oscillators at osc 1's width, spread
symmetrically about the note, Unison Detune × 1.25 / 256 semitone apart (15.6 cents at 32, 31.1 at 64, 62 at
127). M is monophonic: the newest note, with glide. P shares the eight between the held notes, which so far is
provisional: they are split evenly, and the captured chord was more lopsided than that. Each oscillator is scaled
so the eight come out near the measured level, about 5.5 dB below one pulse at full level. Osc 2 and osc 3 play
as usual alongside.

## 22. Sync
OSC3 SYNC>OSC2 makes osc 3 the master of osc 2: osc 3's main edge restarts osc 2, which holds at the end of each
cycle until then (p2Dco notes §8). Osc 3 runs whether or not its level is up, as on the instrument, where sync
works with osc 3 silent.

## 23. Saving
A save writes the parameters through p2_patch_to_syx, keeping the unmodelled bytes of the dump the sound came
from (a file, or the library's copy after a program change). Saving into the library folder under a number
updates that program at once. Init sets every sound parameter to its default and forgets the dump.

## 24. Name, file and the plug-in's own state
The sound's name and its file belong to the instance: the panel shows this instance's, every sync. Both go in
the plug-in's own block of the saved state ("P2S1": name, the kept dump, path), so a reopened project shows its
name and file, and saves as before. A rename changes only the name; it reaches a file at the next save.

## 25. Osc 3's routing and FB
As the manual describes them: MIX into the mixer; LEVEL2 adds osc 3 to osc 2's level; VCF adds it to the cutoff
(audio-rate filter FM, the filter recomputed every oversampled sample); DRIVE adds it to the drive amount. Only MIX
puts osc 3 in the mixer. On the three control routings osc 3 is AC coupled (a high-pass near 14 Hz), measured with
osc 3 as an 8 Hz square on the self-oscillating filter: each edge kicks the cutoff and it decays back in about
30 ms. The depth is 125 cutoff steps per unit of osc 3 (+-1) at full level, scaled by the oscillator level curve. The
square's kicks read 28 off the self-oscillating filter's pitch track, which cannot follow a kick that decays in
11 ms; P002's audio-rate FM (osc 3 four octaves up on VCF) matched the instrument only at 112-168 (brightness within
3 dB at notes 48/60/72 at 125). LEVEL2: osc 2's level plus osc 3's level x osc 3, floored at zero, as the 8 Hz square measured (8 dB apart at level
32). A depth of 3, fitted to one band of one take of P006, was tried and reverted: CT heard P006 go further wrong.
DRIVE: 30 drive units per unit of osc 3 at full level, measured (findings, "osc 3 to DRIVE"). FB, osc 3's fifth
shape, SUBTRACTS the drive's output from the mixer (negative feedback, findings "FB"), at 5.7 × osc 3's level:
with a saw that matches the instrument's level within 1.4 dB at every FB. The instrument also sings with FB alone
(from FB 64 at cutoff 64, near 950 Hz, quietly), which one gain cannot do: its feedback must be stronger near the
filter's corner than in the passband. Not reproduced yet. A tiny noise seed lets a loop start, as the
instrument's noise floor does.
## 26. The paraphonic voice fade
Measured (findings, "voice fade"): in PARA-8/PARA-4 a released voice dies away while the others hold the amp
envelope open, as a release stage of R − 5.5 − 0.61 × (127 − Fade), R the amp envelope's Release: from two points
(Fade 127 and 100 at R 70) and checked by the rest (Fade 83 almost at once, 64 and below at once, R 40 at once
whatever Fade). Osc 2's part of a PARA-4 voice uses osc 2's Fade. The fade in is not changed by Fade. When the
last key is released no voice fades on its own: the amp envelope's release is the release (CT, P010: the pad's
release was far shorter than the instrument's when every voice faded by itself).

## 27. LFO phase at a note
What restarts an LFO is the FILTER envelope's trigger, following that envelope's Trigger mode (SNG on a fresh note
only, RET on every note). When it triggers, LFO 2 restarts at its Phase setting and its delay starts again, and
LFO 1 does too if its shape is one of the fast five. The slow shapes (sSIN on) are never restarted by a note: they
run free whatever their Phase, which is why two identical takes of P011 (sTRI) started at different points.
## 28. Performance controls
Pitch bend, the mod wheel, pressure (channel aftertouch), breath (CC 2) and the sustain pedal (CC 64) are hidden
parameters mapped to their MIDI controls, which is how a VST3 host delivers them (the AU passes the same MIDI to
the same parameters); they are played, never saved, and a program change or Init leaves them alone. Bend moves
every oscillator by Bend Up or Bend Down semitones at full throw, and is also the PITCHBEND mod source (-1..+1).
The wheel, pressure and breath are mod sources 0..1, and LFO1*MODW / LFO1*PRS are LFO 1 times the wheel or
pressure. V.RELEASE is the last note-off's velocity. The pedal holds released notes until it is lifted (64 and
up is down). CC 7 is the sound's Volume, as on the instrument. CONTROL-X (a global choosing a CC) is not wired.
Depths are as the mod matrix's own; none of this is measured against the instrument yet.

## 29. Filter velocity scales the envelope
Velocity's term, (Filter Velocity − 64) × velocity / 128 steps, is added to the envelope amount's and the sum is
multiplied by the filter envelope: velocity makes the envelope deeper or shallower, as the manual's "the overall
modulation ... is the sum of both the ENV F Amount and Filter Velocity" says. With the envelope at full (the
earlier velocity captures, sustain 127) it reads as a plain offset, which is how it was first modelled. P011 showed
the difference: its envelope sustains at 47, where velocity adds about 9 steps, not 25 (findings, "P011").

## 30. The pulse's level
On the instrument a pulse is quieter than the saw against which the plug-in's output was set: with the filter
open, PWM (width 64) measured 4.4 dB and APW 4.2 dB (every width) below what the engine made, while the saw and
triangle agree. Every pulse in the mix (osc 1's PWM and APW, the unison and paraphonic voices, osc 2's PWM, osc 3's
SQR on MIX) is scaled by 0.525 (first set to 0.61 from older captures; a same-session comparison at widths 8-127,
PWM and APW, put every pulse 1.3 dB loud at 0.61, the saw within 0.2 dB). Only in the mix: osc 3's square as a control (LEVEL2, VCF, DRIVE) keeps its full
size, which is what those depths were measured with.

## 31. Keytrack OFF
Osc 1's and osc 2's Keytrack OFF hold the oscillator at note 60: its semitone, detune and modulation still apply,
the played note does not (the instrument adds a fixed C4 in place of the note). Not in the paraphonic shapes,
where each voice has its own note. A patch can then give an oscillator its pitch back through the mod matrix:
P004 WITH DELAY holds osc 2 and feeds it the key through M-ACCU and M-DELAY, a second oscillator that follows late.

## 32. The arpeggiator in the engine
With Arp Active on, keys go to the arpeggiator (p2Arp) rather than the voice, and it plays the voice through the
same note-on and note-off a key would, a step's Glide forcing a glide. Its clock is the host's position when the
host is playing (the arp then follows the host's tempo), otherwise its own Tempo; the Global "clock" setting of the
instrument has no equivalent. The accent reads the mod sources as the last control tick left them.

## 33. Oversampling
The engine (oscillators, filter, drive) runs at 1, 2 or 4 times the host's rate, the Oversampling setting on the
Global page (default 4), and a 64-tap windowed-sinc decimator brings it back down; at 1 there is none. Changing it
rebuilds the decimator and restarts the filter and drive from rest, so it clicks once. At lower factors the
filter's top end is held below the oversampled rate's Nyquist, so the brightest cutoffs close in a little.

## 34. The noise level
Measured with the noise alone (levels 104 and 127, filter open) against the saw: the engine's white noise was 4 dB
below the instrument's in every octave band from 30 Hz to 16 kHz, with the same gentle tilt (a little more bass,
the top octave cut by the filter's top poles). Scaled by 1.585.
A floor of white noise about 100 dB down is always in the mix, as the instrument's analogue floor is, so the filter
or the FB loop can start to oscillate with every source off (a patch playing the self-oscillating filter as a sine
was silent without it).

The noise is darker than white: one pole at 8.5 kHz and a level of 1.4, fitted to the noise alone with the filter
open (within 0.5 dB in every band) and to P006's noise between osc 1's harmonics at three osc 1 levels
(`p006_masking/`, each dump read back from the edit buffer; within 0.7 dB at 1-4 kHz, 1.4 dB low at 4-12 kHz, where
the two sets pull apart). It had been one pole at 5.7 kHz and 1.585 (from the filter's EXT IN pole, removed). A loud
oscillator squashes the noise in the filter's saturation as it does on the instrument (osc 1 at 127: -4.5 dB at
250 Hz-1 kHz on the instrument, -4.0 in the engine). It is band-limited (two Butterworth sections at 22 kHz) and its amplitude scales with the square root of the
internal rate, so its density - what is heard - is the level above at any host rate and oversampling. White at the
internal rate, as it first was, it carried most of its power above the audio band: at a 48 kHz host the audible noise
came out 6 dB over the 192 kHz renders it was measured in, and the ultrasonic power drove the filter's saturation,
pulling the oscillators down 2.4 dB at P006's level 104 (3.4 dB at 127). The instrument shows neither: its triangle
holds its level and harmonics with the noise added.

A paraphonic voice is quieter than the same pulse played mono (measured, one note, PW 127): PARA-8 11.6 dB down
(0.263), PARA-4 8.8 dB (0.363). P010 had come out 10 dB loud.

## 35. Free-running oscillators
The instrument's DCOs never reset, so two of them slightly detuned (P001: osc 3 19 steps from osc 1, a beat of about
1.8 s) start a note anywhere in their beat. Each instance starts its oscillators at its own phases, as the
instrument would be. P001's "darker sustain" in the comparison was its beat caught at a different point (the
instrument's dip at 0.56 s, the plug-in's at 0.9 s), not a fault.

## 36. The oscillator level law, as the engine needs it
((Level − 11.41) / (127 − 11.41))^0.979. The instrument's own law, read off its output, is ((Level − 12.6) / 114.4)^0.864;
the engine's soft saturation at the filter input squeezes the range a little, so with that law a saw at level
16-48 came out 1-1.7 dB loud (and patches mixing two or three oscillators at middle levels 2-4 dB loud). Refitted so
the engine's OUTPUT follows the instrument's from level 16 to 127, within 0.16 dB (law_level captures).
Osc 3 in the mix is louder and less tapered than osc 1 and 2 (a saw alone, the same session: +1.5 dB at level 127,
+2.6 dB at 58 against the shared law; osc 2 matches osc 1): 1.19 x ((Level - 11.41) / 115.59)^0.709. Its control
routings (LEVEL2, VCF, DRIVE) keep the shared law: their depths were measured and fitted with it, and giving them the
louder one deepened P006's LEVEL2 modulation unmeasured.

## 37. The mix into the filter: routing gain and coupling
With osc 3 routed anywhere but MIX (LEVEL2, VCF or DRIVE), the instrument's whole oscillator mix - osc 1, osc 2 and
the noise alike - reaches the filter 1.8 times as hot, even with osc 3's level at 0: +5.1 dB for a quiet saw, falling
to +3.6 dB at level 127 as the filter's input saturates and rounds the wave (the harmonics fall 2-3 dB). The filter's
own saturation reproduces that compression within 0.3 dB and its harmonics within 1 dB, so the gain is all the
engine adds. P006 (osc 3 on LEVEL2) had come out 5 dB quiet in every oscillator.

The mix reaches the filter AC-coupled, and the filter's output is coupled again before the drive and the VCA, both at
5 Hz. Osc 3 on LEVEL2 near osc 2's pitch makes a difference tone of a fraction of a hertz, and an asymmetric wave
through the filter's saturation leaves DC; the instrument's output carries neither (60 dB less below 20 Hz than the
engine had), and the engine's pushed the filter's saturation and its measured level 10-17 dB over the instrument's
with the filter closed. Below 20 Hz is ignored in every comparison since.

## 38. Hiss
The instrument hisses while a note sounds: white across the band, gated by the amp envelope (gone at Sustain 0 once
the decay is over), following Volume exactly as the signal does, so it enters before the VCA. 73.4 dB under a saw
at the same Volume with the filter open and 79.2 dB with it shut, so part of it reaches the filter (74.7 dB under)
and part comes after it (79.2 dB). Two band-limited white sources reproduce both, within 0.2 dB and the spectrum
within 1-3 dB above 150 Hz; the instrument's mains hum below that is left out.
Off by default (the Hiss switch on the GLOBAL page), not saved with a sound. Under a closed filter it is all there
is above 4 kHz, and it is what made the instrument sound brighter on dark patches (15-18 dB more at 4-16 kHz, as
strong between a note's harmonics as on them). Comparisons render the engine with Hiss on, so it cancels.

## 39. The oscillators' polarity
The instrument's saw falls and jumps up (CT, comparing recordings; confirmed 2026-10-23 through the capture chain),
and its pulse is high for the width law's fraction of the cycle - 25% at PW 90 (findings, "pulse width",
2026-10-23; an earlier reading of 74% was the inverted one the findings warned of). The oscillators' audio is
inverted in the mix, and the DCO hands the pulse over the other way up (p2Dco notes §6) so it comes out right;
the noise, the FB loop and osc 3's control paths (LEVEL2, VCF, DRIVE - their directions measured) are not
inverted.

## 40. The filter input's slew
A saw through the open filter darkens with its level on the instrument: at level 127 its 20th harmonic is 1.9 dB and
its 60th 3.4 dB down on level 63's, more than the filter's saturation gives. A slew limit of 15000 per second (filter
input units) on the mix fits saws at levels 32, 63, 95 and 127 to 1.0 dB rms over 60 harmonics (1.2 without).
Left: the instrument's small overshoot right at each jump, and level 32's top octave 2-3 dB dark.

## 41. Link: the instrument following the sound
With Link on, the editor's frame (`p2_panel_sync`, so only while this instance's editor is open, never on the audio
thread) compares every sound parameter with what the instrument was last given. A change to one the instrument
takes by CC goes as that CC, on the Pulse 2 MIDI Channel setting - the value is the parameter's sound byte, as the
measurement rig sends it; a change to one without (the mod matrix, LFOs, bend range, arp steps), or more than 8 at
once (a new sound), sends the whole sound as one edit-buffer dump, at most every 0.1 s. Switching Link on sends the
sound. A sound from a preset (Load Preset, Prev/Next, a program change) is also sent as its bank select
(CC 32) and program change, so the instrument's display shows the same number; the dump follows 0.4 s later, once
the instrument has loaded that program, and carries any edits. A failed send switches Link off and says why. File > Send Sound to Pulse 2 sends it once. Both paths checked
on the instrument (the dump read back byte for byte; a cutoff CC read back).

## 42. Pitch goes on moving in the release
The oscillators' pitches are recomputed every control tick whether or not a key is held: the instrument keeps
modulating them through the release (CT, P004 with osc 1 down: its osc 2 follows the keyboard through
PITCH FLW -> M-ACCU -> M-DELAY (224 ms) -> OSC2 PITCH, and steps to the new note even after a short note is let go).
The engine had frozen them at the key-up, so a note shorter than the delay never got there. Before the first note
they run at the reference note, as the instrument's never stop: started only by the first note, they began
abruptly and the mix's coupling turned that into a one-sided thump decaying over ~40 ms (seen in a render). M-DELAY's 16 ms per step of amount stands: the matrix runs once per 1 ms tick,
the tick the glide law was measured on.

## 43. The mixer's levels follow the control tick; the thump (open)
The oscillator and noise levels, with their modulation, are taken every control tick (1 ms) and glided per sample;
they had been read once per audio block, so a modulated level (P004: the filter envelope pulling osc 2 down) moved
in steps of a block - 5 ms at 256 samples - and its burst at the attack came out late, long and abrupt.
The instrument's P004 (CT's recording, C3/F3 alternating, osc 1 down) also thumps: the slow part of its output rises
about +0.03 in the first 10 ms, falls through zero by 40 ms and holds near -0.05 to about 250 ms. Control-voltage
feedthrough, coupled. Tried, none matching the timing (the instrument's step is gone within ~25 ms): the osc level
controls' leak into the mix (5 Hz coupling) or after the filter (0.7-3 Hz), and the VCA's control into its output
(0.5-2 Hz). The hooks stay at zero (LEVEL_FEEDTHROUGH, VCA_FEEDTHROUGH) until a clean measurement - oscillators
and noise off, one envelope at a time - says which control leaks.
The instrument's osc 2 burst there is also shorter and earlier (peak 0.155 at ~3 ms, gone by ~7 ms) than the
engine's: likely the filter envelope's short attacks are faster than the amp envelope's table; to measure with
noise through the filter (a saw's cycles make 2 ms windows useless at low notes).

## 44. The instrument's mild distortion: per oscillator, and after the filter
A triangle through LP24 with its cutoff just above the fundamental (`postsat/`: osc levels 40-127, osc 3 on MIX and
LEVEL2, Volume 40 and 100) - an almost pure sine, so any 2nd or 3rd harmonic is distortion. Volume changes nothing,
so it all happens before the VCA.
- The 2nd harmonic (asymmetry) grows with level, -58 to -45 dB, and barely depends on the routing gain: it belongs
  to each oscillator's own level stage, x + 0.06 x^2 per oscillator, its DC coupled off at 20 Hz. On the mixed
  signal instead (the filter input) it fitted the triangle and P006 but made the oscillators intermodulate -
  P001's attack 4.5 dB dark - which the instrument does not do.
- The 3rd harmonic grows with level and with the routing gain (-46 to -34 dB). It was modelled as a stage after the
  filter, h - 4 h^3, which matched this test - and was wrong (2026-10-23): with the filter OPEN the instrument's
  triangle keeps a triangle's own harmonics at every level 30-127 (3rd -20.9 to -18.9 dB, level rising 16 dB), while
  the stage rounded it to a near-sine at 120-127 (3rd -37 to -47 dB, 3 dB of compression) - P006's osc 1, "very
  different" (CT). So the 3rd harmonic appears only near the cutoff: the filter's stages saturating on their own
  input, as an OTA ladder does, not a stage after it. The stage is removed; the near-cutoff 3rd harmonic is now
  10-25 dB short until the filter's stages saturate - tried the same day and not kept, since it pulls the
  self-oscillation flat (findings 2026-10-23); a refit of the filter's law with it is in the todo. The loop's saturation also rounds the open triangle
  (3rd -21.9 at 120 against -20.1 without), to be refitted with it.
Triangle test within ~2 dB (2nd) and ~3 dB (3rd); P006's osc 1 alone - CT heard the hardware "PWM-like" - now has
its 2nd harmonic within 2 dB (it was 50 dB short). Open: P006's 3rd harmonic (-12 dB with osc 3 on LEVEL2, +15 on
MIX), set by the filter there rather than by distortion.

## 45. Arp Active changing, by hand or by a program change
The engine notices Arp Active change at the next note or audio block and hands the voice over:

- **From OFF to ON or HOLD:** every note the keys were playing is released (the pedal's too), and the arp takes
  the keys still down - it is told of every key even while off, so it starts with them.
- **To OFF:** the arp ends what it is playing; keys still down stay silent until played again.
- **To ON:** a chord HOLD kept is dropped; only keys down play.
- **From OFF to HOLD:** with Anomalies on, a chord an earlier HOLD sound kept starts again (going from a HOLD
  sound through a non-arp sound to another HOLD sound); off, it is dropped. The plug-in's choice, not yet checked
  on the instrument.
- **HOLD to HOLD** (one HOLD sound to another): nothing changes, the kept chord goes on in the new sound.

A reset (the host stopping the plug-in) forgets the arp's keys and every note.

## 46. The Pulse 2's MIDI channel is remembered
The channel Link sends on belongs to the rig, not to a project, so it is kept in the plug-in's settings file
(p2Prefs notes §1; key `pulse2Channel`, 1-16). Changing the Channel dial saves
it; each instance takes it when its editor first opens, over whatever a project saved (Link only sends while the
editor is open, so nothing goes out on the old channel first). Before anything is saved the default is still 1.

## 47. The fault guard
Hiss switched on once broke the engine live (2026-10-21): off brought it back, on again was fine. Nothing offline
reproduces it - 3,663 renders (every library sound, 44.1/48/96 kHz, 1/2/4x, Hiss switched on and off during
notes) gave no invalid or runaway sample - and no stage can make an invalid value from a finite input. So each
output frame is checked: a value that is not finite, or more than 24 dB over full scale, is replaced with silence,
and the signal path's state (filter, drive, couplings, noise and hiss band limits, decimator history) is put back
to rest so the sound recovers by itself. The first fault per instance logs that state, to say which part went
bad; later ones only count. Enable the log with `touch /tmp/pulse2alike-log`, read `/tmp/pulse2alike.log`.

## 48. Presets
The sound remembers the preset it came from (0 for none: a file, or Init), and the panel shows it as P012 - P012*,
in white, once any byte the sound would store differs from the preset's (its address and checksum aside). Loading
a preset (by number, Prev/Next, Revert) goes through the host like loading a file (§16); a program change does the
same from the audio thread, from the cache (§20), and the panel takes the preset's dump as the base a save keeps.
Store writes the sound as it is now into a preset, through p2Patch's write-through (p2Patch notes §6).
A new instance loads P001, whichever comes first of its first audio block and its editor's first frame (a host
need not process before playback), exactly once (an atomic exchange). A restored project does not - its state
carries its own sound - and an offline render plays what it is given. The project state is `P2S2` (2026-10-23):
`P2S1` plus the preset number, so a reopened project shows its number. A `P2S1` state (a project saved before
the presets) takes its number from the sound's file name ("004 WITH DELAY.syx" is P004), or else from the slot its
stored dump is addressed to, so Prev/Next carry on from where that project was (CT, 2026-10-23).
Another process's store is noticed within a second: the editor's frame looks at the file's time once a second.

## 49. One instance owns the instrument
Everything that talks to the Pulse 2 goes through the instance holding its lock (p2Hardware notes §3; design §7.7).
Link takes it while on and gives it back when off. Link on while another instance holds it leaves Link on but
idle: the panel's button reads "Link busy" in amber, and the instance takes over - sending its whole sound - as
soon as the lock is free, without the Link setting being changed behind the host's back. A one-off Send takes the
lock only while it sends, and is refused with a message if another instance has it. Destroying an instance gives
the lock back (closing its descriptor), as does its host quitting or crashing.

## 50. Backup Pulse 2 to Presets
The instance takes the lock for the whole run (or keeps Link's), starts p2Hardware's backup thread, and polls it
from the editor's frame: progress to the panel, and when it finishes, the 500 dumps stored at once
(`p2_presets_store_all`). Link sends nothing while it runs and sends the whole sound afterwards. Cancelling stores
nothing. Destroying the instance mid-run cancels the thread and waits for it. Checked on the instrument through
vst3host (2026-10-23): 407 sounds read in under a minute, identical byte for byte to the library, 93 slots INIT.

## 51. What a mod amount weighs
Measured on the instrument (2026-10-23, findings "mod amounts"): every amount 1-63 above 64, through MOD 8 into
OSC2 PITCH, and 16 of them through MOD 5 into M-ACCU, which gave the same weights. An amount scales its source by
weight(|amount − 64|)/256, sign from the side of 64, and `kModWeight` is that weight as measured. It is close to
(d/4)², which is what the code used before, but not equal: 1/8 at d 1 doubling to 1 at d 4, runs that rise by a
constant step between, and 256 - full scale - at amount 127, where (63/4)² gives 248. Amounts 0 (d −64) and 127
weigh the same; the negative side mirrors the positive (−16 and −32 measured). Both uses keep their units: an input
to the math destinations is source × weight/256 (so full scale is 1), a target gets source × weight × 16 / 32 in
its own units (so full scale on a pitch target is 128 semitones per unit of source). A fractional amount (MOD1
AMOUNT moving slot 1's) is interpolated between entries.
At the old law P004's osc 2, which gets the key back through PITCH FLW -> M-ACCU (109) -> M-DELAY -> OSC2 PITCH
(127), tracked 0.958 semitone per key - half a semitone flat per octave; the instrument tracks exactly 1, which is
weight 128 at d 45 x 256 at d 63 x 2 semitones per key at full scale.

## 52. LEVEL2: AC coupled, linear, deep
Osc 3 routed to LEVEL2 sets osc 2's level as level2 x (1 + K x depth x c), floored at 0 and capped at LEVEL2_CEILING:
c is osc 3 inverted, AC coupled (8.0 ms) and lagged (0.3 ms); depth = ((level - LEVEL2_ZERO)/(127 - LEVEL2_ZERO))^3.42;
K 46.4 while c rises, 86.2 while it falls. Fitted 2026-10-23 to steps (osc 3 a 16 Hz square, osc 2 at 2 kHz) at
osc 3 levels 12-58, rms 0.09 (findings, "LEVEL2"). A first fit as an exponential, the same day, read the coupling's
decay as a curve and was replaced. `LEVEL2_MEASURED` 0 restores the old linear law (§25).
Open: in those 2 kHz takes the engine's filter-input slew (§40) and loop saturation compress loud osc 2 harder than
the instrument (its peak 1.73 x where the engine without them follows the fit) - an engine issue, small at P006's
low notes.

## 53. Work hoisted out of the sample loops (2026-10-24)
Two values were worked out where they could not change: the 3 ms glide's coefficient (an exp() every host sample;
`glide_coefficient()` now once a block), and osc 3's mix gain (a pow() every oversampled sample; now once a host
sample, after the control tick that is the only thing that moves it). With the drive (p2Drive notes §7), the filter
(p2Filter notes §9), the envelopes (p2Env notes §7), the LFOs (p2Lfo notes §6) and the DCO's base periods (p2Dco
notes §2), this takes 17-28% of the instructions out of the costly cases - Anomalies off, the drive on - and a few
percent elsewhere, for the MOD Dwarf. Every change keeps the expressions it replaced: 63 renders across the shapes,
routings, drive curves, filter modes, mod matrix, arp, glide, oversampling and three rates are byte-identical to
the engine before, built for x86 at -O2 and for aarch64 at the MOD toolchain's -O3 -ffast-math.
