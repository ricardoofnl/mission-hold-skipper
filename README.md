# Mission Hold Skipper

An .asi plugin for GTA San Andreas and GTA III that turns the instant mission
scene skip into a hold to skip action. Instead of a single tap on ENTER, you hold
the key and a small circular prompt in the bottom right corner fills up. Let go
early and nothing is skipped.

There is one .asi per game, built from the same sources:
`MissionHoldSkipper.asi` for San Andreas and `MissionHoldSkipperIII.asi` for
GTA III. Vice City is next and not in this build yet.

## Screenshot

![The prompt filling up during a mission cutscene](docs/screenshot.png)

Mid hold during the Drive-By cutscene, at 1920x1080. The white arc has swept
about a third of the way around the black circle, and the label sits to its left
so it stays clear of the subtitles.

## Requirements

- GTA San Andreas v1.0 US, `gta_sa.exe` with md5 `170b3a9108687b26da2d8901c6948a18`,
  or GTA III v1.0
- An ASI loader, for example the one that ships with Silent's ASI Loader or Ultimate ASI Loader
- Optional, recommended if you play on a gamepad:
  [GInput](https://cookieplmonster.github.io/mods/gta-sa/ginput/), the SA or III
  build to match your game. The pad works without it, but the button icon inside
  the ring comes from GInput's own textures, so without GInput the prompt stays
  text only.

On San Andreas the plugin compares known byte patterns in the executable before
it patches anything. On GTA III it instead checks that the two call sites it
needs sit exactly where its address table says, since there are no byte patterns
for that build yet. Either way, a mismatch is logged and nothing is patched.

## Install

Grab the zip from the Releases page, or build it yourself as described below.

Copy the .asi for your game plus its .ini next to the game exe, or into the
`scripts` folder if your loader uses one. The two files have to sit in the same
directory, and their names have to match, because the plugin reads the ini next
to itself:

- San Andreas: `MissionHoldSkipper.asi` and `MissionHoldSkipper.ini`
- GTA III: `MissionHoldSkipperIII.asi` and `MissionHoldSkipperIII.ini`

A log is written next to the .asi with the same base name. It records the version
check, the screen size, and where the prompt was drawn, which is the first place
to look if something does not appear.

Keep exactly one copy of the .asi. Some loaders scan both the game folder and a
`scripts` folder, so the same file left in both places loads twice, both copies
fight over the same patched call sites, and the second one refuses to install and
says so in its log.

## Configuration

Every game reads its own ini next to its .asi, both under the
`[MissionHoldSkipper]` section. Values are read once at startup.

- `Enabled`: `0` leaves the game completely untouched.
- `HoldMs`: how long the key has to be held, in milliseconds. The shipped ini
  leaves it commented out, which uses the default for the game, `1200` on San
  Andreas and `600` on GTA III, where cutscenes are often only a few seconds.
- `Keys`: any of `ENTER`, `NUMPAD_ENTER`, `SPACE`, `MOUSE_LEFT`, and any of
  `PAD_CROSS`, `PAD_CIRCLE`, `PAD_SQUARE`, `PAD_TRIANGLE`, `PAD_L1`, `PAD_L2`,
  `PAD_R1`, `PAD_R2`, comma separated. On GTA III `ENTER` is the main Return key
  and `NUMPAD_ENTER` is the one on the keypad.
- `ShowHintBeforeHold`: `1` shows the prompt as soon as a scene can be skipped,
  `0` only shows it while a key is held.
- `Label`: the text next to the circle, used for keyboard and mouse. Set it to
  nothing to hide the text.
- `LabelPad`: the text used when a gamepad was the last device touched. The
  button itself is drawn as an icon, so this does not need to name it. Empty
  means always use `Label`.
- `PromptDevice`: `auto`, `keyboard` or `pad`. `auto` follows the last device
  you touched.
- `PadIconMode`: `auto` draws the button icon whenever GInput's textures are
  installed, `off` keeps the prompt text only.
- `PadIconStyle`: `auto`, `ps` or `xbox`. `auto` follows GInput's own
  `PlayStationButtons` setting.
- `PadIconScale`: icon size as a multiple of the ring's inner radius.
- `FadeInMs`, `FadeOutMs`: show and hide easing in milliseconds.
- `RingX`, `RingY`: position in 640x448 units. `-1` means the bottom right corner.
- `RingRadius`, `RingThickness`, `RingSegments`: size and smoothness of the circle.
- `LabelScaleX`, `LabelScaleY`: text size.
- `FontStyle`: `-1` keeps the game's own default, the menu font on San Andreas and
  the heading font on GTA III. Any other value is passed to `CFont::SetFontStyle`
  as is.
- `ColorBackdrop`, `ColorTrack`, `ColorProgress`: colours as `RRGGBBAA`. The
  backdrop is the black disc, progress is the white fill sweeping around the rim,
  and the track is the unfilled part of the rim, invisible by default.
- `LogLevel`: `debug`, `info`, `warn` or `error`. On `debug` the log gains one
  line per frame while a cutscene can be skipped, showing which input the plugin
  sees held and how far the hold got, which is what to attach to a bug report.

## Gamepad and GInput

Holding a pad button works the same as holding ENTER, and which button that is
comes from `Keys`. The button state is read from `CControllerState`, where every
input layer ends up, so a plain DirectInput pad and an XInput pad through
[GInput](https://cookieplmonster.github.io/mods/gta-sa/ginput/) both work without
a separate code path. The button offsets in that struct are the same in San
Andreas and GTA III, so `Keys` behaves identically in both.

With GInput installed the button is also drawn as an icon in the middle of the
ring, with the progress sweeping around it. That is the reason to install it if
you play on a pad: grab the build for your game from
[cookieplmonster.github.io](https://cookieplmonster.github.io/mods/gta-sa/ginput/),
drop it in the game folder, and the icon appears on its own with no extra
setting to change. The icon comes from GInput's own
`models/x360btns.txd` or `models/ps3btns.txd`, loaded into a private txd slot, so
nothing in the game's own assets is touched. Without those files the prompt stays
text only, and the vanilla game never shows a broken glyph.

Device switching uses GInput's public modder API when it is present, which
answers `HasPadInHands()` directly, and `GInputIII.asi`, `GInputVC.asi` and
`GInputSA.asi` are all recognised. Without GInput it falls back to watching the
pad state. If it ever guesses wrong, pin it with `PromptDevice=keyboard` or
`PromptDevice=pad`. The log records the GInput version, which device the prompt
follows, and which icon texture was loaded.

## Building

The plugin is a 32 bit Windows DLL. On Linux it cross compiles with Conan and
msvc-wine, using the toolchain file that ships with msvc-wine.

```bash
conan install . -pr:h=./conanprofile-wine.txt -pr:b=default --build=missing -of build
cmake --preset conan-release
cmake --build --preset conan-release
```

The result is `build/build/Release/MissionHoldSkipper.asi` and
`MissionHoldSkipperIII.asi`, each with its own ini copied next to it. Only
`KERNEL32.dll` is linked, since the runtime is static.

The geometry, the hold timer and the call site scanner contain no game memory
access, so they build and run natively:

```bash
cmake -S . -B build-tests -G Ninja -DMHS_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## How it works

The two games offer the skip in completely different ways, so each one gets its
own backend under `src/game/`, picked at compile time. Everything else, the hold
timer, the ring geometry, the fades, the config and the prompt, is shared.

On San Andreas every skip path funnels through
`CCutsceneMgr::IsCutsceneSkipButtonBeingPressed` at `0x4D5D10`, so that single
function is replaced. It now reports a press only on the frame the hold timer
completes, and it keeps the vanilla behaviour of skipping automatically while the
game sits in the background. A scene counts as skippable when a cutscene is
running or when any active `CRunningScript` has a non zero `m_SceneSkipIP`, which
is exactly the field `CRunningScript::Process` checks before it jumps past a
scene.

GTA III has no such function and no script side skip point at all. Its only skip
lives inline in `CCutsceneMgr::Update`, which calls `CCutsceneMgr::FinishCutscene`
the moment a skip key goes down. That call is repointed so the instant skip never
happens, and the plugin decides for itself when a skip is on offer by reading the
same four conditions the game checks: a cutscene is running, its name is not
`end`, the active camera is in flyby mode, and the cutscene load sequence has
finished. That means the prompt can appear before any key is touched, and a key
that was already held still counts. When the hold completes, the real
`FinishCutscene` runs from the logic phase, through a third repointed call, the
one to `CCutsceneMgr::Update`, because tearing a scene down in the middle of a
frame that is being drawn is what the game itself never does.

Mission scenes that keep a static camera never enter flyby mode, so vanilla
offers no skip there and neither does the plugin. The ending cutscene is excluded
by name for the same reason.

Drawing happens through the call to `CHud::Draw` inside `Render2dStuff`, which is
repointed rather than hooked at the callee, so the original function stays
reachable without a trampoline. Neither call site address is hardcoded for GTA
III, they are found by scanning the code section for the one call that targets
the function we already know, and the plugin refuses to patch when that scan
finds anything other than exactly one match. The circle is built from segmented
triangle fans, handed to `CSprite2d::Draw2DPolygon` on San Andreas and to
`CSprite2d::SetVertices` plus `RwIm2DRenderPrimitive` on GTA III, which has no
standalone `Draw2DPolygon`. Labels go through `CFont`, with the string widened to
16 bit characters for GTA III.


## License

MIT, see [LICENSE](LICENSE).
