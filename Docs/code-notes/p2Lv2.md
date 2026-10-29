# lv2/ - notes

The LV2 plug-in, for MOD devices (the Dwarf first) and any Linux LV2 host: `p2Lv2.c` (the plug-in), `p2Lv2Ports.c`
(its ports), `p2Lv2Ttl.c` (its Turtle), `p2Lv2Gui.c` (its panel on a MOD device), `p2Lv2Presets.c` (sounds into
presets), `Makefile`, `test/`. Started 2026-10-24. The binary is headless: none of the desktop panel, SynthLib's
renderer or FreeType is built - the engine is compiled with `P2_NO_EDITOR`, as `tools/p2render` already did. The
panel a MOD device shows is a web page in the bundle (§16), drawn by MOD's web interface, not by the plug-in.

It is written against SynthLib's format-free description (`synthlibPlugin.h`), as the VST3 and Audio Unit wrappers
are, and never reaches into the engine: `p2Plugin.c` is the same file the desktop plug-ins build.

## 1. Identity
`P2_LV2_URI` names the plug-in and every port is found by its `lv2:symbol`: a MOD pedalboard and an LV2 preset save
symbols, not indices. Neither may change once a bundle is out - like the VST3 class ids. The URI is under the
project's own address, settled on 2026-10-24.

`lv2:minorVersion` / `lv2:microVersion` are what a MOD device compares to take a bundle as newer. They are the LV2
plug-in's own (`P2_LV2_MINOR_VERSION`, `P2_LV2_MICRO_VERSION`), not derived from the release version: a scheme that
let them go down at 1.0 would make MOD keep the old bundle.

They also set what MOD calls the plug-in's stability (mod-ui's `utils_lilv.cpp`): minor 0 is "experimental", an odd
minor or an odd micro "testing", both even "stable". 1.0 lists as testing, which suits it until it has been played
on a device; the first bundle meant as stable is minor 2, micro 0.

## 2. A port's value is what the panel shows
A port holds `offset + scale x plain`, `plain` being the engine's own value (`gP2Params`' plainMin..plainMax) - which
is exactly the panel's text for it: the bipolar dials read -64..+63, the semitones -48..+48, Arp Tempo in BPM
(46 + 2 per step), Swing 0..100 % and Delay -50..+50 %, Range and Length from 1. The lists are `lv2:enumeration`s
of their entries and OFF/ON is `lv2:toggled` (a switch, which a Dwarf footswitch can take), with OFF and ON as its
scale points too, so what MOD prints for it reads as the panel's. The generator checks every
step of every port against the panel's text (§5), so the two cannot drift apart.

A host may send anything: `p2_lv2_normalized()` rounds a stepped port to its nearest step and clamps every port to
its range (a non-number to the minimum), so the engine never sees a value it could not hold. Cutoff and Resonance
are continuous, as they are in the engine: a MOD knob or expression pedal sweeps them without steps.

Only a port the host moved is given to the engine (`take_controls()`), once per `run()`: `setParam()` works out the
oscillators again, which is not free.

## 3. Defaults, and the sound at the start
Every default is the engine's except Oversampling, which is 2x here, not the desktop's 4x (`defaultStep`). On a
Dwarf the engine at 4x costs 55-75% more than at 2x (§13), and one core runs the whole pedalboard; 4x is still there
for a patch that needs it. A desktop LV2 host gets the same 2x - one bundle, one Turtle.

A new engine instance loads P001 from the desktop plug-in's presets file on its first block (p2Plugin notes §48).
In LV2 the sound is the ports', so `instantiate()` hands the instance its own fresh state back (`getState()` then
`setState()`), which is the engine's own way of being told "this sound came from the host": it clears the P001 load
and changes nothing else. Without it the first block would quietly replace every port's value with INIT's.

## 4. Which parameters are ports
All of them, in parameter id order, except: Program and Bank, and the performance controls (Pitch Bend, Mod Wheel,
Pressure, Breath, Sustain) - the `HIDDEN | NO_SAVE` ones, which arrive as MIDI (§8) - and Link and the Pulse 2's
channel, which drive an instrument over CoreMIDI, a desktop thing. 110 ports.

Id order because ids only ever grow at the end (p2Params notes §1): a new parameter becomes a new port at the end,
and no index moves. The panel's order comes back through the groups (§6). The arpeggiator's 16 step bytes are
ports - so a preset or a pedalboard keeps a sound's pattern - but `pprops:notOnGUI`, as they are hidden on the
desktop panel until there is a step editor.

The generator fails on a parameter that is neither a port nor one of the above, so a parameter added to
`tools/gen_params.py` cannot silently go missing here: it needs a line in `gP2Lv2Ports`, or a reason not to.

## 5. The Turtle is generated, and committed
`p2Lv2Ttl` links the engine's tables and writes `manifest.ttl` and `Pulse2Alike.ttl` from them: names, ranges,
defaults, scale points, units, groups - and `modgui.ttl` and the panel's template (§16). `make -C lv2 ttl` after any parameter change; `make -C lv2 check` fails while
the committed files differ from what it would write. Committed, so a cross build (mod-plugin-builder, `make dwarf`)
needs no program run on the build machine - only the generator needs a host compiler.

Before writing anything it checks: each port against the panel at every step (§2); symbols valid and unique; short
names unique and at most 16 characters, which is what MOD keeps of `lv2:shortName` - the Dwarf's display shows them,
so they say "Osc1 Shape", not the panel's "Shape" three times over; `mod:brand` at most 11 (MOD's schema) and
`mod:label` 16; every conversion surviving a round trip at every step.

## 6. Groups
One `pg:InputGroup` per panel section (OSC 1 ... PLUG-IN), with `lv2:index` in the panel's order - mod-ui sorts the
settings dialog by a group's index and then by port index, and colours each group - and "ARP STEPS" for the hidden
step bytes. This is MOD's own way of grouping (its port-groups patch to TAL-NoiseMaker).

## 7. The host's transport
`time:Position` arrives on the event port: mod-host sends one every block while the transport rolls and one when it
stops, with bar, beat and tempo when JACK has them. The engine wants quarter notes from the start and a tempo in
quarter notes, so `quarters = (bar x beatsPerBar + barBeat) x 4 / beatUnit`, and the BPM likewise. Between
positions the wrapper carries the position on by the frames rendered, as a host would. Until the host has said
anything, the transport is not valid and the arpeggiator keeps its own clock (Arp Tempo); with a tempo and a
position while rolling it follows the host (p2Plugin notes §32).

## 8. MIDI
Notes, with a note-on at velocity 0 a note-off; channel pressure, pitch bend (14 bits over 16383, as the Audio Unit
wrapper does it) and the controllers the parameter table maps (`midiControl`) onto the parameters that are not
ports - the mod wheel, breath, the sustain pedal, bank select. ALL SOUND OFF and ALL NOTES OFF reset the voice, as
in the Audio Unit wrapper. On every channel, as the desktop plug-ins.

The instrument's CCs too (Docs/design.md §4 and midi.guide's table for the Pulse 2: every sound parameter's,
`gP2Lv2Ccs` in `p2Lv2Ports.c`; CC 7 and 10 as the sound's volume and pan), though the parameters they move are
ports. A CC carries the parameter's sound byte,
as the instrument takes it and a dump holds it (`p2_lv2_byte_value`: from the parameter's lowest byte, one byte a
step, clamped to its range), so a controller or sequence made for the instrument plays this the same way - CC 34 = 3
is APW, not 3/127 of the way through the shapes. The CC moves the parameter at once, on its own frame, and asks the
host to move the port to match through KXStudio's control input port change request, which mod-host offers: a MOD
device's web interface and saved pedalboard then follow, the lists too - MOD's MIDI learn reaches no list of more
than two entries (`lv2:enumeration`), and this does. The device's display does not follow at once: mod-ui passes a
port the plug-in moved to the web page, not to the display, which shows the new value the next time it draws the
control, and a knob on it carries on from the value it showed. Until the port moves, its value from before the CC is not
given to the engine again (`held`), so a host that moves it later, or never (one without the feature), does not undo
the CC; the next value the host gives the port is taken as any port move. A port MIDI-learnt on the same CC by the
host moves twice, the host's way and this one.

Not taken: program changes, since there is no presets file on a device and the ports would not follow a sound
loaded behind the host's back - MOD switches sounds with presets and snapshots, and can take program changes for
those itself; controllers the instrument has not. A message the engine takes nothing from does not split the block
(§12).

## 9. Flush to zero
`run()` sets flush-to-zero (and denormals-are-zero on x86) for its length and puts the host's mode back after
(`p2Lv2Fp.h`). The engine does make subnormal numbers - a released note's VCA decaying through the decimator - and
on x86 they are slow. What it changes was measured: against the engine running without it, only output samples
too small for a float to hold in full differ (below 1.2e-38); the test checks that on every case (§14).

## 10. What the engine asks of a wrapper
Only `synthlib_plugin_param_edited()`, from its program change, which LV2 never delivers; there is no host to tell
anyway, the ports being the host's. The wrapper's is empty.

## 11. Block size
LV2 promises nothing about it without the options feature, and the engine asks nothing of it, so `prepare()` is
told 0.

## 12. Events land on their own frame
The engine cannot place an event inside a block - it ignores the offset (synthlibPlugin.h notes §13) - so `run()`
splits the block at every event that reaches the engine: the frames before it are rendered, then the event is
given, then the rest. A note therefore starts on its own frame, not at the block's start as in the desktop
wrappers, and the arpeggiator gets the transport at each part's first frame.

## 13. Building for a MOD Dwarf
The Dwarf is an aarch64 Cortex-A35 with **glibc 2.27** (mod-plugin-builder's `toolchain/moddwarf*.config`). A
plug-in linked against a newer glibc asks for symbols the Dwarf does not have - `exp`, `log` and `pow` at GLIBC_2.29,
`stat` at 2.33, `pthread_once` at 2.34 (where 2.27 keeps it in libpthread) - and fails to load. Two ways to build
one that loads:

- **mod-plugin-builder**, MOD's own toolchain: `lv2/mod-plugin-builder/pulse2alike/` is a package for it, building a
  local checkout (by default /root/source, where the MOD wiki's Docker setup mounts one). Its flags come from
  `TARGET_CONFIGURE_OPTS`; warnings are not errors there, a different compiler being free to warn differently.
- **`make -C lv2 dwarf`**, with a stock cross compiler (`gcc-aarch64-linux-gnu` on Debian or Ubuntu):
  `do-glibc-sysroot` fetches Ubuntu 18.04's arm64 glibc - 2.27, the Dwarf's - with pinned SHA-256s, and the build
  links against that, with mod-plugin-builder's Dwarf flags (`-mcpu=cortex-a35 ... -ffast-math`). A Debian or
  Ubuntu cross compiler searches its own C library ahead of any `--sysroot`, so every path is given explicitly
  (`-nostdinc`, the compiler's own headers, the sysroot's; `-B` and `-L` for its start files and libraries).

Either way `check-binary` reads the result with readelf, which works on any architecture: no glibc version above
2.27, no library a device may not have, nothing exported but `lv2_descriptor` (hidden visibility everywhere else, so
two SynthLib-based plug-ins in one mod-host cannot bind to each other's functions). It refused the first `make dwarf`,
whose sysroot had silently not been used. `make -C lv2 publish` sends the bundle to a device on USB, as
mod-plugin-builder's `publish` does.

**CPU** (2026-10-24, estimated: aarch64 instructions counted under qemu, through glibc 2.27's own libm, at an
assumed 0.7-1.0 instructions a cycle on a 1.3 GHz A35 - not measured on a device), a three-note chord, as a share of
one core - mod-host runs the whole pedalboard on one:

| Patch | 1x | 2x | 4x |
|---|---|---|---|
| saw (the defaults) | 3-4% | 6-9% | 10-14% |
| unison | | 9-13% | 16-22% |
| PARA-8 | | 8-11% | 13-18% |
| three oscillators, noise, hiss, FUZZ | 8-11% | 15-22% | 27-38% |
| saw, Anomalies off | | 15-21% | |
| unison, Anomalies off | | 22-31% | |

On the same counts the engine's bit-exact optimisations (p2Plugin notes §53) took 8-39% of the instructions out,
most where it costs most: the drive, and Anomalies off. glibc 2.27's `exp`, `log` and `pow` predate the fast ones
of 2.28, so on a Dwarf they weigh more than any x86 profile shows.

## 14. The checks
`make -C lv2 check`: the committed Turtle and panel template are the generator's (§5); the Turtle is valid against the
LV2 specifications, MOD's vocabularies (`lv2/test/mod.lv2` and `modgui.lv2`, from mod-lv2-extensions) and the port
change request's (`lv2/test/kx-control-input-port-change-request.lv2`), all for checking only; `check-binary` (with no glibc
ceiling, for a build for this machine); and `test/p2Lv2Test`.

`p2Lv2Test` loads the built `.so` as a host does - URID map, 128-frame blocks as on a MOD device, MIDI and
`time:Position` in atom sequences, ports moving between blocks and past their ends - and drives the engine, linked
into the test from the plug-in's own object files, directly through its descriptor as an ideal host would. Sixteen
cases (the oscillator modes, osc 3's routings, the drive, filter modes, oversampling, Anomalies, performance
controls, ignored messages, the instrument's CCs, port moves, the arpeggiator on its own clock and following a host):
the two must agree to the bit, both channels must be the same, and each must sound (or be silent, for FX). Each case
runs in pairs so the two engines number their instances alike, each instance's oscillators starting where that count
puts them (p2Plugin notes §35). Each also runs with a host offering the port change request (§8), moving the port at
once as mod-host does and three blocks later: both must sound as the host without it, and each instrument CC must
ask for its port, with its value. The CC table is checked too - each CC once, none of the performance controls',
each onto a port - and a few CCs' values against Docs/design.md §4. Mutating the wrapper - no block split, the bend
off by one, velocity 0 as a note-on, CC 123 ignored, the tempo not per quarter, the transport not carried on, ports
applied once, aftertouch to the wheel, the P001 load left in, a CC's port value taken back, a request for the wrong
port - fails the test each time.

`make -C lv2 dwarf-check` builds the same test for aarch64 against the Dwarf's C library and runs it under
qemu-aarch64 on that library: the Dwarf's binary, its compiler flags and its glibc. The GitHub workflow
(`.github/workflows/lv2.yml`) runs both and keeps the Dwarf's bundle as an artifact.

## 15. Sounds into presets
On a desktop a program change recalls one of 500 sounds from the presets file (p2Plugin notes §48); on a MOD device
sounds are LV2 presets, which it lists, saves with a pedalboard and can put on a footswitch or knob. `p2Lv2Presets`
turns what Import Presets reads - a .syx of one or many dumps, or a folder of `NNN Name.syx` - into presets in the
bundle, with the engine's own readers (`p2_presets_read_file/folder`). `make -C lv2 dwarf PRESETS=<sounds>`.

A preset is PNNN, the slot its dump is addressed to (edit-buffer dumps in turn from 1), and its URI is the slot's,
so a pedalboard that chose P012 still finds P012 after the sounds are changed. It sets the parameters a dump
carries and no others: loading a sound leaves Oversampling, Anomalies, Hiss and Master Volume as the player has
them. A slot left INIT is not a preset. A bundle built without PRESETS after one built with them loses them.

## 16. The panel on a MOD device
A MOD device's web interface draws a plug-in from its modgui: an HTML template, a stylesheet and a script in the
bundle (`modgui.ttl`, `modgui/`). Without one it draws a plain box with a few knobs and the rest only in the
settings dialog - 110 controls in a list. With one, the pedalboard shows the instrument's own way of reaching them:
its matrix. On the Dwarf itself there is no panel; it is what a browser on the device's address shows, and what
makes a sound there, live, with the Dwarf's knobs for what is played (README).

**The drawing is original**, in the manner of the instrument's panel - black, its lime green, the matrix of six rows
over six knobs, the LCD framed in green and the line from it round the arrows - with none of Waldorf's: no name,
logo, wordmark or photograph. The name on it is the project's.

**Pages.** `kPages` (`p2Lv2Gui.c`) is the instrument's matrix row by row - OSC 1/2/3, LFO 1 - LFO 2 - GLIDE,
ENV F/A, ARP, VCF, VCA - and then what it reaches with Mod (MOD 1-2 ... 7-8) and Global (VOICE, GLOBAL); ARP TIMING
(Step, Swing, Delay, Length, Accent) shares the ARP row as the instrument's arp settings share its button. Each page
is six knobs. A row's button steps through its pages and an LED shows which, as on the instrument; the Mod and
Global labels, where the instrument prints its shift functions, step through theirs; the two arrows step through
every page. The LCD heads the page and names each knob with its value as MOD formats it - a list's entry, a unit -
and a value there can be typed. Master Volume has its own knob, where the instrument has its data knob. A page the
matrix does not print dims the matrix, so its labels are not read for the LCD's.

**Every control is on it once.** `p2_lv2_gui_check()` fails the generator unless every port the plug-in shows is on
exactly one page (the arp step bytes are not shown, §4), so a parameter added to the ports cannot be missing from
the panel. The template is generated from the page table for the same reason the Turtle is (§5) - 110 bindings by
hand would drift - and `check-ttl` compares it; the stylesheet and the script are written by hand.

**How it works.** Every page's knobs are in the template at once, each bound to its port by MOD
(`mod-role="input-control-port"`), in the same six places; the script shows one page's and hides the rest
(`visibility`, so a hidden knob cannot be clicked either). MOD keeps every one of them current - a change from a
Dwarf knob, MIDI or a preset lands on its page whether it is shown or not. A knob is a still steel cap and a pointer
MOD turns (`mod-widget-rotation`), so the light on the steel does not turn with it; that attribute is mod-ui's from
May 2022 (MOD OS 1.12 on). On an older one the pointers stay still and the LCD values still follow. The page shown
is the panel's own, not saved: a pedalboard opens on VCF.

Under MOD's drag handle (`z-index` 20) is the face and what is only printed on it; above it, what is pressed or
turned, which stops a press reaching the pedalboard so a click is not a drag. Every size is a border box, whatever
the page around says. The script is plain ES5 with the jQuery mod-ui already has: it runs in whatever browser opens
the device.

**The pictures.** `screenshot-pulse2alike.png` (the panel at its size, 760 x 300) and `thumbnail-pulse2alike.png`
(at most 256 x 64, as MOD asks) are the panel as mod-ui draws it, on its VCF page at the defaults: mod-ui run in
development mode (`MOD_DEV_HOST=1`, `LV2_PATH` on `build/lv2`), the plug-in added, the panel taken at zoom 1 with
Playwright and its corners cut round. After changing the look, take them again.
