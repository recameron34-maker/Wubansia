"""FUT01's recipes: one module per instrument (or family), each exposing ITEMS = [Item(...), ...].

The contract (tools/sound/item.py has the details):
- render(take) is a module-level function returning float audio at 48 kHz, (n,) or (n, 2), or an Out with its
  splices, loop points or chord tones. Render long: the renderer trims at -90 dB and fades; it also high-passes at
  20 Hz (Item.hp_hz) and sets one gain for the whole instrument.
- All noise comes from take.rng(...) and the room from kits.fut01.space_rng(): no global random state, no hash().
- Take the kit's material from kits.fut01: ALLOY, VOICE, SPACE_SEED, ROOT, BPM, STEP.
- Low end: kicks and basses pass the mud gate; everything else is high-passed around 100 Hz and mono under ~150 Hz
  (dsp48.mono_below).
- Names: CamelCase, unique in the kit, never an artist's name; vocals carry their script and rating.
"""
