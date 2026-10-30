# Home Assistant and Spotify

[Italiano](README.it.md) · [Project](../README.md)

These integrations are optional. Use your own Home Assistant installation, MQTT broker and Spotify integration. Never copy credentials into this repository. The blueprint declares Home Assistant 2024.10 as its syntax baseline; current integration behavior still needs testing on the version you use.

1. Configure MQTT in Home Assistant. In Companion, enter the broker hostname, port and credentials; use TLS outside a trusted local network. Discovery creates the StopWatch device and its sensors/controls.
2. Copy `blueprints/stopwatch_favorites.yaml` into your Home Assistant `config/blueprints/automation/stopwatch/` directory, reload blueprints and create an automation from it. Select the discovered StopWatch and up to eight entities. Leave unused slots blank. Italian forms are available as `stopwatch_favorites.it.yaml`; install one language variant, not duplicate automations.
3. Save and allow synchronization. Labels come from your Home Assistant entities. Lights/switches/fans toggle; scenes/scripts run; buttons press; other entities are read-only. Dimmable lights expose a separate brightness confirmation on StopWatch. Commands use actual state confirmation, not optimistic success.
4. For Spotify, authorize the official Home Assistant Spotify integration with your account, then install `stopwatch_spotify.yaml` (or the `.it.yaml` form). Create one automation per StopWatch and select the account. If needed, supply a Home Assistant URL reachable by the device for artwork. Spotify controls the active player; StopWatch does not play Spotify audio.

Use both blueprints from this version when updating firmware. The selectors accept the current `StopWatch C152` discovery model and the previous model name, without changing MQTT device IDs, topics or your entity selections.

## Internet through iPhone

Optional: configure a Home Assistant HTTPS URL reachable over cellular and an administrator token in Companion → Internet via iPhone. The token stays in the iOS keychain. The bridge reuses MQTT blueprints through Home Assistant's HTTPS/WebSocket APIs; do not expose your broker to the Internet. Direct device Wi-Fi has priority.

The bridge's subscription flow requires administrator scope. It is subject to iOS background execution limits: unlock after reboot and avoid force-quitting Companion. Confirm a real command with iPhone locked before relying on this path.

## Troubleshooting

Check broker connectivity, blueprint/device selection, entity availability, certificate/clock and actual Home Assistant state. A stale state disables new actions until refreshed. An uncertain result means check the target before retrying, especially scenes and scripts. Some cloud-connected lights report state late; configure their integration according to its own documentation rather than adding repeated commands.

Manual REST favorites remain available in Companion. Updating a section preserves unrelated settings and omitted optional fields. A later blueprint synchronization can replace manual favorites with its managed list.
