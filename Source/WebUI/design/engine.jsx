// Curve engine — drawing, playback, quantization
// React hook that owns lane state, playhead, and scale quantization.

function useDrawnQurveEngine(initial = {}) {
  const [lanes, setLanes] = React.useState(() => [
    {
      id: 0,
      color: window.LANES[0].color,
      name: window.LANES[0].name,
      curve: makeSineCurve(256, 0.5, 0.35, 1.2, 0),
      enabled: true,
      target: 'CC',      // 'CC' | 'Aftertouch' | 'PitchBend' | 'Note'
      targetDetail: 74,  // CC number or velocity
      channel: 1,
      smooth: 0.08,
      rangeMin: 0.05,
      rangeMax: 0.95,
      scaleId: 'major',
      scaleRoot: 0,
      scaleMask: 0b101011010101,
      velocity: 100,
      // Per-lane quantization — mirrors C++ APVTS xQuantize/yQuantize/xDivisions/yDivisions
      quantizeX: false,
      quantizeY: false,
      xDivisions: 4,
      yDivisions: 4,
    },
    {
      id: 1,
      color: window.LANES[1].color,
      name: window.LANES[1].name,
      curve: makeSineCurve(256, 0.5, 0.25, 2.4, Math.PI / 3),
      enabled: true,
      target: 'Note',
      targetDetail: 100,
      channel: 2,
      smooth: 0.0,
      rangeMin: 0.2,
      rangeMax: 0.85,
      scaleId: 'pentMin',
      scaleRoot: 9, // A
      scaleMask: 0b100101010010,
      velocity: 96,
      quantizeX: false,
      quantizeY: false,
      xDivisions: 4,
      yDivisions: 4,
    },
    {
      id: 2,
      color: window.LANES[2].color,
      name: window.LANES[2].name,
      curve: null,
      enabled: false,
      target: 'PitchBend',
      targetDetail: 0,
      channel: 1,
      smooth: 0.15,
      rangeMin: 0.35,
      rangeMax: 0.65,
      scaleId: 'chromatic',
      scaleRoot: 0,
      scaleMask: 0b111111111111,
      velocity: 100,
      quantizeX: false,
      quantizeY: false,
      xDivisions: 4,
      yDivisions: 4,
    },
  ]);
  const [focus, setFocus] = React.useState(0);   // focused lane id
  const [playing, setPlaying] = React.useState(true);
  const [direction, setDirection] = React.useState('fwd'); // 'fwd'|'rev'|'pp'
  const [syncOn, setSyncOn] = React.useState(true);
  const [speed, setSpeed] = React.useState(1);
  const [beats, setBeats] = React.useState(4);
  const [phase, setPhase] = React.useState(0);   // 0..1
  const [ppForward, setPpForward] = React.useState(true);
  const [mode, setMode] = React.useState(initial.mode || 'standard');
  // Note: quantizeX/Y are PER-LANE (on each lane object), not global, so the
  // C++ engine can quantize lanes independently.  The UI surfaces them via the
  // currently-focused lane in juce-ipad.jsx.

  // playhead animator
  React.useEffect(() => {
    if (!playing) return;
    let raf, last = performance.now();
    const loop = (now) => {
      const dt = (now - last) / 1000;
      last = now;
      const cycleSec = syncOn ? (beats * 60 / 100) : (2 / speed); // pretend bpm=100
      setPhase(p => {
        let inc = dt / cycleSec;
        if (direction === 'rev') inc = -inc;
        if (direction === 'pp') inc = ppForward ? inc : -inc;
        let next = p + inc;
        if (direction === 'pp') {
          if (next >= 1) { next = 2 - next; setPpForward(false); }
          if (next <= 0) { next = -next; setPpForward(true); }
        } else {
          next = ((next % 1) + 1) % 1;
        }
        return next;
      });
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, [playing, direction, syncOn, beats, speed, ppForward]);

  const updateLane = (id, patch) => setLanes(ls => ls.map(l => l.id === id ? { ...l, ...patch } : l));
  const setCurve = (id, curve) => updateLane(id, { curve });
  const clearLane = (id) => updateLane(id, { curve: null });
  const clearAll = () => setLanes(ls => ls.map(l => ({ ...l, curve: null })));

  return {
    lanes, setLanes, focus, setFocus, updateLane, setCurve, clearLane, clearAll,
    playing, setPlaying, direction, setDirection,
    syncOn, setSyncOn, speed, setSpeed, beats, setBeats, phase,
    mode, setMode,
  };
}

// Sample a curve value at phase [0..1]; curve is Float32Array of 256 values in [0..1]
function sampleCurve(curve, phase) {
  if (!curve) return 0.5;
  const n = curve.length;
  const idx = phase * (n - 1);
  const i = Math.floor(idx);
  const f = idx - i;
  const a = curve[i];
  const b = curve[Math.min(n - 1, i + 1)];
  return a + (b - a) * f;
}

// Sample a lane's curve with the lane's quantization applied — mirrors the
// C++ engine's processLane() so the JS display matches what the audio path
// actually emits.  Use this for any UI that wants to show "current playback
// value" (TypoReadout, lane-panel readouts, future staircase overlay).
//
//   xQuantize: snap phase to ⌊phase / tickWidth⌋ × tickWidth (S&H in time)
//   yQuantize: snap output value to nearest 1/yDivisions step
//
// Both are interval-based 0..1 — same convention as the engine.
function sampleLaneQuantized(lane, phase) {
  if (!lane?.curve) return 0.5;
  let p = phase;
  if (lane.quantizeX && lane.xDivisions >= 2) {
    const tickWidth = 1 / lane.xDivisions;
    p = Math.floor(p / tickWidth) * tickWidth;
  }
  let v = sampleCurve(lane.curve, p);
  if (lane.quantizeY && lane.yDivisions >= 2) {
    const step = 1 / lane.yDivisions;
    v = Math.round(v / step) * step;
  }
  return v;
}

// Apply lane's range + scale quantization. Returns { value, semitone }
function applyLane(lane, raw) {
  const { rangeMin, rangeMax, target, scaleMask, scaleRoot } = lane;
  const ranged = rangeMin + raw * (rangeMax - rangeMin);
  if (target === 'Note') {
    // map ranged [0..1] to semitone range across 2 octaves starting at C4 (60)
    const semiRange = 24;
    const base = 60;
    let semi = Math.round(ranged * semiRange);
    // snap to nearest active pitch class
    const snapped = snapSemitone(semi, scaleMask, scaleRoot);
    return { value: ranged, semitone: base + snapped };
  }
  return { value: ranged, semitone: null };
}

function snapSemitone(semi, mask, root) {
  // find nearest active pc (relative to root)
  const rel = ((semi - root) % 12 + 12) % 12;
  if (pcActive(mask, rel)) return semi;
  for (let d = 1; d < 12; d++) {
    const up   = ((rel + d) % 12 + 12) % 12;
    const down = ((rel - d) % 12 + 12) % 12;
    if (pcActive(mask, up))   return semi + d;
    if (pcActive(mask, down)) return semi - d;
  }
  return semi;
}

function pcActive(mask, pc) { return (mask >> (11 - pc)) & 1; }

// generators
function makeSineCurve(n, center, amp, cycles, phase = 0) {
  const arr = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / (n - 1);
    arr[i] = Math.max(0, Math.min(1, center + amp * Math.sin(2 * Math.PI * cycles * t + phase)));
  }
  return arr;
}

// smoothed freehand recording helper
function smoothCurvePoints(points, n = 256) {
  if (!points || points.length < 2) return null;
  // sort by x and dedupe
  const sorted = [...points].sort((a,b) => a.x - b.x);
  const arr = new Float32Array(n);
  let j = 0;
  for (let i = 0; i < n; i++) {
    const t = i / (n - 1);
    while (j < sorted.length - 2 && sorted[j+1].x < t) j++;
    const a = sorted[j], b = sorted[j+1] || a;
    const span = Math.max(0.0001, b.x - a.x);
    const f = Math.max(0, Math.min(1, (t - a.x) / span));
    arr[i] = Math.max(0, Math.min(1, a.y + (b.y - a.y) * f));
  }
  // one-pole LP
  let prev = arr[0];
  const k = 0.3;
  for (let i = 0; i < n; i++) {
    prev = prev + k * (arr[i] - prev);
    arr[i] = prev;
  }
  return arr;
}

Object.assign(window, {
  useDrawnQurveEngine, sampleCurve, sampleLaneQuantized, applyLane, snapSemitone,
  makeSineCurve, smoothCurvePoints,
});
