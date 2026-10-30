import json
import re
import unittest
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace

import jinja2
import yaml


class Loader(yaml.SafeLoader):
    pass


Loader.add_constructor("!input", lambda loader, node: loader.construct_scalar(node))
BLUEPRINT = yaml.load(
    (Path(__file__).resolve().parents[1] / "home_assistant/blueprints/stopwatch_favorites.yaml").read_text(),
    Loader=Loader,
)


class BlueprintTests(unittest.TestCase):
    def test_brightness_capability_validation_action_and_confirmation(self):
        self.setUp()
        attributes = {"supported_color_modes": ["rgb"], "brightness": 153}
        self.env.globals["state_attr"] = lambda entity, key: attributes.get(key)
        sequence = BLUEPRINT["actions"][0]["choose"][0]["sequence"]
        now = self.env.globals["now"]().timestamp()
        request = {"request": 12, "action": True, "brightness": 60, "sent_at": now}
        values = dict(entity="light.camera", request=request, service="toggle", selected=["light.camera"], domain="light")
        self.assertEqual(self.render(sequence[1]["value_template"], **values), "True")
        action = sequence[2]["then"][1]
        self.assertEqual(self.render(action["action"], **values), "light.turn_on")
        self.assertEqual(self.render(action["data"], **values), "{'brightness_pct': 60}")
        wait = sequence[2]["then"][2]["then"][0]["wait_template"]
        self.env.globals["states"] = lambda entity: "on"
        self.assertEqual(self.render(wait, **values), "True")
        attributes["brightness"] = 255
        self.assertEqual(self.render(wait, **values), "False")
        payload = json.loads(self.render(sequence[3]["data"]["payload"], **values))
        self.assertTrue(payload["dimmable"]); self.assertEqual(payload["brightness"], 100)
        for level in [-1, 101, True, 1.5, "50"]:
            request["brightness"] = level
            self.assertEqual(self.render(sequence[1]["value_template"], **values), "False")
        request["brightness"] = 0
        self.assertEqual(self.render(action["action"], **values), "light.turn_off")
        self.assertEqual(self.render(action["data"], **values), "{}")
        self.env.globals["states"] = lambda entity: "off"
        self.assertEqual(self.render(wait, **values), "True")
        attributes["supported_color_modes"] = ["onoff"]
        self.assertEqual(self.render(sequence[1]["value_template"], **values), "False")
        payload = json.loads(self.render(sequence[3]["data"]["payload"], **values))
        self.assertFalse(payload["dimmable"]); self.assertEqual(payload["brightness"], -1)
    def setUp(self):
        self.env = jinja2.Environment(undefined=jinja2.StrictUndefined)
        self.env.filters["to_json"] = lambda value, **kwargs: json.dumps(value, **kwargs)
        self.env.filters["regex_match"] = lambda value, pattern: bool(re.match(pattern, value))
        self.env.globals.update(
            device_attr=lambda device, key: {("mqtt", "stopwatch_ec6244858428")},
            state_attr=lambda entity, key: "Caffè ☀️" if key == "friendly_name" else "°C",
            states=lambda entity: "off",
            now=lambda: datetime(2026, 10, 24, tzinfo=timezone.utc),
            as_timestamp=lambda value: value.timestamp(),
        )

    def render(self, template, **values):
        return self.env.from_string(template).render(**values).strip()

    def test_eight_native_selectors(self):
        inputs = BLUEPRINT["blueprint"]["input"]
        self.assertEqual(len(inputs), 9)
        for index in range(1, 9):
            self.assertEqual(inputs[f"favorite_{index}"]["selector"], {"entity": {}})
            self.assertEqual(inputs[f"favorite_{index}"]["default"], "")

    def test_config_names_order_deduplication_and_unicode(self):
        selected = json.loads(self.render("{{ slots | reject('equalto', '') | unique | list | to_json }}", slots=["light.studio", "", "scene.cinema", "light.studio"]))
        self.assertEqual(selected, ["light.studio", "scene.cinema"])
        payload = self.render(BLUEPRINT["actions"][0]["default"][1]["data"]["payload"], selected=selected)
        favorites = json.loads(payload)["favorites"]
        self.assertEqual([item["entity"] for item in favorites], selected)
        self.assertEqual(favorites[0]["label"], "Caffè ☀️")

    def test_largest_config_fits_firmware_limit(self):
        self.env.globals["state_attr"] = lambda entity, key: "🌙" * 32
        selected = ["light." + "a" * 89 + str(i) for i in range(8)]
        payload = self.render(BLUEPRINT["actions"][0]["default"][1]["data"]["payload"], selected=selected)
        self.assertLessEqual(len(payload.encode()), 2048)
        self.assertEqual(len(json.loads(payload)["favorites"]), 8)

    def test_only_selected_entities_and_fresh_actions(self):
        check = BLUEPRINT["actions"][0]["choose"][0]["sequence"][1]["value_template"]
        current = self.env.globals["now"]().timestamp()
        for entity, action, sent_at, service, expected in [
            ("light.studio", True, current, "toggle", True),
            ("light.studio", True, current - 6, "toggle", False),
            ("light.studio", True, current + 1, "toggle", False),
            ("light.other", True, current, "toggle", False),
            ("light.studio", True, current, "", False),
            ("light.studio", False, 0, "", True),
        ]:
            request = {"request": 12, "action": action, "sent_at": sent_at}
            value = self.render(check, entity=entity, request=request, service=service, selected=["light.studio"])
            self.assertEqual(value, str(expected))

    def test_service_allowlist_matches_firmware(self):
        template = BLUEPRINT["actions"][0]["choose"][0]["sequence"][0]["variables"]["service"]
        for domain, expected in [("light", "toggle"), ("switch", "toggle"), ("scene", "turn_on"),
                                 ("script", "turn_on"), ("input_button", "press"), ("sensor", ""), ("lock", "")]:
            self.assertEqual(self.render(template, domain=domain), expected)

    def test_other_devices_are_ignored(self):
        template = BLUEPRINT["conditions"][1]["value_template"]
        self.assertEqual(self.render(template, mqtt_id="stopwatch_ec6244858428", trigger=SimpleNamespace(platform="mqtt", topic="stopwatch/another/favorite/request")), "False")

    def test_scene_can_be_activated_before_it_has_a_timestamp(self):
        self.env.globals["states"] = lambda entity: "unknown"
        condition = BLUEPRINT["actions"][0]["choose"][0]["sequence"][2]["if"]
        self.assertEqual(self.render(condition, request={"action": True}, entity="scene.cinema"), "True")

    def test_toggle_waits_for_actual_state_without_a_fixed_delay(self):
        actions = BLUEPRINT["actions"][0]["choose"][0]["sequence"][2]["then"]
        self.assertIn("previous_state", actions[0]["variables"])
        self.assertIn("action", actions[1])
        self.assertEqual(self.render(actions[2]["if"], service="toggle"), "True")
        self.assertEqual(self.render(actions[2]["if"], service="turn_on"), "False")
        wait = actions[2]["then"][0]
        self.assertEqual(wait["timeout"], {"seconds": 3})
        self.assertTrue(wait["continue_on_timeout"])
        for previous, current, expected in [("off", "on", True), ("on", "off", True),
                                             ("on", "on", False), ("off", "off", False),
                                             ("off", "unknown", False), ("on", "unavailable", False)]:
            self.env.globals["states"] = lambda entity, state=current: state
            self.assertEqual(self.render(wait["wait_template"], entity="light.studio", previous_state=previous), str(expected))


if __name__ == "__main__":
    unittest.main()
