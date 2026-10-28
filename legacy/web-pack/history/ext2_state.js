    /* ---- ext-spec kits: vocal guide spans, mutes, cuts, pitch bends, lyrics (all empty for the other kits) ---- */
    const BEND = {}, META = {};
    EXT_TRACKS.forEach(t => { BEND[t.key] = []; META[t.key] = []; });
    const vocalBlocks = [], vocalSpans = [], leadOff = [], cuts = [], vocalCues = [];
    const SH = (kit.ext && kit.ext.shouts) || [];
    let pendingShout = null, shoutIdx = 0, lastShoutT = -1e9;
    const inSpans = (arr, t) => { for (let i = 0; i < arr.length; i++) if (t >= arr[i][0] && t < arr[i][1]) return true; return false; };
    const muted = (trk, t) => (trk === 'lead' && (inSpans(leadOff, t) || inSpans(vocalBlocks, t))) || (trk === 'vox' && inSpans(vocalSpans, t));
    const NOLIFT = new Set(['breaks', 'shouts', 'sidechain']);
