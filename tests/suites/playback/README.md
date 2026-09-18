# playback suite

Plays generated clips in the player under test and asserts on what reached
the output devices -- not on what the player says it did.

```
New-PlaybackClips.ps1     ffmpeg-synthesised clips and clips.json, the
                          declaration of what each contains (media\, gitignored)
Invoke-Suite.ps1          the suite: deploy the player, run the cases, assert
Run-PlayerCase.guest.ps1  target side: start the player in the console session,
                          grab a frame part-way, wait for it to exit
```

## How a case works

1. A fresh portable profile: `mpc-hc64.ini` is written beside the deployed
   exe, so the player runs in ini mode and nothing carries over from the last
   case or the guest's history. `UpdaterAutoCheck=0` is always in it; without
   it an unattended player sits behind its first-run prompt.
2. The player is started from the command line in the console session
   (`<clip> /play /close`, plus whatever the case is about) and left alone.
3. Evidence is collected from outside the player:
   - **sound** -- the WAV the virtual audio endpoint wrote while the case
     ran, checked by `wavcheck.py`: tone per channel, and how long it lasted;
   - **picture** -- the frame the virtual monitor was sent, captured by
     `vdisplayctl` part-way through: the colour a quarter of the way in from
     each corner of where the fitted picture should be;
   - **process** -- exited by itself, exit code 0.

The clips are built so that content identifies itself: four flat colour
quadrants (orientation), a different sine per channel and per audio track
(which track, which channel). Nothing is compared against a stored image.

## Cases

| Case | Asserts | Tracker history |
|---|---|---|
| `plays-to-end-and-exits` | `/play /close` exits 0 by itself; 4.0 s of audio, 440 Hz left, 880 Hz right | the baseline the others stand on |
| `default-audio-track` | of two tracks, the one flagged default in the container plays | #99, #1551, #2093, #2673, #3935 |
| `fullscreen-second-monitor` | `/fullscreen /monitor 2` fills the second (virtual) monitor, right way up | #1614, #2859, #2892 |
| `rotation-metadata` | 90 degrees of display rotation is honoured, in the direction ffmpeg's autorotation renders it | #375, #3832, #3909 |

Each was checked the other way round when written: the default-track capture
is rejected against the other track's tone, the stereo capture against
swapped channels, so a pass means something.

## What it needs on the target

Two virtual devices, siblings of the tuner emulator:

- **vaudio-endpoint** -- a sound card that records what is played to it;
- **idd-vdisplay** -- a monitor that can be plugged, unplugged and read back.

Both work on a guest with no GPU and no audio hardware, and both are
submodules here (`tests\vaudio`, `tests\vdisplay`). Running the suite against
a provisioned guest needs them checked out, not built; building
(`tools\Install-Toolchain.ps1`, then `tools\Build.ps1`, in each) is only for
installing the drivers. `$env:MPC_TEST_VAUDIO` and `$env:MPC_TEST_VDISPLAY`
point the suite at other checkouts.

`Install-OutputDevices.ps1` provisions one guest: installs both drivers,
proves each works there (a plug/capture/unplug, and a two-tone played and read
back), and checkpoints the result. On a pooled rig, run it for every guest with
the pool closed to claims.

Installing a driver changes what a guest *is*, so the suite does not do it
unasked. If the devices are absent it reports that and counts itself skipped.
`-InstallDrivers` installs them first: use it on a guest you hold and will
revert, or as part of provisioning a pool.

```powershell
.\tests\suites\playback\Invoke-Suite.ps1 -VMName <guest> -OutDir <dir> `
    -PlayerBinary <path\to\mpc-hc64.exe> [-InstallDrivers]
```

Artefacts per case land in `-OutDir`: the guest's JSON, the captured WAV and
the captured PNG, so a failure can be looked at rather than re-run.

## Not yet here

Anything that needs to *drive* a running player (seek, switch track, change
rate) waits for the `/slave` host described in `..\..\PLAN.md`; these cases
use only the command line. HDR cases need a Windows 11 guest (the virtual
monitor does HDR there) and a renderer that outputs HDR.
