import json
import unittest
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace

import jinja2
import jinja2.sandbox
import yaml

class Loader(yaml.SafeLoader):
    pass

Loader.add_constructor("!input", lambda loader, node: loader.construct_scalar(node))
BLUEPRINT = yaml.load((Path(__file__).resolve().parents[1] / "home_assistant/blueprints/stopwatch_spotify.yaml").read_text(), Loader=Loader)

class MusicBlueprintTests(unittest.TestCase):
    def setUp(self):
        self.attributes = {
            "media_title": "Caffè al mare", "media_artist": "Artista", "source": "iPhone",
            "source_list": ["iPhone", "Mac", "🌙" * 32, "x" * 129],
            "media_position": 12, "media_duration": 180, "supported_features": 18481,
            "entity_picture": "/api/media_player_proxy/media_player.spotify?token=example&cache=1",
            "media_position_updated_at": 1000,
        }
        self.state = "playing"
        self.env = jinja2.sandbox.ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined)
        self.env.filters["to_json"] = lambda value, **kwargs: json.dumps(value, **kwargs)
        self.env.filters["regex_match"] = lambda value, pattern: bool(__import__("re").match(pattern, value))
        self.env.globals.update(
            state_attr=lambda entity, key: self.attributes.get(key), states=lambda entity: self.state,
            now=lambda: datetime.fromtimestamp(1005, tz=timezone.utc),
            as_timestamp=lambda value, default=None: value.timestamp() if isinstance(value, datetime) else value if isinstance(value, (int,float)) else default,
        )

    def render(self, template, **values):
        return self.env.from_string(template).render(**values).strip()

    def test_native_spotify_selector(self):
        selector = BLUEPRINT["blueprint"]["input"]["spotify"]["selector"]["entity"]["filter"]
        self.assertEqual(selector, [{"integration": "spotify", "domain": "media_player"}])
        self.assertEqual(BLUEPRINT["mode"], "single")

    def test_request_guard(self):
        guard = BLUEPRINT["actions"][1]["value_template"]
        for command, sent, source, expected in [
            ("refresh", 0, "", True), ("next", 1005, "", True),
            ("next", 999, "", False), ("next", 1006, "", False),
            ("source", 1005, "Mac", True), ("source", 1005, "Other", False),
            ("turn_off", 1005, "", False),
        ]:
            request = {"request": 12, "sent_at": sent, "source": source}
            self.assertEqual(self.render(guard, request=request, command=command, entity="media_player.spotify"), str(expected))
        for number in [True, -1, 4294967296, "12"]:
            self.assertEqual(self.render(guard, request={"request": number}, command="refresh", entity="media_player.spotify"), "False")

    def test_device_filter(self):
        guard = BLUEPRINT["conditions"][0]["value_template"]
        for device, expected in [("stopwatch_ec6244858428", True), ("another", False)]:
            self.assertEqual(self.render(guard, mqtt_id="stopwatch_ec6244858428", trigger=SimpleNamespace(topic=f"stopwatch/{device}/music/request")), str(expected))

    def test_only_refresh_forces_an_extra_update(self):
        action = BLUEPRINT["actions"][3]
        for state in ["playing", "paused", "idle"]:
            self.state = state
            for command in ["play", "pause", "next", "previous", "source", "playlist"]:
                self.assertEqual(self.render(action["if"], command=command, entity="media_player.spotify"), "True")
            self.assertEqual(self.render(action["if"], command="refresh", entity="media_player.spotify"), "False")
        for state in ["unknown", "unavailable", "off"]:
            self.state = state
            self.assertEqual(self.render(action["if"], command="play", entity="media_player.spotify"), "False")
        self.assertEqual(action["else"][0]["action"], "homeassistant.update_entity")
        self.assertTrue(action["else"][0]["continue_on_error"])
        self.assertEqual(BLUEPRINT["actions"][4]["action"], "mqtt.publish")

    def test_playlist_commands_and_uri_bounds(self):
        guard = BLUEPRINT["actions"][1]["value_template"]
        for command, value, expected in [
            ("playlists", {"offset": 0}, True), ("playlists", {"offset": 8}, True),
            ("playlists", {"offset": True}, False), ("playlists", {"offset": -8}, False),
            ("playlists", {"offset": 1}, False),
            ("playlist", {"playlist": "spotify:playlist:"+"A"*22}, True),
            ("playlist", {"playlist": "https://evil.example/stream"}, False),
            ("playlist", {"playlist": "spotify:track:"+"A"*22}, False),
        ]:
            request = {"request": 12, "sent_at": 1005, **value}
            self.assertEqual(self.render(guard, request=request, command=command, entity="media_player.spotify"), str(expected))

    def test_catalog_pagination_and_unicode(self):
        template = BLUEPRINT["actions"][2]["choose"][0]["sequence"][1]["data"]["payload"]
        children = [dict(title="🌙"*64, media_class="playlist", can_play=True, media_content_id="spotify:playlist:"+str(i).zfill(22)) for i in range(19)]
        children.append(dict(title="Invalid", media_class="playlist", can_play=True, media_content_id="http://evil/stream"))
        library = {"media_player.spotify": {"children":children}}
        for offset, expected in [(0,8),(8,8),(16,3)]:
            payload = self.render(template, request={"request":12,"offset":offset}, entity="media_player.spotify", library=library)
            self.assertLessEqual(len(payload.encode()),4096)
            page = json.loads(payload)["playlists"]
            self.assertEqual(page["total"],19)
            self.assertEqual(len(page["items"]),expected)
            self.assertEqual(page["items"][0]["uri"],"spotify:playlist:"+str(offset).zfill(22))
        unavailable = json.loads(self.render(template, request={"request":12,"offset":0}, entity="media_player.spotify"))
        self.assertFalse(unavailable["success"])
        self.assertFalse(json.loads(self.render(template, request={"request":12,"offset":0}, entity="media_player.spotify", library=None))["success"])
        objects = {"media_player.spotify": SimpleNamespace(children=[SimpleNamespace(**child) for child in children])}
        page = json.loads(self.render(template, request={"request":12,"offset":0}, entity="media_player.spotify", library=objects))
        self.assertTrue(page["success"])
        self.assertEqual(len(page["playlists"]["items"]),8)

    def payload(self):
        return self.render(BLUEPRINT["actions"][-1]["data"]["payload"], request={"request": 12}, entity="media_player.spotify", ha_url="http://192.168.0.150:8123")

    def test_metadata_progress_and_bounds(self):
        reply = json.loads(self.payload())
        self.assertEqual(reply["playback"]["position_ms"], 17000)
        self.assertEqual(reply["playback"]["title"], "Caffè al mare")
        self.assertEqual(len(reply["playback"]["sources"]), 3)
        self.state = "paused"
        self.assertEqual(json.loads(self.payload())["playback"]["position_ms"], 12000)
        self.attributes["media_position"] = -1
        self.assertEqual(json.loads(self.payload())["playback"]["position_ms"], 0)
        self.attributes["media_position"] = 99999
        self.assertEqual(json.loads(self.payload())["playback"]["position_ms"], 180000)
        self.state = "unavailable"
        self.assertFalse(json.loads(self.payload())["success"])

    def test_unicode_payload_fits_packet(self):
        for key in ["media_title", "media_artist", "source"]:
            self.attributes[key] = "🌙" * 64
        self.attributes["source_list"] = ["🌙" * 31 + str(i) for i in range(12)]
        self.attributes["entity_picture"] = "https://i.scdn.co/image/" + "a" * 480
        payload = self.payload()
        self.assertLessEqual(len(payload.encode()), 4096)
        self.assertEqual(len(json.loads(payload)["playback"]["sources"]), 12)

if __name__ == "__main__":
    unittest.main()
