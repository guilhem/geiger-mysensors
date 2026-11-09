"""Keep the dashboard and entity metadata examples consistent."""

from pathlib import Path
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]


class HomeAssistantExampleTests(unittest.TestCase):
    def test_dashboard_entities_have_compatible_metadata(self):
        customization = yaml.safe_load(
            (ROOT / "examples/home-assistant-customize.yaml").read_text()
        )["homeassistant"]["customize"]
        dashboard = yaml.safe_load(
            (ROOT / "examples/radioactivity-dashboard.yaml").read_text()
        )
        for view in dashboard["views"]:
            for section in view["sections"]:
                for card in section["cards"]:
                    if "entity" in card:
                        self.assertIn(card["entity"], customization)
                    if card["type"] == "gauge":
                        self.assertLess(card["min"], card["max"])
                    if card["type"] == "statistics-graph":
                        for entity in card["entities"]:
                            # Mean/min/max statistics require a measurement sensor.
                            self.assertEqual(customization[entity]["state_class"], "measurement")


if __name__ == "__main__":
    unittest.main()
