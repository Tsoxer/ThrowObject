import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from stage_animations import CARRY_PRIORITY, OAR_PATH, carry_slots, stage
from check_oar_priority import conflicts


class OarTests(unittest.TestCase):
    def test_staged_bytes_and_threshold(self):
        with tempfile.TemporaryDirectory() as temp:
            data = Path(temp)
            stage(ROOT / "exported", data)
            carry = data / OAR_PATH / "Carry"
            config = json.loads((carry / "config.json").read_text())
            self.assertEqual(config["priority"], 2147483647)
            self.assertTrue(config["interruptible"] and config["replaceOnLoop"] and config["replaceOnEcho"])
            player, flag = config["conditions"]
            self.assertEqual(player["Form"], {"pluginName": "Skyrim.esm", "formID": "000014"})
            self.assertEqual(flag["Value A"]["form"], {"pluginName": "ThrowObject.esm", "formID": "000800"})
            self.assertEqual(flag["Value B"]["value"], 1)
            for subdir in (Path(), Path("female"), Path("male")):
                for slot, clip in carry_slots():
                    self.assertEqual((carry / subdir / slot).read_bytes(),
                                     (ROOT / f"exported/ThrowObject_{clip}.hkx").read_bytes())
            third_party = carry.parent.parent / "Test Replacer" / "Standing"
            third_party.mkdir(parents=True)
            competing = third_party / "MT_Idle.HKX"
            competing.touch()
            (third_party / "other_idle.hkx").touch()
            (third_party / "config.json").write_text(json.dumps({"priority": CARRY_PRIORITY - 1}))
            self.assertEqual(list(conflicts(data)), [])
            (third_party / "user.json").write_text(json.dumps({"priority": CARRY_PRIORITY}))
            self.assertEqual(list(conflicts(data)), [(CARRY_PRIORITY, third_party, ["mt_idle.hkx"])])
            competing.unlink()
            self.assertEqual(list(conflicts(data)), [])
            legacy = carry.parent.parent.parent / "DynamicAnimationReplacer/_CustomConditions" / str(CARRY_PRIORITY)
            legacy.mkdir(parents=True)
            (legacy / "mt_sprintforward.hkx").touch()
            self.assertEqual(list(conflicts(data)), [(CARRY_PRIORITY, legacy, ["mt_sprintforward.hkx"])])


if __name__ == "__main__":
    unittest.main()
