"""The plugin contract's two copies of the MIDI channel map agree: tools/sound/channels.py (MIDI v2, ADR 0019) and
engine/kit/channel_map.h, which the engine's router and the manifests' checks use. Skipped where channels.py doesn't
exist yet (P0's branch, before P1's lands)."""
import importlib.util
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
CHANNELS_PY = REPO / 'tools' / 'sound' / 'channels.py'
CHANNEL_MAP_H = REPO / 'engine' / 'kit' / 'channel_map.h'
KIT_H = REPO / 'engine' / 'kit' / 'kit.h'


def cpp_channel_map(text, kit_h):
    """The tables of channel_map.h, as channels.py spells them."""
    def block(name):
        return re.search(name + r'\s*\{([^;]*?)\};', text, re.S).group(1)

    roles = re.findall(r'"([a-z_]+)"', block(r'kChannelRoleIds'))
    numbers = [int(n) for n in re.findall(r'\d+', block(r'kRoleChannels'))]
    lanes = tuple(re.findall(r'"([a-z]+)"', re.search(r'kLaneIds\s*\{([^}]*)\}', kit_h).group(1)))
    role_lane = {}
    for role, lane in re.findall(r'case ChannelRole::(\w+): return static_cast<int> \(Lane::(\w+)\);', text):
        role_lane[role] = lane
    one_key = set(re.findall(r'role == ChannelRole::(\w+)', re.search(r'isOneKeyRole[^}]*\}', text, re.S).group(0)))
    tracks = {name: (role, lane) for name, role, lane in
              re.findall(r'\{ "([^"]+)", ChannelRole::(\w+), Lane::(\w+) \}', text)}
    constant = lambda name: int(re.search(name + r'\s*=\s*(\d+)', text).group(1))
    return {
        'CHANNELS': dict(zip(roles, numbers)),
        'EXTRA_CHANNELS': tuple(range(constant('kFirstExtraChannel'), constant('kLastExtraChannel') + 1)),
        'KEY_BASE': {'accents': constant('kKeyBaseAccents'), 'fx': constant('kKeyBaseFx'),
                     'chops': constant('kKeyBaseChops')},
        'DRUM_RANGE': (constant('kDrumKeyLo'), constant('kDrumKeyHi')),
        'ONE_KEY_ROLES': one_key,
        'ROLE_LANE': role_lane,
        'TRACKS': tracks,
        'LANES': lanes,
    }


@unittest.skipUnless(CHANNELS_PY.exists(), 'tools/sound/channels.py arrives with MIDI v2 (P1)')
class TestChannelMapCopies(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('channels_under_test', CHANNELS_PY)
        self.py = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.py)
        self.cpp = cpp_channel_map(CHANNEL_MAP_H.read_text(encoding='utf-8'), KIT_H.read_text(encoding='utf-8'))

    def test_the_channels_and_key_ranges(self):
        self.assertEqual(self.cpp['CHANNELS'], self.py.CHANNELS)
        self.assertEqual(self.cpp['EXTRA_CHANNELS'], tuple(self.py.EXTRA_CHANNELS))
        self.assertEqual(self.cpp['KEY_BASE'], self.py.KEY_BASE)
        self.assertEqual(self.cpp['DRUM_RANGE'], tuple(self.py.DRUM_RANGE))
        self.assertEqual(self.cpp['ONE_KEY_ROLES'], set(self.py.ONE_KEY_ROLES))

    def test_the_lanes(self):
        self.assertEqual(self.cpp['LANES'], tuple(self.py.LANES))
        self.assertEqual(self.cpp['ROLE_LANE'], self.py.ROLE_LANE)
        self.assertEqual(self.cpp['TRACKS'], {name: tuple(v) for name, v in self.py.TRACKS.items()})


if __name__ == '__main__':
    unittest.main()
