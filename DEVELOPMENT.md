# RammsUI Plugin — Development Guide

This guide covers the architecture, patterns, and workflows for developing UI
with the RammsUI plugin. It is intended for engineers adding new widgets,
creating layout presets, or extending the system.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Widget Class Hierarchy](#widget-class-hierarchy)
3. [Widget Lifecycle](#widget-lifecycle)
4. [Creating a New Widget](#creating-a-new-widget)
5. [Communication Patterns](#communication-patterns)
6. [Layout System & Presets](#layout-system--presets)
7. [Styling](#styling)
8. [Interfaces](#interfaces)
9. [Quick Reference](#quick-reference)

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                    URammsUISubsystem                             │
│    (Robot Controller Registry + UI Event Bus)                    │
└──────┬───────────────────────────────────────────┬───────────────┘
       │ auto-discovery                            │ events
       ▼                                           ▼
┌─────────────────────┐     ┌─────────────────────────────────────┐
│ IRammsRobotController│     │ URammsLayoutManager                 │
│ (interface on Actor) │     │ (ActorComponent — manages presets)  │
└─────────────────────┘     └─────────────────────────────────────┘
       ▲                                           │
       │ Execute_*()                               │ position / visibility
       │                                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                  URammsBaseWidget (base class)                    │
│  ┌────────────┐ ┌────────────────┐ ┌──────────────────────────┐  │
│  │ Styling    │ │ Animation      │ │ Robot Controller Binding │  │
│  │ (DataAsset)│ │ (tick-driven)  │ │ (auto / explicit)        │  │
│  └────────────┘ └────────────────┘ └──────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
       ▲
       │ inherits
       │
┌──────┴──────────────────────────────────────────────────────────┐
│ URammsMebotController │ URammsArmController │ URammsArmTaskWidget│
│ URammsTaskWidget      │ URammsCameraWidget  │ URammsStatusPanel  │
│ URammsJoystickWidget  │ URammsButton        │ URammsPanel ...    │
└─────────────────────────────────────────────────────────────────┘
```

### Key Principles

| Principle | How It's Applied |
|-----------|-----------------|
| Composition over inheritance | Base class provides services; derived widgets compose from UMG components |
| Interface-based dependencies | `IRammsRobotController`, `IRammsStateProvider`, `IRammsCameraProvider` decouple transport |
| Event-driven | `URammsUISubsystem` event bus eliminates direct widget-to-actor coupling |
| Weak references | Controller registry uses `TWeakObjectPtr` — supports late binding, no GC blocking |
| Programmatic construction | `BuildWidgetTree()` enables runtime modification + designer preview |
| Hierarchical styling | `URammsUIStyle` DataAsset propagates to children with per-widget overrides |

---

## Widget Class Hierarchy

All custom widgets inherit from `URammsBaseWidget` (which extends `UUserWidget`).

### Control Widgets (send commands to robot)
- **`URammsMebotController`** — MEBot wheelchair modes (Self-Level, Curb Ascent/Descent)
- **`URammsArmController`** — Arm home/retract actions
- **`URammsArmTaskWidget`** — Arm task selection (Open Door, Order Drink, Drink)
- **`URammsTaskWidget`** — Task-level Exit/Cancel (event-bus driven)
- **`URammsJoystickWidget`** — Virtual joystick (-1 to 1)

### Display Widgets (show robot state)
- **`URammsCameraWidget`** — Multi-mode camera display (Fullscreen/Window/Corner, RGB/Depth/SbS/Overlay)
- **`URammsStatusPanel`** — Speed, battery, mode, connection, arm status

### Container Widgets
- **`URammsPanel`** — Styled container with optional header/border, NamedSlot
- **`URammsCollapsibleContainer`** — Expandable container with 7 animation types

### Building Blocks
- **`URammsButton`** — Styled button (Normal/Hover/Press/Disabled states)
- **`URammsImageButton`** — Icon button with toggle mode
- **`URammsSlider`** — Styled slider with optional panel background
- **`URammsRadioButton`** — Group-based mutual exclusion
- **`URammsToolbar`** — Horizontal/Vertical action toolbar
- **`URammsNotificationWidget`** — Toast popup (Info/Success/Warning/Error)
- **`URammsNotificationContainer`** — Toast stack manager

---

## Widget Lifecycle

```
Constructor (CDO)
  │
  ▼
Initialize()
  │  ← Super sets up WidgetTree
  │  ← BuildWidgetTree() called here (tree must exist before RebuildWidget)
  │
  ▼
NativePreConstruct()
  │  ← Designer preview path
  │  ← SynchronizeProperties() on property edits
  │
  ▼
NativeOnInitialized()    [runtime only]
  │  ← BuildWidgetTree() also called here for safety
  │
  ▼
NativeConstruct()        [runtime only]
  │  ← Bind delegates (button clicks, etc.)
  │  ← Auto-apply style if bAutoApplyStyle
  │  ← ResolveController() if bAutoFindRobotController
  │
  ▼
NativeTick()             [runtime only]
  │  ← Animation updates
  │
  ▼
BeginDestroy()
  │  ← Unsubscribe from registry delegate
```

### Important Notes

- **Build in `Initialize()`**: `RebuildWidget()` reads WidgetTree *before*
  `NativePreConstruct()` runs, so the tree must be built in `Initialize()`
  (after `Super::Initialize()`) for designer preview to work.
- **`NativeOnInitialized()` is runtime-only**: Never called in the editor.
- **`NativePreConstruct()` takes no arguments** in UE 5.7. Use `IsDesignTime()`
  to detect design time.
- **Guard against double-building**: Check `if (!WidgetTree || RootWidget)`.

---

## Creating a New Widget

### Checklist

| Step | File(s) | Notes |
|------|---------|-------|
| 1. Add enum values | `RammsRobotTypes.h` | Only if new action/mode types are needed |
| 2. Add interface methods | `IRammsRobotController.h` | `BlueprintNativeEvent` — do NOT provide `_Implementation` body |
| 3. Add event bus broadcasts | `RammsUISubsystem.h/.cpp` | Only if cross-widget/actor notification is needed |
| 4. Create widget `.h` | `Public/UI/` | Inherit from `URammsBaseWidget` |
| 5. Create widget `.cpp` | `Private/UI/` | Follow patterns below |
| 6. Add icon assets | `Content/UI/Icons/` | Naming: `task-xxx`, `control-xxx` |

### Step-by-Step: Widget Header

```cpp
#pragma once
#include "CoreMinimal.h"
#include "UI/RammsBaseWidget.h"
#include "UI/RammsImageButton.h"
#include "RammsRobotTypes.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "MyNewWidget.generated.h"

UCLASS(meta = (DisplayName = "My New Widget"))
class RAMMSUI_API UMyNewWidget : public URammsBaseWidget
{
    GENERATED_BODY()

public:
    UMyNewWidget(const FObjectInitializer& ObjectInitializer);

protected:
    // ── Editable Properties ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyWidget")
    FText HeaderTitle = FText::FromString(TEXT("My Widget"));

    // ── Widget References (built programmatically) ──
    UPROPERTY()
    TObjectPtr<UBorder> PanelBorder;

    // ... more widget pointers

public:
    // ── Delegate ──
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSomethingChanged, int32, NewValue);
    UPROPERTY(BlueprintAssignable, Category = "MyWidget")
    FOnSomethingChanged OnSomethingChanged;

    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void ApplyStyle_Implementation() override;
    virtual void SynchronizeProperties() override;

protected:
    virtual void ResetCachedWidgets() override;
    virtual void BuildWidgetTree() override;
    virtual void OnRobotControllerResolved(AActor* ControllerActor) override;
};
```

### Step-by-Step: Widget Implementation

```cpp
#include "UI/MyNewWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Interfaces/IRammsRobotController.h"

UMyNewWidget::UMyNewWidget(const FObjectInitializer& OI)
    : Super(OI)
{
    bAutoFindRobotController = true;  // set false for pure-UI widgets
}

void UMyNewWidget::ResetCachedWidgets()
{
    PanelBorder = nullptr;
    // null out all cached UWidget* pointers
}

void UMyNewWidget::BuildWidgetTree()
{
    if (!WidgetTree || PanelBorder)  // guard against double-build
        return;

    PanelBorder = WidgetTree->ConstructWidget<UBorder>(...);
    WidgetTree->RootWidget = PanelBorder;
    // ... build the rest of the tree
}

void UMyNewWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
}

void UMyNewWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Bind delegates here (button clicks, etc.)
}

void UMyNewWidget::ApplyStyle_Implementation()
{
    if (!Style) return;
    // Apply Style->Colors, Style->Typography, Style->Spacing to widgets
    // Propagate to child RammsBaseWidgets: ChildWidget->SetStyle(Style);
}

void UMyNewWidget::OnRobotControllerResolved(AActor* ControllerActor)
{
    // Query initial state:
    // auto State = IRammsRobotController::Execute_GetXxx(ControllerActor);
}
```

---

## Communication Patterns

### Pattern A: Direct Robot Controller (MebotController, ArmController, ArmTaskWidget)

Use when the widget **sends commands to a specific robot**.

```
User clicks button
  → OnButtonClicked() handler
  → Update local state
  → Broadcast widget-level delegate (OnModeChanged, etc.)
  → IRammsRobotController::Execute_RequestXxx(GetResolvedControllerActor(), ...)
```

Key points:
- Set `bAutoFindRobotController = true` in constructor
- Override `OnRobotControllerResolved()` to sync initial state
- Use `Execute_*()` statics (works for both C++ and BP implementers)

### Pattern B: Event Bus (TaskWidget)

Use when the widget **coordinates UI-level actions** not tied to one robot.

```
User clicks button
  → OnButtonClicked() handler
  → GetWorld()->GetSubsystem<URammsUISubsystem>()->BroadcastXxx(...)
  → Listeners anywhere subscribe via subsystem delegate
```

### Pattern C: Camera Streaming (CameraWidget)

```
SetCameraProvider(Provider)
  → StartStream(StreamID)
  → IRammsCameraProvider::OnCameraFrameReady fires
  → Update texture display
```

### Pattern D: State Polling (StatusPanel)

```
SetStateProvider(Provider)
  → Subscribe: OnRobotStateUpdate.Add(...)
  → Periodic: GetRobotState(OutState)
  → Update display widgets
```

---

## Layout System & Presets

### URammsLayoutManager

An `ActorComponent` attached to your PlayerController or GameMode. Manages
widget positions using normalized (0-1) coordinates.

#### Built-in Presets

| Preset | Behavior |
|--------|----------|
| `SingleFullscreen` | First widget fills screen, others hidden |
| `Grid` | Configurable columns × rows |
| `PictureInPicture` | One large (80%) + corners (20%) |
| `SideBySide` | Split vertical |
| `Custom` | Manual positions via `SetWidgetPosition()` |

#### Data Asset Presets (URammsLayoutPresetAsset)

For complex, designer-configurable layouts, create `URammsLayoutPresetAsset`
instances in the editor:

1. **Right-click** in Content Browser → Miscellaneous → Data Asset
2. Select **RammsLayoutPresetAsset**
3. Fill in `Entries`: each entry specifies a `WidgetTag`, position, size, z-order, visibility
4. Optionally set `WidgetClass` for auto-creation

#### Registering Widgets

```cpp
// In your setup code (GameMode, PlayerController, etc.)
URammsLayoutManager* LayoutMgr = FindComponentByClass<URammsLayoutManager>();

// Register widgets with tags for preset matching
LayoutMgr->AddWidget(MyCameraWidget, /*ZOrder=*/ 0);
LayoutMgr->SetWidgetTag(MyCameraWidget, FName("CameraMain"));

LayoutMgr->AddWidget(MyArmTaskWidget, /*ZOrder=*/ 10);
LayoutMgr->SetWidgetTag(MyArmTaskWidget, FName("ArmTaskSelector"));
```

#### Transitioning Between Presets

```cpp
// Using built-in presets
LayoutMgr->TransitionTo(ERammsLayoutPreset::Grid, /*bAnimated=*/ true);

// Using data asset presets
UPROPERTY(EditAnywhere) URammsLayoutPresetAsset* SystemOverviewPreset;
UPROPERTY(EditAnywhere) URammsLayoutPresetAsset* ArmCenteredPreset;

LayoutMgr->TransitionToPreset(SystemOverviewPreset, /*bAnimated=*/ true);
```

#### Designing a Preset (Example: Arm Centered)

```
┌────────────────────────────────────────────────────────────┐
│                          3D Arm Pose View                   │
│                         (0,0 → 0.6,0.7)                   │
│                                                             │
│                                    ┌──────────────────────┐│
│                                    │  Wrist Camera        ││
│                                    │  (0.6,0 → 0.4,0.5)  ││
│                                    │                      ││
│                                    └──────────────────────┘│
│                                    ┌──────────────────────┐│
│                                    │  Depth Overlay       ││
│                                    │  (0.6,0.5 → 0.4,0.5)││
│                                    └──────────────────────┘│
├─────────────────────┬──────────────────────────────────────┤
│  Arm Task Selector  │  Task Status / Exit                  │
│  (0,0.7 → 0.4,0.3) │  (0.4,0.7 → 0.6,0.3)               │
└─────────────────────┴──────────────────────────────────────┘
```

---

## Styling

All widgets share a common `URammsUIStyle` DataAsset containing:

- **Colors**: Primary, Secondary, Background, Surface, Text, Status (Success/Warning/Error/Info)
- **Typography**: HeadingLarge (32px), HeadingMedium (24px), HeadingSmall (18px), Body (14px), Caption (12px), Monospace (14px)
- **Spacing**: XSmall (2px), Small (4px), Medium (8px), Large (16px), XLarge (24px), XXLarge (32px)
- **Border**: Width (1px standard, 2px thick), Corner Radii (2/4/8/12px)
- **Animation**: Duration, Easing, Delay curves

### Propagation

Set `bPropagateStyleToChildren = true` on container widgets. Call
`ChildWidget->SetStyle(Style)` in your `ApplyStyle_Implementation()` override.

The default style asset lives at: `Plugins/RammsUI/Content/UI/UI_Style.uasset`

---

## Interfaces

### IRammsRobotController
High-level robot command interface. Widgets auto-discover actors implementing
this via the subsystem registry.

**Calling convention** (always use statics for BP compatibility):
```cpp
IRammsRobotController::Execute_RequestArmAction(Actor, ERammsArmAction::Home);
IRammsRobotController::Execute_RequestArmTask(Actor, ERammsArmTask::OpenDoor);
```

**Do NOT** provide `_Implementation` bodies — `GENERATED_BODY()` auto-generates
default no-op implementations.

### IRammsStateProvider
Transport-agnostic robot state reception. Provides `FRammsRobotState`,
`FRammsArmState`, `FRammsCurbInfo` with delegate-based update notifications.

### IRammsCameraProvider
Camera frame streaming. Manages stream lifecycle (start/stop), provides
`UTexture*` access, and fires `OnCameraFrameReady` per frame.

### IRammsCommandSink
Low-level command dispatch (movement, joint, mode, generic, e-stop).
Returns command IDs with `OnCommandAck` acknowledgment.

### IRammsDataSource
Extensible data stream reception (cameras, state, joints, transforms, sensors).

---

## Quick Reference

### File Locations

```
Plugins/RammsUI/Source/RammsUI/
├── Public/
│   ├── RammsRobotTypes.h              ← Shared enums
│   ├── RammsUIEventTypes.h            ← Event bus enums + FRammsUIEvent
│   ├── RammsUISubsystem.h             ← World subsystem (registry + events)
│   ├── Interfaces/
│   │   ├── IRammsRobotController.h    ← Main robot interface
│   │   ├── IRammsStateProvider.h      ← State reception interface
│   │   ├── IRammsCameraProvider.h     ← Camera streaming interface
│   │   ├── IRammsCommandSink.h        ← Command dispatch interface
│   │   └── IRammsDataSource.h         ← Data stream interface
│   └── UI/
│       ├── RammsBaseWidget.h          ← Base class for all widgets
│       ├── RammsUIStyle.h             ← Styling DataAsset
│       ├── RammsLayoutManager.h       ← Layout management component
│       ├── RammsLayoutPresetAsset.h   ← Layout preset DataAsset
│       ├── RammsButton.h
│       ├── RammsImageButton.h
│       ├── RammsPanel.h
│       ├── RammsMebotController.h
│       ├── RammsArmController.h
│       ├── RammsArmTaskWidget.h
│       ├── RammsTaskWidget.h
│       ├── RammsCameraWidget.h
│       ├── RammsStatusPanel.h
│       ├── RammsJoystickWidget.h
│       └── ...
├── Private/
│   ├── RammsUISubsystem.cpp
│   └── UI/
│       ├── RammsBaseWidget.cpp
│       ├── RammsLayoutManager.cpp
│       ├── RammsMebotController.cpp
│       ├── RammsArmTaskWidget.cpp
│       └── ...
Content/UI/
├── Icons/                             ← task-xxx, control-xxx icons
├── UI_Style.uasset                    ← Default style DataAsset (in plugin)
├── W_DemoInterface.uasset             ← Main demo UI blueprint widget
└── WBP_VirtualCameras.uasset          ← Camera management widget
```

### Enum Reference

| Enum | Values |
|------|--------|
| `ERammsArmAction` | Home, Retract |
| `ERammsArmTask` | None, OpenDoor, OrderDrink, Drink |
| `ERammsMebotMode` | None, SelfLevel, CurbAscent, CurbDescent |
| `ERammsTaskAction` | Exit, Cancel |
| `ERammsLayoutPreset` | SingleFullscreen, Grid, PictureInPicture, SideBySide, Custom |
| `ERammsButtonState` | Normal, Hovered, Pressed, Disabled |
| `ERammsUIEasing` | Linear, EaseIn, EaseOut, EaseInOut, Bounce, Elastic |
| `ERammsNotificationLevel` | Info, Success, Warning, Error |
