# Phrases

The Phrases drawer's library (ADR 0031): 64 eight-bar phrases per kit, written over the kit's own chord loop by
`core/gen/phrase.{h,cpp}`. Two files per kit, both embedded in the plugin:

- `<kit>/<KIT>_phrases.json`, **the spec**: the chord loop (names, degrees and the app's voicings), the phrase register,
  the swing, the hook's material (the style's hook block, copied), the eight kinds (quota, form weights, track and
  channel, cells, centre, span, a kind's own `register`...), the 24 + 24 name words, `rename` and `cut`. The loader is
  strict: every unknown key is refused, naming the file, the JSON path and the line.
- `<kit>/<KIT>_phrase_library.json`, **the library**: the rows `groovecore phrases --library` chose, each with its id
  (`hook-5`: kind and seed), name, form, notes, keys, tags and its MIDI file's SHA-256. It is generated, like the
  generator snapshots: change it only in its own commit, and only by regenerating it.

## Regenerate and check

```
groovecore phrases --kit FUT01 --library   # choose the library again from the spec and write the file
groovecore phrases --kit FUT01 --check     # exit 1, writing nothing, if the file isn't a fresh choice
groovecore phrases --kit FUT01 --out build/phrases   # the 64 MIDI files, named as their drags are
```

`core_tests "[phrase]"` regenerates every row to its hash, and the plugin refuses a library chosen for another spec.

## Cut and rename

- **Cut** a phrase you don't want: list its seed under its kind, `"cut": { "hook": [5] }`. The rest of the library stays
  as it is (ids, names and bytes); the kind is refilled from the seeds after its last one. `"cut": { "stabs": "all" }`
  removes a kind and doesn't refill it. Never lower a `quota` to cut: that moves later kinds' phrases. A kept phrase
  that is cut stays under Kept for whoever starred it.
- **Rename** one: `"rename": { "hook-5": "Night Shift" }`. Names are original: never an artist, a song or a record.

Then run `--library` and commit the spec and the library together, the library's change alone in its own commit if
the generator moved it.
