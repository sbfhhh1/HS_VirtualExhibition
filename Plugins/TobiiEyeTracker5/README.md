# Tobii Eye Tracker 5

Windows runtime integration for `Tobii.GameIntegration.dll`.

## Blueprint API

- `UTobiiEyeTrackerSubsystem`: connection, gaze, head pose, presence, simulation, and system calibration.
- `UTobiiGazeTrailComponent`: screen-gaze raycast and green hit trail.
- `UTobiiDataRecorderComponent`: XML recording under `Saved/TobiiData`.
- `ATobiiScreenBasedDemoActor`: reusable demo controller and runtime HUD.

## Demo

Open `/Game/TobiiEyeTracker5/Examples/ScreenBasedPrefabDemo`.

- `C`: open Tobii system calibration
- `T`: toggle Eye Tracker 5 position guide
- `S`: toggle XML recording
- `F8`: toggle simulated gaze
- `Esc`: quit

Smoke test:

```powershell
UnrealEditor-Cmd.exe MCPGameProject.uproject /Game/TobiiEyeTracker5/Examples/ScreenBasedPrefabDemo -game -nullrhi -unattended -TobiiSimulate -TobiiAutoRecord -TobiiAutoQuit=5
```

Eye Tracker 5 does not support Tobii Pro Research SDK. Calibration and positioning use the installed Tobii system UI and Game Integration data; saved XML is not research-grade per-eye raw data.
