# RammsUI

Unreal Engine plugin providing UI widgets, camera visualization, and robot control interfaces for the RAMMS platform.

## Plugin Dependencies

| Plugin | Required | Purpose |
|---|---|---|
| **RammsStreaming** | Yes | Bridges external RMSS streams into the camera pipeline |
| **CameraCapture** | No | Optional capture replay support |

## Port Assignments

| Service | Port |
|---|---|
| UE Remote Control HTTP | 30010 |
| UE Remote Control WebSocket | 30020 |
| RMSS Binary Streaming | 30030 |

## Features

### Camera System

The camera pipeline follows a provider→widget→display architecture:

```
Internal:   URammsCameraProviderBase ──► URammsCameraWidget ──► Display Mode
External:   RMSS Stream ──► URammsStreamCameraBridge ──► IRammsCameraProvider ──► URammsCameraWidget
```

| Class | Description |
|---|---|
| `IRammsCameraProvider` | Interface for delivering camera frame data via `OnCameraFrameReady` |
| `URammsCameraProviderBase` | Actor-based provider with socket and transform support |
| `URammsCameraProviderComponent` | Actor component implementing `IRammsCameraProvider` |
| `URammsCameraWidget` | Displays camera feeds with multi-provider support. Supports stream selection, depth, overlay, and side-by-side display modes |
| `URammsStreamCameraBridge` | Bridges RMSS streaming server to the camera widget pipeline. Auto-registers streams with intrinsics/extrinsics metadata |
| `URammsCameraProjectionManager` | Manages camera projection decals for world-space projection of camera imagery |
| `URammsCameraProjectorComponent` | Scene component for a single camera stream projector |

### Robot Control

| Class | Description |
|---|---|
| `URammsArmController` | Widget for 7-joint Kinova Gen3 arm control |
| `URammsMebotController` | Widget for Mebot motor control |
| `URammsStatusPanel` | Displays robot state: mode, battery, speed, e-stop |

### UI Components

| Class | Description |
|---|---|
| `URammsBaseWidget` | Base class for all RammsUI widgets; provides style application, animation helpers, and layout utilities |
| `URammsButton` | Styled button widget |
| `URammsImageButton` | Button with image background |
| `URammsSlider` | Numeric slider |
| `URammsJoystickWidget` | 2D joystick input |
| `URammsRadioButton` | Radio button selection |
| `URammsCollapsibleContainer` | Collapsible panel container |
| `URammsToolbar` | Toolbar with customizable buttons |
| `URammsNotificationWidget` | Toast notifications with severity levels |
| `URammsNotificationContainer` | Viewport container for stacking notifications |
| `URammsLayoutManager` | Responsive layout management component |

### Styling

| Class | Description |
|---|---|
| `URammsUIStyle` | Data asset for centralized UI theming (colors, fonts, spacing) |

### Remote Control

| Class | Description |
|---|---|
| `URammsRemoteBridge` | Static function library for Remote Control API integration — widget discovery, status panel updates, and notifications |

### Interfaces

| Interface | Description |
|---|---|
| `IRammsCameraProvider` | Provides camera frame data |
| `IRammsStateProvider` | Provides and updates robot state |
| `IRammsDataSource` | Generic data source interface |
| `IRammsCommandSink` | Receives command inputs |

## Usage

### Adding a camera widget (C++)

```cpp
// Create the widget and bind a provider
URammsCameraWidget* CamWidget = CreateWidget<URammsCameraWidget>(GetWorld(), URammsCameraWidget::StaticClass());
CamWidget->AddProvider(MyCameraProvider);
CamWidget->AddToViewport();
```

### Bridging an external RMSS stream (C++)

```cpp
// Attach a stream bridge component to an actor — it auto-registers
// incoming RMSS streams as camera providers with intrinsics/extrinsics.
URammsStreamCameraBridge* Bridge = NewObject<URammsStreamCameraBridge>(MyActor);
Bridge->RegisterComponent();
```

### Blueprint

1. Add a **RammsCameraWidget** to your UMG layout.
2. In your level Blueprint, spawn an actor with a **RammsCameraProviderComponent** or **RammsStreamCameraBridge** and call **AddProvider** on the widget.
3. Use **RammsStatusPanel** to display robot state — bind it to any actor implementing the **IRammsStateProvider** interface.
4. Apply a **RammsUIStyle** data asset to any widget hierarchy via **URammsBaseWidget::SetStyle**.
