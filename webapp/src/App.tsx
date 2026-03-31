import { useState, useRef, useEffect, useCallback } from 'react'
import { GestureEngine } from './engine/gestureEngine'
import {
  type LaneParams,
  type LaneSnapshot,
  defaultLaneParams,
  PlaybackDirection,
} from './engine/types'
import { WebMidiManager, type MidiPort } from './midi/webMidi'
import { CurveDisplay } from './components/CurveDisplay'
import { LaneControls } from './components/LaneControls'
import { Toolbar } from './components/Toolbar'

const NUM_LANES = 3
const DEFAULT_CC_NUMBERS = [74, 1, 11]

function makeDefaultLanes(): LaneParams[] {
  return DEFAULT_CC_NUMBERS.map((cc, i) => ({
    ...defaultLaneParams(cc),
    midiChannel: i, // Lanes default to channels 1, 2, 3
  }))
}

export function App() {
  const [theme, setTheme] = useState<'light' | 'dark'>('dark')
  const [focusedLane, setFocusedLane] = useState(0)
  const [laneParams, setLaneParams] = useState<LaneParams[]>(makeDefaultLanes)
  const [snapshots, setSnapshots] = useState<(LaneSnapshot | null)[]>(new Array(NUM_LANES).fill(null))
  const [speedRatio, setSpeedRatio] = useState(1)
  const [direction, setDirection] = useState<PlaybackDirection>(PlaybackDirection.Forward)
  const [midiPorts, setMidiPorts] = useState<MidiPort[]>([])
  const [selectedMidiPort, setSelectedMidiPort] = useState<string | null>(null)
  const [midiSupported] = useState(() => typeof navigator !== 'undefined' && 'requestMIDIAccess' in navigator)

  // Refs to avoid stale closures in the tick loop
  const engineRef = useRef<GestureEngine>(new GestureEngine())
  const midiManagerRef = useRef(new WebMidiManager())
  const speedRef = useRef(speedRatio)
  const directionRef = useRef(direction)
  const selectedPortRef = useRef(selectedMidiPort)
  speedRef.current = speedRatio
  directionRef.current = direction
  selectedPortRef.current = selectedMidiPort

  // Apply theme to document
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme)
  }, [theme])

  // Sync scale configs to engine when lane params change
  useEffect(() => {
    for (let i = 0; i < NUM_LANES; i++) {
      engineRef.current.setScaleConfig(i, laneParams[i].scaleConfig)
      engineRef.current.setLaneEnabled(i, laneParams[i].enabled)
    }
  }, [laneParams])

  // Engine tick loop — runs at ~100 Hz (10ms) for MIDI timing
  useEffect(() => {
    let lastTime = performance.now()
    const id = setInterval(() => {
      const now = performance.now()
      const delta = Math.min((now - lastTime) / 1000, 0.05) // cap at 50ms
      lastTime = now

      engineRef.current.tick(
        delta,
        speedRef.current,
        directionRef.current,
        (status, d1, d2) => {
          midiManagerRef.current.send(status, d1, d2)
        },
      )
    }, 10)
    return () => clearInterval(id)
  }, []) // intentionally no deps — uses refs

  // MIDI manager setup
  const handleRequestMidi = useCallback(async () => {
    try {
      const ports = await midiManagerRef.current.requestAccess()
      setMidiPorts(ports)
      midiManagerRef.current.onPortsChange(setMidiPorts)
      if (ports.length > 0 && !selectedMidiPort) {
        setSelectedMidiPort(ports[0].id)
        midiManagerRef.current.setSelectedOutput(ports[0].id)
      }
    } catch {
      alert('Could not access Web MIDI. Try Chrome or Edge on desktop.')
    }
  }, [selectedMidiPort])

  const handleSelectMidiPort = useCallback((id: string | null) => {
    setSelectedMidiPort(id)
    midiManagerRef.current.setSelectedOutput(id)
  }, [])

  // Lane parameter updates — re-bake snapshots when output params change
  const handleUpdateParams = useCallback(
    (lane: number, partial: Partial<LaneParams>) => {
      setLaneParams(prev => {
        const next = prev.map((p, i) => (i === lane ? { ...p, ...partial } : p))

        // If the snapshot exists and output params changed, update it in the engine
        setSnapshots(prevSnaps => {
          const snap = prevSnaps[lane]
          if (snap?.valid) {
            const updated: LaneSnapshot = {
              ...snap,
              ccNumber: next[lane].ccNumber,
              midiChannel: next[lane].midiChannel,
              minOut: next[lane].minOut,
              maxOut: next[lane].maxOut,
              smoothing: next[lane].smoothing,
              messageType: next[lane].messageType,
              noteVelocity: next[lane].noteVelocity,
            }
            engineRef.current.setSnapshot(lane, updated)
            const nextSnaps = [...prevSnaps]
            nextSnaps[lane] = updated
            return nextSnaps
          }
          return prevSnaps
        })

        return next
      })
    },
    [],
  )

  const handleCurveDrawn = useCallback((lane: number, snapshot: LaneSnapshot) => {
    setSnapshots(prev => {
      const next = [...prev]
      next[lane] = snapshot
      return next
    })
  }, [])

  const handleClearLane = useCallback((lane: number) => {
    engineRef.current.clearSnapshot(lane)
    setSnapshots(prev => {
      const next = [...prev]
      next[lane] = null
      return next
    })
  }, [])

  const handleClearAll = useCallback(() => {
    engineRef.current.clearAllSnapshots()
    setSnapshots(new Array(NUM_LANES).fill(null))
  }, [])

  const handlePanic = useCallback(() => {
    engineRef.current.setPlaying(false)
    midiManagerRef.current.panic()
    engineRef.current.reset()
    engineRef.current.setPlaying(true)
  }, [])

  const dark = theme === 'dark'
  const hasSnapshot = snapshots.map(s => !!(s?.valid))

  return (
    <div
      style={{
        height: '100dvh',
        display: 'flex',
        flexDirection: 'column',
        background: dark ? '#1a1a1a' : '#f0f0f0',
        color: dark ? '#e0e0e0' : '#1a1a1a',
        fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
        overflow: 'hidden',
      }}
    >
      {/* Toolbar */}
      <Toolbar
        speedRatio={speedRatio}
        direction={direction}
        theme={theme}
        midiPorts={midiPorts}
        selectedMidiPort={selectedMidiPort}
        midiSupported={midiSupported}
        onSpeedChange={speed => {
          setSpeedRatio(speed)
          speedRef.current = speed
        }}
        onDirectionChange={dir => {
          setDirection(dir)
          directionRef.current = dir
        }}
        onThemeToggle={() => setTheme(t => (t === 'dark' ? 'light' : 'dark'))}
        onClearAll={handleClearAll}
        onPanic={handlePanic}
        onRequestMidi={handleRequestMidi}
        onSelectMidiPort={handleSelectMidiPort}
      />

      {/* Main content */}
      <div
        style={{
          flex: 1,
          display: 'flex',
          minHeight: 0,
          gap: 0,
        }}
      >
        {/* Canvas area */}
        <div
          style={{
            flex: 1,
            padding: 16,
            minWidth: 0,
            display: 'flex',
            flexDirection: 'column',
            gap: 8,
          }}
        >
          {/* Hint text when no curves are drawn */}
          {hasSnapshot.every(h => !h) && (
            <div
              style={{
                position: 'absolute',
                top: '50%',
                left: '35%',
                transform: 'translate(-50%, -50%)',
                textAlign: 'center',
                pointerEvents: 'none',
                zIndex: 1,
              }}
            >
              <div style={{ fontSize: 32, opacity: 0.15, marginBottom: 8 }}>✏</div>
              <div style={{ fontSize: 14, opacity: 0.25, lineHeight: 1.6 }}>
                Draw a curve to start looping
              </div>
            </div>
          )}
          <CurveDisplay
            snapshots={snapshots}
            laneParams={laneParams}
            focusedLane={focusedLane}
            theme={theme}
            engineRef={engineRef}
            onCurveDrawn={handleCurveDrawn}
          />
        </div>

        {/* Right panel */}
        <div
          style={{
            width: 300,
            flexShrink: 0,
            borderLeft: `1px solid ${dark ? '#2a2a2a' : '#ddd'}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}
        >
          <LaneControls
            laneParams={laneParams}
            focusedLane={focusedLane}
            hasSnapshot={hasSnapshot}
            theme={theme}
            onFocusLane={lane => {
              setFocusedLane(lane)
              engineRef.current.setLaneEnabled(lane, laneParams[lane].enabled)
            }}
            onUpdateParams={handleUpdateParams}
            onClearLane={handleClearLane}
          />
        </div>
      </div>

      {/* Status bar */}
      <div
        style={{
          padding: '4px 16px',
          borderTop: `1px solid ${dark ? '#2a2a2a' : '#e0e0e0'}`,
          fontSize: 11,
          color: dark ? '#555' : '#aaa',
          display: 'flex',
          gap: 16,
          flexShrink: 0,
        }}
      >
        <span>Draw on the canvas to capture a curve → it loops as MIDI</span>
        <span style={{ marginLeft: 'auto' }}>
          {hasSnapshot.filter(Boolean).length} / {NUM_LANES} lanes active
        </span>
      </div>
    </div>
  )
}
