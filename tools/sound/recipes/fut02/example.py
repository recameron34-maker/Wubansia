"""Example recipe: one small ping on the kit's ratio set, so the pipeline runs end to end (render.py --kit FUT01 --set
example).

Copy this file to start an instrument of your own: change the kit import to your kit, and it renders as it is.
"""
from tools.sound import dsp48 as D
from tools.sound.item import Item
from tools.sound.kits import fut02 as K

PING_NOTE = K.ROOT + 36      # three octaves over the key's root: F#5 in FUT01


def ping(take):
    """Struck modes on the kit's ratio set: harder hits are louder and keep more of the upper modes."""
    n = int(3.0 * D.SR)
    decays = [0.30 / (1 + 0.8 * k) for k in range(len(K.RATIOS))]
    amps = [take.vel ** (0.6 * k) / (1 + k) for k in range(len(K.RATIOS))]
    x = D.modal_ir(take.f0, K.RATIOS, decays, amps, n, take.rng('modes')) * D.env_exp(n, 1.0, attack=0.001)
    return 0.3 * take.vel * x


ITEMS = [
    Item('Perc', 'Ping', 'perc', ping, layers=(0.5, 1.0), rr=2, root=PING_NOTE, sets=('example',),
         about="example item: a ping on the kit's ratio set, three octaves over its root"),
]
