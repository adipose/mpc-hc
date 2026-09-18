# dvb-playback suite

Tunes channels on the virtual tuner and asserts on the audio that reaches the
virtual sound card.

The `dvb` suite proves the player *decodes* a broadcast correctly. Its scans
are headless and never build a playback graph, so it says nothing about
tuning a channel and rendering it. That half is where the tracker's oldest DTV
reports live -- #74 "audio is lost after changing a channel", #295, #372 --
and nothing tested it, because the rig had no audio device to render to. With
`tests\vaudio` installed it has one that records.

```
Invoke-Suite.ps1           the suite
Run-TunedCase.guest.ps1    target side: start the player in the console session,
                           give it a fixed time on air, close it
```

## How it works

Every emulator test channel carries its own sine tone
(`tests\emulator\tools\New-TestStreams.ps1`: Test Channel 1 is 440 Hz, 2 is
880, 3 is 1320, 4 is 1760, 5 is 2200). Which channel is playing is therefore
something a recording can say: tune Test Channel 3, find 1320 Hz.

From one fresh portable profile (`mpc-hc64.ini` beside the deployed exe):

1. **scan-and-store** -- `/dvbscan <range> /dvbscanout <file> /dvbscansave`.
   Asserts the scan finishes with exit code 0, finds channels, and that the
   profile now holds as many channels as the scan reported.
2. **tune-\<channel\>**, for the first, second and last channel found (two
   services of one multiplex, and another multiplex) -- the profile's
   `LastChannel` is set to the channel, the player is started with `/device`,
   left on air for `-OnAirSec` seconds and asked to close. Asserts the player
   was still running when its time was up, closed when asked, and that the
   sound card received the channel's tone for a good part of the time on air.

Live television never ends, so unlike a file case the player is closed from
outside; a player that has gone away by itself is the failure.

## The profile has three load-bearing values

- `UpdaterAutoCheck=0` -- or the first-run prompt blocks an unattended player.
- `DefaultCapture=1` -- digital. With 0 the player opens the analog capture
  options page instead of the tuner.
- `BDASymbolRate` in `[DVBConfiguration2]` -- the player reads that section
  only if this value is present in it, and otherwise falls back to the legacy
  `[DVBConfiguration]` section, silently ignoring the tuner named in the new
  one. A profile that names a tuner and no symbol rate opens no device.

## What it needs

- A player build with **`/dvbscansave`**: a headless scan that also stores
  what it found. Without it a scripted run has no channel list to tune from --
  the headless scan of #4138 writes its JSON and saves nothing, and the only
  other way to a saved list is driving the scan dialog. The suite looks for
  the switch in the binary (as UTF-16, and only after finding a switch that
  is certainly there) and counts itself skipped when it is absent.
- On the target: the virtual tuner (`tests\emulator`) and the virtual audio
  endpoint (`tests\vaudio`; `..\playback\Install-OutputDevices.ps1`).
- LAV Filters beside the player exe, as in any build output.

```powershell
.\tests\suites\dvb-playback\Invoke-Suite.ps1 -VMName <guest> -OutDir <dir> `
    -PlayerBinary <path\to\mpc-hc64.exe> [-Standard DVBC] [-OnAirSec 14]
```

## First results, 2026-09-17

On a Windows 10 guest with the DVB-C virtual tuner, against a build carrying
`/dvbscansave` (branch `dvb-scan-save`): scan-and-store found the five test
channels and stored five; Test Channels 1, 2 and 5 each rendered their own
tone. Two things the run taught, both now built into the assertions:

- **The emulator streams its previous frequency until the tune request
  lands.** A capture of a channel on a different multiplex from the one the
  tuner was last on opens with about a second of the old channel's audio,
  then switches. A real tuner delivers nothing until it locks. The suite
  judges the tone from three seconds in; the emulator is the right place to
  fix it (start delivering data only once tuned, or flush on retune).
- **The rendered pitch is a fraction of a percent off, varying run to run**
  (446 Hz for 440 in one run, 881 for 880 in another). A live source is
  rendered against a clock the player has to match, so some rate adjustment
  is expected; whether this much is expected has not been looked into. The
  suite allows 2%.

The first version of the assertion measured the first second of signal and
reported Test Channel 1 as playing Channel 5's tone. It was the stale second,
not the player. Worth remembering when a live-source result looks wrong:
look at the capture over time before believing a single number about it.

## Not yet here

Changing channel *while playing* -- the literal subject of #74 -- needs a way
to drive a running player (`/slave`, or a channel command posted to its
window); see `..\..\PLAN.md`. This suite re-tunes by restarting the player,
which covers "does this channel render" and not "does audio survive a channel
change". The picture is not asserted either; with the virtual monitor it
could be, the channels' colour pairs being declared as well.
