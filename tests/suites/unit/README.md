# unit suite

The native unit tier as an orchestrator suite. A thin wrapper over
`tests\unit\Invoke-UnitTests.ps1`: it builds `tests\unit\MpcUnitTests.vcxproj`
(which references the player's `DSUtil`, `Subtitles` and `SubPic` static
libraries so msbuild builds them as dependencies), runs the resulting console
exe, and reports its counts.

This suite runs no player and needs no rig, so its probe returns
`NeedsRig = $false`; the orchestrator then claims no test guest when the unit
suite is the only runnable one.

```powershell
.\tests\Invoke-MpcTests.ps1 -Suite unit     # through the orchestrator
.\tests\unit\Invoke-UnitTests.ps1           # directly (build + run)
.\tests\unit\Invoke-UnitTests.ps1 -NoBuild WebVTT,TextFile   # a subset
.\tests\unit\Invoke-UnitTests.ps1 -NoBuild -List
```

A test that documents a bug still present in the code under test is marked in
the source with `TEST_CASE_EXPECTED_FAILURE` and counted here as *skipped*, not
failed. When such a bug is fixed the test starts passing, the exe reports it as
an unexpected pass and returns non-zero, and the suite fails until the marker
is removed. See `tests\unit` for the tests, fixtures and the harness.
