"""The kits' recipes: one package per kit, recipes/<kit>/ (its kit module's RECIPES), one module per instrument (or
family) in it, each exposing ITEMS = [Item(...), ...]. A kit's recipes import their own kit module and the shared tools
(dsp48, parts, phrase, sampler, sends, vocals), never another kit's module or recipes.

The contract (tools/sound/item.py has the details):
- render(take) is a module-level function returning float audio at 48 kHz, (n,) or (n, 2), or an Out with its
  splices, loop points, chord tones or kick hits. Render long: the renderer trims at -90 dB and fades; it also
  high-passes at 20 Hz (Item.hp_hz) and sets one gain for the whole instrument.
- All noise comes from take.rng(...) and the room from the kit's space_rng(): no global random state, no hash().
- Take the kit's material from its kit module: RATIOS, VOICE, SPACE_SEED, ROOT, KICK_NOTE, SCALE, BPM, STEP, SWING.
- Low end: kicks, toms and booms pass the mud gate; basses pass it or the bass-body rule (qa={'mud': 'bass-body',
  'why': ...}, with a mono sine sub under any detuned stack); everything else is high-passed around 100 Hz and mono
  under ~150 Hz (dsp48.mono_below).
- Shared helpers take the kit's settings as arguments, with no defaults: pass K.BPM, qa.GROOVES[K.GROOVE], K.SWING.
- Names: CamelCase, unique in the kit, never an artist's name; vocals carry their script and rating.
"""
