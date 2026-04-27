// main.jsx — JUCE WebView entry point for DrawnQurve
//
// esbuild injects react-globals.js first, so React/ReactDOM are already globals
// by the time any import runs.  Design files use React.useState etc. as globals —
// that's intentional; do not add React imports to the design files.

// ── Design system globals ─────────────────────────────────────────────────────
import './design/tokens.jsx';       // window.PAPER, LANES, SCALES, PITCH_*
import './design/engine.jsx';       // window.useDrawnQurveEngine (demo), sampleCurve …
import './design/ui-primitives.jsx';
import './design/curve-canvas.jsx';
import './design/scale-editor.jsx';

// ── JUCE bridge ───────────────────────────────────────────────────────────────
import { initJuceBridge, sendCurve, sendParam, sendFocus,
         sendPlaying, sendDirection, sendEnabled, sendGlobalActual,
         globalFieldForParamId,
         sendClearLane, sendAddLane, sendRemoveLane } from './juce-bridge.js';

// ── Patch useDrawnQurveEngine ─────────────────────────────────────────────────
// Must run before juce-ipad.jsx is imported (esbuild preserves import order in
// the output bundle, so the IIFE below executes before the juce-ipad.jsx content).
//
// Strategy: wrap the demo hook so all mutations also notify JUCE, and overlay
// JUCE-driven state (phase, lane params) on top of the demo animation state.
// Falls back gracefully to pure demo mode when __JUCE__ is not present.

(function patchEngine() {
  const demoEngine = window.useDrawnQurveEngine;

  window.useDrawnQurveEngine = function useDrawnQurveEngine(initial = {}) {
    const demo = demoEngine(initial);

    // Phase driven by JUCE (overrides the demo RAF loop *while it's actively
    // emitting* — i.e. inside a host that's running the audio graph).  In the
    // standalone the JUCE timer often emits a single phase=0 at startup and
    // then nothing more (the engine's phase doesn't actually advance there).
    // If we trusted that one-shot value forever, the playhead would freeze at
    // 0; so we tag every JUCE phase event with a wall-clock timestamp and only
    // honour it for ~200 ms — long enough to bridge between consecutive 30 Hz
    // ticks, short enough to fall back to the demo RAF when JUCE goes quiet.
    const [jucePhase, setJucePhase] = React.useState(null);
    const jucePhaseTimeRef = React.useRef(0);
    const isJuceDriving =
      jucePhase != null && (performance.now() - jucePhaseTimeRef.current) < 200;
    const effectivePhase = isJuceDriving ? jucePhase : demo.phase;

    // Wire up the bridge on first mount
    React.useEffect(() => {
      initJuceBridge((action) => {
        switch (action.type) {
          case 'setPhase':
            jucePhaseTimeRef.current = performance.now();
            setJucePhase(action.phase);
            break;
          case 'setLanes':     demo.setLanes(action.lanes);         break;
          case 'setPlaying':   demo.setPlaying(action.playing);     break;
          case 'setDirection': demo.setDirection(action.direction); break;
          case 'setFocus':     demo.setFocus(action.focus);         break;
          case 'setSyncOn':    demo.setSyncOn(action.syncOn);       break;
          case 'setBeats':     demo.setBeats(action.beats);         break;
          case 'setSpeed':     demo.setSpeed(action.speed);         break;
          case 'setActiveLaneCount':
            // Truncate lanes to the active count
            demo.setLanes(prev => prev.slice(0, action.count));
            break;
          case 'curveData':
            demo.setLanes(prev => prev.map(l =>
              l.id === action.lane ? { ...l, curve: action.curve } : l));
            break;
          case 'paramChange': {
            const { id, value } = action;

            // Global APVTS params (no l<n>_ prefix) — apply to every lane so
            // any UI surface that reads lane.scaleRoot/scaleMask sees the
            // shared value.  Mapping is owned by GLOBAL_PARAM_MAP in the bridge.
            const globalMap = globalFieldForParamId(id);
            if (globalMap) {
              const v = globalMap.rawToReact(value);
              demo.setLanes(prev => prev.map(l => ({ ...l, [globalMap.field]: v })));
              break;
            }

            // Per-lane params — match l<n>_ prefix and dispatch into that lane.
            demo.setLanes(prev => {
              const next = [...prev];
              for (let L = 0; L < next.length; L++) {
                const prefix = `l${L}_`;
                if (!id.startsWith(prefix)) continue;
                const suffix = id.slice(prefix.length);
                const patch = {};
                // value is the ACTUAL parameter value (not 0-1 normalised):
                //   AudioParameterChoice → index   AudioParameterInt → integer
                if (suffix === 'enabled')      patch.enabled     = value > 0.5;
                if (suffix === 'msgType')      patch.target      = ['CC','Aftertouch','PitchBend','Note'][Math.round(value)] ?? 'CC';
                if (suffix === 'ccNumber')     patch.targetDetail = Math.round(value);        // 0-127
                if (suffix === 'midiChannel')  patch.channel     = Math.round(value);         // 1-16
                if (suffix === 'smoothing')    patch.smooth      = value;                     // 0-1
                if (suffix === 'minOutput')    patch.rangeMin    = value;                     // 0-1
                if (suffix === 'maxOutput')    patch.rangeMax    = value;                     // 0-1
                if (suffix === 'noteVelocity') patch.velocity    = Math.round(value);         // 1-127
                if (suffix === 'xQuantize')    patch.quantizeX   = value > 0.5;
                if (suffix === 'yQuantize')    patch.quantizeY   = value > 0.5;
                if (suffix === 'xDivisions')   patch.xDivisions  = Math.round(value);     // 2-32
                if (suffix === 'yDivisions')   patch.yDivisions  = Math.round(value);     // 2-24
                if (Object.keys(patch).length) {
                  next[L] = { ...next[L], ...patch };
                  return next;
                }
              }
              return prev;
            });
            break;
          }
        }
      });
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    // ── Wrapped mutators that also notify JUCE ──────────────────────────────
    const setCurve = React.useCallback((id, curve) => {
      demo.setCurve(id, curve);
      sendCurve(id, curve);
    }, [demo.setCurve]);

    const setFocus = React.useCallback((id) => {
      demo.setFocus(id);
      sendFocus(id);
    }, [demo.setFocus]);

    const setPlaying = React.useCallback((p) => {
      const next = typeof p === 'function' ? p(demo.playing) : p;
      demo.setPlaying(next);
      sendPlaying(next);
    }, [demo.setPlaying, demo.playing]);

    const setDirection = React.useCallback((d) => {
      demo.setDirection(d);
      sendDirection(d);
      // Mirror the UI choice ('fwd'/'rev'/'pp') into the global APVTS param.
      const idx = Math.max (0, ['fwd', 'rev', 'pp'].indexOf(d));
      sendGlobalActual('playbackDirection', idx);
    }, [demo.setDirection]);

    // ── Global transport setters wired to APVTS ──────────────────────────────
    // The demo RAF used to drive the playhead from `beats`/`speed`; now JUCE
    // does, and we need these sliders to actually move the engine.  All four
    // dispatch through sendGlobalActual which lets C++ apply the parameter's
    // own NormalisableRange (so e.g. playbackSpeed's skew=0.5 is honoured).
    const setSyncOn = React.useCallback((v) => {
      demo.setSyncOn(v);
      sendGlobalActual('syncEnabled', v ? 1.0 : 0.0);
    }, [demo.setSyncOn]);
    const setBeats = React.useCallback((v) => {
      const next = typeof v === 'function' ? v(demo.beats) : v;
      demo.setBeats(next);
      sendGlobalActual('syncBeats', next);
    }, [demo.setBeats, demo.beats]);
    const setSpeed = React.useCallback((v) => {
      const next = typeof v === 'function' ? v(demo.speed) : v;
      demo.setSpeed(next);
      sendGlobalActual('playbackSpeed', next);
    }, [demo.setSpeed, demo.speed]);

    const updateLane = React.useCallback((id, patch) => {
      // Diagnostic — log only quantize-related patches so the live MIDI test
      // can confirm whether clicks reach the bridge during playback.
      const keys = Object.keys(patch);
      if (keys.some(k => k === 'quantizeX' || k === 'quantizeY'
                       || k === 'xDivisions' || k === 'yDivisions')) {
        console.log('[click→updateLane]',
          'lane=' + id,
          'patch=' + JSON.stringify(patch),
          'playing=' + (demo.playing ? '1' : '0'));
      }
      demo.updateLane(id, patch);
      if ('enabled' in patch) sendEnabled(id, patch.enabled);
      Object.entries(patch).forEach(([field, value]) => sendParam(id, field, value));
    }, [demo.updateLane, demo.playing]);

    const clearLane = React.useCallback((id) => {
      demo.clearLane(id);
      sendClearLane(id);
    }, [demo.clearLane]);

    const clearAll = React.useCallback(() => {
      demo.clearAll();
      demo.lanes.forEach(l => sendClearLane(l.id));
    }, [demo.clearAll, demo.lanes]);

    return {
      ...demo,
      phase: effectivePhase,
      setCurve,
      setFocus,
      setPlaying,
      setDirection,
      setSyncOn,
      setBeats,
      setSpeed,
      updateLane,
      clearLane,
      clearAll,
    };
  };
})();

// ── iPad layout (must be imported after patchEngine runs) ─────────────────────
import './design/juce-ipad.jsx';   // Object.assign(window, { JuceIPadStudio, … })

// ── Root component ────────────────────────────────────────────────────────────
function App() {
  const [size, setSize] = React.useState({
    w: window.innerWidth,
    h: window.innerHeight,
  });

  React.useEffect(() => {
    const update = () => setSize({ w: window.innerWidth, h: window.innerHeight });
    window.addEventListener('resize', update);
    return () => window.removeEventListener('resize', update);
  }, []);

  const Studio = window.JuceIPadStudio;
  return Studio
    ? React.createElement(Studio, { width: size.w, height: size.h })
    : React.createElement('div', { style: { padding: 24, fontFamily: 'sans-serif' } }, 'Loading…');
}

ReactDOM.createRoot(document.getElementById('root')).render(
  React.createElement(App)
);
