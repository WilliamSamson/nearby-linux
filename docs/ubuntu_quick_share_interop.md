# Ubuntu Quick Share Interoperability

## Goal

Build a real Ubuntu desktop implementation that interoperates with Quick Share
and legacy Nearby Share peers. The app must use the existing Quick Share and
Nearby Connections C++ stacks in this repository; it must not implement a
parallel file sharing protocol with similar UI.

## Assumptions

- Initial target OS is Ubuntu desktop on Linux.
- Initial interoperability target is receiving a single file from Android or
  Windows Quick Share.
- Build system stays Bazel for C++ libraries. The GTK desktop app is built with
  meson because the GTK4/libadwaita ecosystem is meson-native.
- UI is the first visible milestone, but the UI links to a `ShareSessionFacade`
  rather than `NearbySharingService` directly. The current C facade owns setup
  persistence and runtime capability checks, and now starts a real BlueZ BLE
  fast advertisement when receive visibility is enabled. It still does not wrap
  `NearbySharingServiceImpl` or receive payload bytes.
- The existing `internal/platform/implementation/g3` backend is test-only. It is
  useful as a reference for API shape, but it cannot discover or connect to real
  devices because it uses in-process `MediumEnvironment` simulation.

## Non-Goals For The First Milestone

- No custom LAN-only protocol.
- No sender flow.
- No account or contacts integration beyond what is required for visible-to-all
  receive testing.
- No packaging, autostart, or tray UI.
- No claim that the UI works end-to-end with real peers before slice 5 lands.
  The mock facade only proves UI plumbing and visuals.

## Existing Architecture

- `sharing/nearby_sharing_service.*` is the application-facing service API.
- `sharing/nearby_sharing_service_impl.*` owns receive surfaces, discovery,
  advertising, incoming sessions, outgoing sessions, and transfer metadata.
- `sharing/incoming_share_session.*` and `sharing/outgoing_share_session.*`
  implement transfer state handling.
- `sharing/nearby_connections_manager_impl.*` bridges Nearby Sharing to Nearby
  Connections.
- `connections/implementation/*` implements the Nearby Connections protocol and
  bandwidth upgrade machinery.
- `internal/platform/implementation/platform.h` defines the OS boundary.
- Production platform backends exist for Apple and Windows. Linux currently has
  only the `g3` test backend.

## Required Ubuntu Platform Boundary

The Ubuntu backend must implement the `nearby::api::ImplementationPlatform`
factory methods with real OS primitives:

- Files: input/output files, download path, app data path.
- Executors and timers: thread pools, scheduled work, cancellation behavior.
- Device info: OS type, device name, device type.
- Preferences and credential storage: persistent local settings and keys.
- HTTP loader: required by certificate/contact/device RPC flows when enabled.
- Bluetooth adapter and BLE medium: scanning, advertising, GATT, sockets where
  required by Nearby Connections.
- Wi-Fi LAN medium: mDNS/DNS-SD advertise/discover, TCP listen/connect.
- Wi-Fi Direct and hotspot: either real implementations or explicit unsupported
  behavior that does not poison the receiver-first path.

## UI Architecture

The desktop app is a GTK4 + libadwaita binary written in C, built with meson,
living under `app/ui-gtk/`. It depends on a narrow C facade:

```
[ GTK4 / libadwaita UI (C, meson) ]
              |
              v
[ ShareSessionFacade (extern "C" header) ]
              |
   +----------+----------+
   |                     |
[ Facade ]
   (C setup + BlueZ discovery today,
    C++ NearbySharingServiceImpl wrapper next)
                         |
                         v
              [ NearbySharingService ]
                         |
                         v
              [ Linux platform backend ]
```

The UI never includes `sharing/*.h`. Swapping `MockFacade` for `RealFacade` is
the only seam the UI sees when the real backend lands.

### UI State Model

The single source of truth is `sharing/transfer_metadata.h::Status`. The UI
collapses the 15-value enum into 5 screens:

| `TransferMetadata::Status`                                   | Screen        | Actions                |
| ------------------------------------------------------------ | ------------- | ---------------------- |
| (no active session)                                          | Idle/Visible  | Toggle visibility      |
| kConnecting, kAwaitingRemoteAcceptance                       | Incoming      | Cancel                 |
| kAwaitingLocalConfirmation                                   | Incoming      | Accept / Reject        |
| kInProgress                                                  | Transferring  | Cancel                 |
| kComplete                                                    | Complete      | Open folder / Done     |
| kRejected, kCancelled                                        | toast → Idle  | (auto)                 |
| kFailed, kTimedOut, kMediaUnavailable, kNotEnoughSpace,      | Failed        | Retry / Dismiss        |
| kUnsupportedAttachmentType, kDeviceAuthenticationFailed,     |               |                        |
| kIncompletePayloads                                          |               |                        |

Responsive layout uses a single `AdwBreakpoint` at 600px width. Large/medium
windows show stacked content with peer detail visible alongside transfer
progress; small windows collapse to single-column. No glow or gradient effects.

## Receiver-First MVP State Machine

1. `Idle`
   - No receive surface registered.
   - No advertising.

2. `Visible`
   - UI registers a foreground receive surface via the facade.
   - `NearbySharingServiceImpl` advertises via `NearbyConnectionsManager`.
   - Ubuntu appears as a share target on Android/Windows.

3. `IncomingConnection`
   - Remote peer connects through Nearby Connections.
   - Service creates an `IncomingShareSession`.

4. `AwaitingLocalConfirmation`
   - Introduction frame has attachment metadata.
   - UI prompts accept/reject.

5. `Transferring`
   - Accepted transfer receives payload bytes.
   - File output path is inside Downloads by default.

6. `Complete`
   - Transfer metadata reaches `kComplete`.
   - UI shows saved file path and offers open-folder.

7. `Failed`
   - Any protocol, medium, auth, file, or timeout failure yields an explicit
     transfer status and diagnostic log.

## First Verifiable Implementation Slices

1. Toolchain gate (DONE)
   - Bazelisk installed at `/tmp/bazelisk`.
   - `wifi_utils_test` passes without source modification.

2. UI shell with setup persistence and backend capability gate (DONE)
   - `app/ui-gtk/` meson project: AdwApplicationWindow, libadwaita styling.
   - `app/facade/share_session_facade.h`: the extern "C" UI ↔ service contract.
   - `app/facade/nearby_facade.c`: setup persistence, backend readiness checks,
     and a guarded BlueZ BLE fast advertisement path.
   - Responsive layout validated at large, medium (600-900px), small (<600px).
   - Acceptance: app launches; onboarding is one-time; no references to
     `sharing/*.h` from UI code.

3. Linux platform scaffold (IN PROGRESS)
   - Non-test `internal/platform/implementation/linux` backend target.
   - Reuse shared POSIX primitives where already available.
   - Local Bluetooth adapter identity is now exposed through
     `CreateBluetoothAdapter()` using the Linux adapter path; privileged power,
     discoverability, and name mutations remain explicit no-ops until the BlueZ
     D-Bus medium layer lands.
   - Return explicit `Unimplemented` or null for mediums not implemented yet.
   - Narrow tests for path, device info, preferences, Bluetooth adapter
     identity, and file behavior.

4. Real BLE medium
   - Move the BlueZ D-Bus advertisement code out of the C facade and into
     `api::ble::BleMedium`.
   - Implement scanning, GATT advertisement read server/client, and BLE socket
     characteristics required by Nearby Connections.
   - Validate Android discovery with `NearbySharingServiceImpl` endpoint info,
     not hand-built C facade bytes.

5. Real Wi-Fi LAN medium
   - TCP listen/connect and mDNS/DNS-SD advertise/discover.
   - Validate with two Ubuntu processes on one LAN.
   - Keep the public API compatible with `api::WifiLanMedium`.

6. Real facade (wire UI to NearbySharingService)
   - Replace `MockFacade` with `RealFacade` in app builds.
   - Foreground receive surface registered when window enters Visible state.
   - Transfer metadata callbacks dispatched to the GTK main loop.

7. End-to-end receiver
   - Android or Windows Quick Share sends one file to Ubuntu.
   - Ubuntu accepts and saves the file via the GTK UI.

## Acceptance Criteria

- No fake medium is used in interoperability tests.
- A real Android or Windows Quick Share sender can discover the Ubuntu device.
- A single file can be accepted and saved to Downloads.
- Transfer progress and final status are observable in the GTK UI.
- Failures are surfaced as explicit transfer statuses, not silent exits.
- Existing sharing/session tests still pass.
- The UI slice's acceptance is plumbing and visuals only. Slice 2 passing does
  not imply any interoperability claim.

## Current Blockers

- `bazel` is not installed system-wide; Bazelisk is available at
  `/tmp/bazelisk` for local verification in this workspace.
- The production Linux platform backend exists but still lacks real BLE GATT,
  BLE socket, and Wi-Fi LAN medium implementations.
- GTK4/libadwaita meson builds pass in `/tmp/quick-share-ui-build`.
- Host BlueZ exposes `org.bluez.LEAdvertisingManager1`, Avahi is running, and
  the GTK app can hold one active Quick Share fast BLE advertisement.
- Android file receive is still blocked on real `NearbySharingServiceImpl`
  facade wiring plus Linux BLE GATT socket and Wi-Fi LAN upgrade mediums.
