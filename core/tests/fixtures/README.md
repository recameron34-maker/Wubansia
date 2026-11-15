# Generator test fixtures

Content the generator tests load besides `content/`, with `gen::loadBundle (<fixture folder>, <kit>)`.
Each folder holds `kits/`, `styles/` and `feels/` like `content/`. They are frozen: a fixture changes only
with the tests that read it, never to follow `content/`.

- `nohook/`: FUT01 (`garage2step`) and FUT02 (`sciclub`, its arp lane of kind `arp`, the kit's arp on range
  62-86) as they were before the hook (S1b2b), byte for byte the files of commit `baab997`, with no lead
  lane and no hook block.
  - `arp.cpp` keeps its tests on FUT02 here once FUT02's arp carries the hook.
  - `test_gen_hook.cpp` compares the shipped FUT02 with it (the hook lane is additive) and pins its files'
    hashes (a style without a hook block generates the same bytes).
  - `test_gen_form.cpp`'s grammar tests put their own forms into FUT01's style here: those forms name no
    lead lane, which a style with a hook block refuses.
- `hook/`: FUT01 with the lead lane and the hook block of `docs/plans/S1b2b-plan.md` ("Full example:
  content/styles/garage2step.json"), its kit lane and its feel lane, on the `nohook/` files. The hook's
  loader, material and line plan are tested on it before the shipped styles carry a hook block.
