"""Example recipe: one small alloy ping, so the pipeline runs end to end (render.py --kit FUT01 --set example).

Copy this file to start an instrument of your own.
"""
from tools.sound import dsp48 as D
from tools.sound.item import Item
from tools.sound.kits import fut01 as K

PING_NOTE = K.ROOT + 36      # F#5


def ping(take):
    """Struck modes on the kit's alloy ratios: harder hits are louder and keep more of the upper modes."""
    n = int(3.0 * D.SR)
    decays = [0.30 / (1 + 0.8 * k) for k in range(len(K.ALLOY))]
    amps = [take.vel ** (0.6 * k) / (1 + k) for k in range(len(K.ALLOY))]
    x = D.modal_ir(take.f0, K.ALLOY, decays, amps, n, take.rng('modes')) * D.env_exp(n, 1.0, attack=0.001)
    return 0.3 * take.vel * x


ITEMS = [
    Item('Perc', 'Ping', 'perc', ping, layers=(0.5, 1.0), rr=2, root=PING_NOTE, sets=('example',),
         about='example item: an alloy-ratio ping on F#5'),
]
