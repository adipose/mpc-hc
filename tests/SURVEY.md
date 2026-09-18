# What the issue tracker says the tests should be

A cursory pass over every closed issue (2,677) and every pull request (932,
825 merged) in clsid2/mpc-hc as of 2026-09-17, read by title with bodies
sampled, to answer two questions: what kinds of test would have caught what
actually went wrong, and whether the command line plus the `/slave`
WM_COPYDATA API is the right surface to write them against.

Counts are keyword classifications of titles and overlap; they size the
classes, they are not a census.

## The record in one table

| Class | Issues (approx.) | Deterministic? | Right tool |
|---|---|---|---|
| Subtitles: parsing, charset, styling, positioning, fonts | 446 | parsing yes; rendering mostly | unit tests; snapshot verb for rendering |
| UI paint, theme, layout, DPI, RTL, translation fit | 369 | yes | resource lint; dialog capture |
| Seek, step, A-B, rate, repeat, autoplay, long pause | 285 | mostly | integration through the player contract |
| Crash, hang, leak, "timeout closing graph" | 260 | partly | lifecycle soak; unit tests for parser crashes |
| Driver, GPU, HDR, decoder, madVR, Discord, antivirus | 260 | display side yes, with a virtual monitor; vendor decode no | virtual display driver; rest out of scope |
| Audio tracks, renderer, switcher, volume | 236 | yes, with a virtual endpoint | integration; virtual audio device |
| Window, fullscreen, zoom, multi-monitor | 221 | single-monitor yes | integration, needs a state dump |
| History, file position, favorites, settings persistence | 202 | yes | integration across restarts; unit for INI |
| Playlist, M3U/PLS/CUE, next/previous file | 191 | yes | unit tests once parsing is reachable; integration for navigation |
| Streaming, yt-dlp, URLs, HLS | 186 | only with a stub | stub `yt-dlp.exe`, local HTTP server |
| Command line, API, web interface, hotkeys, SMTC | 108 | yes | integration -- this *is* the contract |
| Seek preview, thumbnails, save image | 98 | yes | snapshot verbs, structural image checks |
| Build, install, packaging | 69 | -- | CI, not this framework |
| DVD, Blu-ray, RAR | 49 | parsers yes | unit tests on IFO/MPLS/RAR fixtures |
| DVB, capture | 28 | yes (emulator) | the existing `dvb` suite |

About 450 closed issues are feature requests or questions, 418 were closed
as not planned, and 359 are phrased as regressions ("no longer", "since
1.9.x", "again"). The regression-phrased ones are the target, and they
cluster hard: the same behaviours break repeatedly.

## Behaviours that broke more than once

These are the strongest argument for the framework, because each was fixed
and then came back:

- **Remember file position**: #1595, #1805, #2287, #2659, #3182, #3352,
  #3847; with A-B marks #1578, #1906, #1948, #2344, #3863.
- **`.pls` playlists**: #347, #400, #1889 ("not working... again").
- **Screenshots with subtitles**: #487, #651, #746, #1514, #1720, #2682,
  #3278, #3381.
- **Video rotation**: #375, #3832, then #3909 "[Regression bug 3832]".
- **Launch in fullscreen / `/fullscreen` / `/play`**: #546, #1127, #1423
  ("1.9.17 breaks /play"), #1844, #2859, #2891.
- **Default/remembered audio and subtitle track**: #99, #1551, #2093,
  #2452, #2673, #2876, #3283, #3914.
- **Repeat / loop at end of file**: #1691, #1850, #2488, #3324, #3738.
- **Next file in folder**: #414, #697, #1419, #2200, #2209, #2579.
- **After Playback resets**: #342, #1650, #2494.
- **MediaInfo tab missing on first open**: #2242, #2322, #2336, #2341, #2348
  (five reports of one bug).
- **External audio/subtitle autoload**: #1121, #1164, #1894, #3152.
- **Translated text clipped**: #2697, #2816, #4076, #4107, #4141, #4159.

None of these needs a GPU, a network or a human eye to detect.

## Four kinds of test, by what they need

### 1. Unit tests: bytes in, struct out

The recent hardening run (#4174-#4183 filed, #4184-#4208 fixed) is the
template: a parser trusts a length, a palette index, an offset. So are
years of text-format fixes: WebVTT (#668, #673, #677, #992, #1054, #1806,
#4197), SRT tags (#4185), ASS syntax (#1038), codepages (#419, #2299,
#2548, #3413), language-tag mapping (#632, #927, #3321, #3452), M3U
relative paths and wildcards (#318, #724, #1383, #1557), URL and UTF-8
decoding (#334, #470, #1376), long paths (#1717, #1464, #3766, #3992),
filename sanitising (#2612, #2525), menu-text escaping (#4130), natural
sort (#973, #2025, #3995), transport_stream_id byte order (#4139), INI
round-trip (#2412, #4193).

Shape: a fixture file or literal, one function call, assertions on the
result. Milliseconds each, no player process, no VM, runnable in CI.

The command line and WM_COPYDATA are the **wrong** surface for these. A
malformed PGS palette can be fed to a running player, but the only
observable is "did it crash", the round trip is seconds, and it needs a
desktop session. A unit test sees the parsed structure.

What it needs: a native test executable. `src/Subtitles` and `src/DSUtil`
are already static libraries (MFC static), so STS/WebVTT/PGS/VobSub/DVBSub
and the path/text/language helpers link as they are. The obstacle is that
a lot of pure logic lives inside MFC window classes: playlist parsing is
`CPlayerPlaylistBar::ParseM3UPlayList`/`ParseCUESheet`/`ParseMPCPlayList`,
and `MainFrm.cpp` is 24,870 lines. Those need a small extraction (free
functions taking a stream, returning a list) before they are reachable --
a refactor with its own upstream-acceptance question, so start with what
links today.

### 2. Resource lint: static checks on `.rc` and `.po`

The second-largest class is UI, and most of it is not worth automating
through a running player. But a recurring slice is statically checkable
with no player at all: translated strings wider than their controls,
duplicate resource IDs (#344), duplicate accelerators, tab
order (#4104), strings missing from the translation set (#4158, #4167),
command-line help alignment (#1294, #3741, #3862). A script that measures
each translation against its dialog template would have pre-empted most of
#4076-#4146.

### 3. Integration through the player contract

This is where the repeat offenders live, and here the existing surface
**is** appropriate. Every item in the list above reduces to: start with a
known profile, open known media, act, observe, sometimes restart and
observe again. The pieces exist:

- *Arrange*: command-line switches (there are 62), plus a seeded portable
  `mpc-hc64.ini` for settings that have no switch.
- *Act*: `/slave` commands (40 inbound), and for the other ~300
  actions, `WM_COMMAND` with the `ID_*` value posted to the main HWND that
  `CMD_CONNECT` hands over -- no player change needed.
- *Observe*: `/slave` notifications and getters; the INI and media-history
  files after exit; debug logs; output files from verbs.

Its limits, in order of how much they hurt:

1. **No single state query.** State arrives as separate notifications.
   MPCTestAPI's `CPlayerStateSnapshot` works around this by firing every
   getter and collecting replies for a fixed 750 ms window -- a timing
   guess where a test needs a fact. One `CMD_GETSTATE` returning JSON
   (load state, position, duration, rate, repeat, A-B, fullscreen, zoom,
   window rect, selected audio/subtitle track, playlist index, loaded
   subtitle sources, filter list) removes the guess and makes fullscreen,
   zoom and track-selection bugs observable at all. This is item 5 in
   PLAN.md and the survey moves it to the front.
2. **No request/response correlation.** Replies are untagged
   WM_COPYDATA messages; the harness must serialise. Acceptable.
3. **Same-desktop only.** WM_COPYDATA needs the harness in the player's
   session, which the guest-side scheduled task already provides.
4. **No settings access.** Seeding the INI covers it; nothing to add.

The web interface overlaps (it has `wm_command` and `status.json`) and
works across machines, but it is a feature under test with its own bug
history (#704, #718, #3977, #4093, #4135, #4176), so it stays a subject,
not a dependency.

What it needs from the framework: a `/slave` host in the guest harness, a
**profile sandbox** (fresh portable directory with a seeded INI per test,
kept across the restarts a persistence test makes), and a **media
generator** (ffmpeg clips with known duration, tracks, languages, default
flags, chapters, rotation, embedded and sidecar subtitles) -- the same
declared-expectation approach the emulator's encoding matrix takes.

### 4. Rendered output, structurally

Subtitle placement and aspect (#134, #1476, #1608, #1616, #2161, #4040),
screenshots with subtitles, rotation, thumbnail aspect (#3127, #653). The
player already has verbs that write images (`/thumbnails`, save-image
commands); with clips that carry known markers, assertions are geometric
("the subtitle box lies inside the video rect", "the red corner is
top-left") rather than golden pixels. Renderer availability in a VM
without a GPU is the open risk; EVR-CP on the basic display adapter needs
proving before this tier is planned in detail.

## Virtual devices at the other OS boundaries

The `dvb` suite works because the emulator sits at an OS boundary (the BDA
driver model), feeds the player declared content and makes the result
assertable. The same pattern applies on the output side, and it moves a
large part of what would otherwise be "needs real hardware" into reach:

- **Display: an indirect display driver (IddCx).** A virtual monitor whose
  EDID the test chooses, and whose driver *receives the frames the desktop
  compositor presents to it*. That covers, by title count, about 60
  HDR/colour/levels issues and about 40 multi-monitor, hot-unplug, per-
  monitor DPI and mode-switch issues: HDR switched on at open and not off
  at close (#3437, #3550, #3564), washed-out fullscreen (#3790, #3300),
  autochange fullscreen mode against a chosen mode list (#2153, #2748,
  #1444), crashes on monitor removal (#538, #2083, #3634), moving between
  monitors of different DPI (#1115, #1871, #3661), fullscreen on the
  second monitor (#1614, #2892). It is also the honest observation point
  for tier 4: assert on what the monitor was sent, not on a screenshot
  API's idea of it. IddCx 1.10 (Windows 11 22H2) added HDR10 and
  wide-gamut surfaces with metadata; Microsoft publishes an IDD sample,
  and open-source virtual display drivers with HDR exist to start from.
- **Audio: a virtual render endpoint.** Microsoft's SYSVAD sample is the
  audio counterpart of the WDK tuner sample. An endpoint whose supported
  formats the test declares and whose received samples it captures reaches
  about 95 issues: exclusive-mode and sample-rate negotiation (#423, #702),
  bitstream format acceptance (#1643, #2131), downmix levels (#105, #1343,
  #1455), device selection and removal (#637, #2237, #2417), pitch under
  rate change, and -- with a distinct tone per track in the generated
  clip -- a direct check of which audio track is actually playing and
  whether it is in sync.

Both were tried on 2026-09-17, on a pool guest (Windows 10 22H2, Hyper-V
video adapter, no GPU), which answers the first unknown:

- **Display** (`idd-vdisplay`): an indirect display presents on the Basic
  Render Driver alone. A 1920x1080 monitor and a second at 3840x2160 were
  plugged, captured pixel-exact, and cycled; Windows offered exactly the
  declared modes, and `ChangeDisplaySettingsEx` to 23 Hz committed
  24000/1001. No GPU partitioning needed.
- **Audio** (`vaudio-endpoint`): the guests had no audio endpoint at all.
  With the driver they have Speakers; a clip with 1000 Hz left and 2500 Hz
  right came back from the driver as 4.000 s of exactly those tones on
  those channels.

HDR was then tried on a Windows 11 23H2 guest, also without a GPU: with the
driver offering 10 bits per component, Windows reports the monitor as
advanced-colour capable, the HDR toggle succeeds, and the driver receives
FP16 scRGB frames with the link committed as HDR10 at 10 bits. A captured
frame of the SDR desktop peaks at exactly the SDR white level Windows reports
(240 nits = 3.00 scRGB), so "did the player pass HDR through or tone-map it"
becomes a comparison against that level.

Still open: multichannel, exclusive-mode and bitstream audio are not yet
exercised; and much of the HDR logic lives in MPC Video Renderer, a separate
project, so those tests pin the *pairing* of player and renderer rather than
player code alone. Both drivers derive from Microsoft samples under MS-PL,
not MIT.

## Still out of scope

Vendor-specific behaviour stays out: DXVA decode bugs on a particular
GPU, RTX Video HDR/VSR, madVR internals, Discord's overlay, antivirus
false positives. So does third-party service drift (OpenSubtitles, yt-dlp
site support), though the integration *around* those is testable with
stubs -- a fake `yt-dlp.exe` that prints canned JSON covers format
selection and argument passing (#527, #1522, #3040, #3476) the way the
cast mock covers Cast. A later suite, not a foundation.

Lifecycle crashes (close during open #2761, exit during a prompt #3989,
close with a modal up #4209, rapid opens #553 #3617, GDI leaks #874 #2156
#4194) fit a soak suite on tier 3: loop, watch exit codes, timeouts and
`GetGuiResources`. Cheap once the host exists.

## Where the fixes landed

Changed files of all 825 merged PRs, grouped by area (a PR counts once
per area it touches):

| Area | Merged PRs touching it |
|---|---|
| `MainFrm.cpp`/`.h` | 251 |
| `mpc-hc.rc`, `resource.h` | 149 |
| Theme widgets (`CMPCTheme*`) | 141 |
| Options pages (`PPage*`) | 130 |
| Settings and app (`AppSettings`, `mplayerc`) | 113 |
| Translations | 102 |
| `src/Subtitles` | 75 |
| Playlist (`PlayerPlaylistBar`, list controls) | 70 |
| `src/DSUtil` | 48 |
| Subtitle UI and download providers | 35 |
| Video renderers | 24 |
| `src/SubPic` | 17 |
| Web interface and `MpcApi` | 12 |
| DVB | 9 |

`MainFrm.cpp` alone is in 247 merged PRs, nearly one in three. That is the
player's state machine, and it is reachable only through a running player:
the churn sits exactly where tier 3 aims and where a unit test cannot go
without refactoring. `Subtitles` plus `DSUtil` account for about 120 PRs
and link into a test executable today. Resources, options pages and
translations together are the lint tier's territory. DVB, the one area
with a suite so far, is the smallest row in the table.

## What this changes in the plan

1. `tests/unit/` now exists: a native test executable over `Subtitles`,
   `SubPic` and `DSUtil` with a fixture corpus, run by
   `tests/unit/Invoke-UnitTests.ps1` or as the `unit` suite. Highest yield
   per hour, no rig, and the natural home for a test accompanying every
   parser fix. Its first pass already documents five open bugs as expected
   failures. Playlist, MPLS and INI logic still need extracting from MFC
   classes before it can reach them.
2. `tests/lint/` for resources and translations is nearly free and aims at
   the most frequently reported cosmetic class.
3. For integration, keep the command line and `/slave`; do not invent a
   new IPC. Add exactly one player change early -- `CMD_GETSTATE` -- and
   build the profile sandbox and media generator on the framework side.
   First suites, by recurrence: persistence (position, A-B, tracks, After
   Playback), command-line switches, playlist navigation and end-of-file
   behaviour, track selection.
4. Virtual display and virtual audio devices are the emulator idea applied
   to the output boundaries, and both work on the existing GPU-less guests.
   Tier 4 observes frames through the display driver's capture rather than
   a screenshot API; audio-track and sync assertions read the audio
   driver's WAV. HDR works the same way on a Windows 11 guest, which now
   has its own broker pool (`rig-claim -Pool win11`). Next: provision the
   drivers across both pools and write the first suites against them.
5. `dvb` and `chromecast` stay as they are: specialised suites with their
   own mocks, siblings of the general ones rather than the model for them.
