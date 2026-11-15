# USB Bridge

[Italiano — protocol and controls](USB_CONTROL.it.md)

Connect StopWatch to a computer with a USB data cable. The normal firmware exposes USB CDC plus standard HID keyboard/mouse support. FIDO is not active in this build.

For phone control, pair Companion, open Bridge on StopWatch and USB Control on iPhone. One finger moves the pointer, two fingers scroll, one-finger tap left-clicks, and two-finger tap right-clicks. Type into the selected field on the computer using the Companion keyboard. Choose the computer's actual keyboard layout (US/ABC or Italian PC); unsupported characters/emoji are rejected rather than silently substituted.

Input is gated by the device screen and session. Disconnects, suspension and screen transitions invalidate remote input. The fast BLE mouse channel is best-effort; commands are bounded and stale sessions are rejected. Keep the intended target application focused on the computer.

The standalone Slide remote is not in the launcher of the current build.

See [development checks](DEVELOPMENT.md) for HID/session tests. Actual pointer latency, and host layout behavior require a physical USB test on each supported host.
