import { useRef, useEffect, useCallback } from 'react'
import { CaptureSession } from '../engine/captureSession'
import { type LaneSnapshot, type LaneParams, MessageType } from '../engine/types'
import { type GestureEngine } from '../engine/gestureEngine'
import { NOTE_NAMES } from '../engine/scaleData'

const LANE_COLORS_DARK = ['#4a90e2', '#e8a838', '#5cb85c']
const LANE_COLORS_LIGHT = ['#1a60c8', '#c87010', '#228b22']
const GRID_COLOR_DARK = 'rgba(255,255,255,0.07)'
const GRID_COLOR_LIGHT = 'rgba(0,0,0,0.08)'
const QUANT_COLOR_DARK = 'rgba(255,255,255,0.20)'
const QUANT_COLOR_LIGHT = 'rgba(0,0,0,0.16)'

interface CurveDisplayProps {
  snapshots: (LaneSnapshot | null)[]
  laneParams: LaneParams[]
  focusedLane: number
  theme: 'light' | 'dark'
  engineRef: React.RefObject<GestureEngine>
  onCurveDrawn: (lane: number, snapshot: LaneSnapshot) => void
  onUpdateParams: (lane: number, partial: Partial<LaneParams>) => void
}

// Converts a MIDI note number to a display string e.g. "C4", "F#3"
function midiNoteName(note: number): string {
  const oct = Math.floor(note / 12) - 1
  return NOTE_NAMES[note % 12] + oct
}

export function CurveDisplay({
  snapshots,
  laneParams,
  focusedLane,
  theme,
  engineRef,
  onCurveDrawn,
  onUpdateParams,
}: CurveDisplayProps) {
  const dark = theme === 'dark'
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const captureRef = useRef(new CaptureSession())
  const isDrawingRef = useRef(false)
  const drawingPointsRef = useRef<{ x: number; y: number }[]>([])
  const rafRef = useRef<number>(0)

  // Keep refs to avoid stale closures in rAF
  const snapshotsRef = useRef(snapshots)
  const focusedLaneRef = useRef(focusedLane)
  const themeRef = useRef(theme)
  const laneParamsRef = useRef(laneParams)
  snapshotsRef.current = snapshots
  focusedLaneRef.current = focusedLane
  themeRef.current = theme
  laneParamsRef.current = laneParams

  const params = laneParams[focusedLane]
  const laneColors = dark ? LANE_COLORS_DARK : LANE_COLORS_LIGHT
  const color = laneColors[focusedLane]

  const update = useCallback(
    (partial: Partial<LaneParams>) => onUpdateParams(focusedLane, partial),
    [onUpdateParams, focusedLane],
  )

  const draw = useCallback(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    const dpr = window.devicePixelRatio || 1
    const W = canvas.width / dpr
    const H = canvas.height / dpr
    const dark = themeRef.current === 'dark'
    const laneColors = dark ? LANE_COLORS_DARK : LANE_COLORS_LIGHT
    const gridColor = dark ? GRID_COLOR_DARK : GRID_COLOR_LIGHT
    const quantColor = dark ? QUANT_COLOR_DARK : QUANT_COLOR_LIGHT
    const p = laneParamsRef.current[focusedLaneRef.current]
    const gx = p.xDivisions
    const gy = p.yDivisions
    const xQ = p.xQuantize
    const yQ = p.yQuantize

    // Background
    ctx.fillStyle = dark ? '#111' : '#f8f8f8'
    ctx.fillRect(0, 0, W, H)

    // Grid
    ctx.lineWidth = 1
    for (let i = 1; i < gx; i++) {
      const x = (i / gx) * W
      ctx.strokeStyle = xQ ? quantColor : gridColor
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke()
    }
    for (let i = 1; i < gy; i++) {
      const y = (i / gy) * H
      ctx.strokeStyle = yQ ? quantColor : gridColor
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke()
    }

    // Border
    ctx.strokeStyle = dark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.15)'
    ctx.lineWidth = 1
    ctx.strokeRect(0.5, 0.5, W - 1, H - 1)

    // Draw stored curves
    const snaps = snapshotsRef.current
    for (let lane = 0; lane < 3; lane++) {
      const snap = snaps[lane]
      if (!snap?.valid) continue
      const focused = lane === focusedLaneRef.current
      ctx.save()
      ctx.globalAlpha = focused ? 1 : 0.3
      ctx.strokeStyle = laneColors[lane]
      ctx.lineWidth = focused ? 2.5 : 1.5
      ctx.lineJoin = 'round'
      ctx.beginPath()
      for (let i = 0; i < 256; i++) {
        const x = (i / 255) * W
        const y = (1 - snap.table[i]) * H
        if (i === 0) ctx.moveTo(x, y)
        else ctx.lineTo(x, y)
      }
      ctx.stroke()
      ctx.restore()
    }

    // Draw in-progress curve
    if (isDrawingRef.current && drawingPointsRef.current.length >= 2) {
      ctx.save()
      ctx.strokeStyle = laneColors[focusedLaneRef.current]
      ctx.lineWidth = 2.5
      ctx.globalAlpha = 0.7
      ctx.lineJoin = 'round'
      ctx.beginPath()
      const pts = drawingPointsRef.current
      for (let i = 0; i < pts.length; i++) {
        const x = pts[i].x * W
        const y = pts[i].y * H
        if (i === 0) ctx.moveTo(x, y)
        else ctx.lineTo(x, y)
      }
      ctx.stroke()
      ctx.restore()
    }

    // Draw playheads
    const engine = engineRef.current
    if (engine) {
      const phases = engine.getPhases()
      for (let lane = 0; lane < 3; lane++) {
        if (!snaps[lane]?.valid) continue
        const phase = phases[lane]
        const x = phase * W
        const clr = laneColors[lane]
        ctx.save()
        ctx.strokeStyle = clr
        ctx.lineWidth = 1.5
        ctx.globalAlpha = lane === focusedLaneRef.current ? 0.9 : 0.4
        ctx.setLineDash([4, 4])
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke()
        ctx.setLineDash([])
        const snap = snaps[lane]
        if (snap) {
          const idx = Math.round(phase * 255) & 255
          const y = (1 - snap.table[idx]) * H
          ctx.globalAlpha = lane === focusedLaneRef.current ? 1 : 0.5
          ctx.fillStyle = clr
          ctx.beginPath()
          ctx.arc(x, y, lane === focusedLaneRef.current ? 5 : 3.5, 0, Math.PI * 2)
          ctx.fill()
          ctx.strokeStyle = dark ? '#111' : '#fff'
          ctx.lineWidth = 1.5
          ctx.globalAlpha = 1
          ctx.stroke()
        }
        ctx.restore()
      }
    }

    rafRef.current = requestAnimationFrame(draw)
  }, [engineRef])

  useEffect(() => {
    rafRef.current = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(rafRef.current)
  }, [draw])

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const observer = new ResizeObserver(() => {
      const dpr = window.devicePixelRatio || 1
      const rect = canvas.getBoundingClientRect()
      canvas.width = Math.round(rect.width * dpr)
      canvas.height = Math.round(rect.height * dpr)
      const ctx = canvas.getContext('2d')
      ctx?.scale(dpr, dpr)
    })
    observer.observe(canvas)
    return () => observer.disconnect()
  }, [])

  const getPos = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const rect = canvasRef.current!.getBoundingClientRect()
    return {
      x: Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width)),
      y: Math.max(0, Math.min(1, (e.clientY - rect.top) / rect.height)),
    }
  }

  const handlePointerDown = useCallback(
    (e: React.PointerEvent<HTMLCanvasElement>) => {
      e.currentTarget.setPointerCapture(e.pointerId)
      isDrawingRef.current = true
      drawingPointsRef.current = []
      captureRef.current.begin()
      engineRef.current?.stopLane(focusedLane)
      const pos = getPos(e)
      captureRef.current.addPoint(pos.x, pos.y)
      drawingPointsRef.current.push(pos)
    },
    [engineRef, focusedLane],
  )

  const handlePointerMove = useCallback((e: React.PointerEvent<HTMLCanvasElement>) => {
    if (!isDrawingRef.current) return
    const pos = getPos(e)
    captureRef.current.addPoint(pos.x, pos.y)
    drawingPointsRef.current.push(pos)
  }, [])

  const handlePointerUp = useCallback(
    (e: React.PointerEvent<HTMLCanvasElement>) => {
      if (!isDrawingRef.current) return
      isDrawingRef.current = false
      const pos = getPos(e)
      captureRef.current.addPoint(pos.x, pos.y)
      const snapshot = captureRef.current.finalize(laneParams[focusedLane])
      if (snapshot) {
        engineRef.current?.setSnapshot(focusedLane, snapshot)
        onCurveDrawn(focusedLane, snapshot)
        engineRef.current?.resetLane(focusedLane)
      }
      drawingPointsRef.current = []
    },
    [engineRef, focusedLane, laneParams, onCurveDrawn],
  )

  // ── Axis control helpers ─────────────────────────────────────────────────

  const clampX = (n: number) => Math.max(2, Math.min(32, n))
  const clampY = (n: number) => Math.max(2, Math.min(24, n))

  // Tick label generators
  const xLabel = (tick: number): string => {
    const pct = Math.round((tick / params.xDivisions) * 100)
    return pct + '%'
  }

  const yLabel = (tick: number): string => {
    // tick 1 = just below top; tick yDivisions-1 = just above bottom
    const valueFrac = 1 - tick / params.yDivisions  // 0=bottom, 1=top
    const ranged = params.minOut + valueFrac * (params.maxOut - params.minOut)
    if (params.messageType === MessageType.Note) {
      const note = Math.max(0, Math.min(127, Math.round(ranged * 127)))
      return midiNoteName(note)
    }
    if (params.messageType === MessageType.PitchBend) {
      return Math.round((ranged - 0.5) * 200) + '%'
    }
    return String(Math.round(ranged * 127))
  }

  // ── Shared button styles ─────────────────────────────────────────────────

  const axisBtn: React.CSSProperties = {
    width: 22,
    height: 18,
    padding: 0,
    fontSize: 13,
    lineHeight: '18px',
    borderRadius: 3,
    border: `1px solid ${dark ? '#444' : '#ccc'}`,
    background: 'transparent',
    color: dark ? '#888' : '#999',
    cursor: 'pointer',
    flexShrink: 0,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
  }

  const lockBtn = (active: boolean): React.CSSProperties => ({
    ...axisBtn,
    border: `1px solid ${active ? color : (dark ? '#444' : '#ccc')}`,
    color: active ? color : (dark ? '#666' : '#bbb'),
    fontWeight: active ? 700 : 400,
    fontSize: 11,
  })

  const countStyle: React.CSSProperties = {
    fontSize: 10,
    fontWeight: 600,
    color: dark ? '#ccc' : '#555',
    textAlign: 'center' as const,
    minWidth: 16,
    userSelect: 'none',
  }

  const labelStyle: React.CSSProperties = {
    fontSize: 8,
    pointerEvents: 'none',
    background: dark ? 'rgba(0,0,0,0.55)' : 'rgba(255,255,255,0.75)',
    color: dark ? '#aaa' : '#777',
    padding: '0 2px',
    borderRadius: 2,
    lineHeight: '12px',
    whiteSpace: 'nowrap' as const,
  }

  const bothLocked = params.xQuantize && params.yQuantize

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 0 }}>

      {/* Main row: Y controls + canvas */}
      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>

        {/* Y axis controls (left column) */}
        <div style={{
          width: 26,
          flexShrink: 0,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          gap: 3,
          paddingRight: 2,
          paddingBottom: 4,
        }}>
          <button style={axisBtn} onClick={() => update({ yDivisions: clampY(params.yDivisions + 1) })}>+</button>
          <span style={countStyle}>{params.yDivisions}</span>
          <button style={axisBtn} onClick={() => update({ yDivisions: clampY(params.yDivisions - 1) })}>−</button>
          <div style={{ flex: 1 }} />
          <button
            style={lockBtn(params.yQuantize)}
            onClick={() => update({ yQuantize: !params.yQuantize })}
            title={params.yQuantize ? 'Unlock Y' : 'Lock Y quantization'}
          >
            {params.yQuantize ? '⊠' : '⊡'}
          </button>
        </div>

        {/* Canvas with tick labels */}
        <div style={{ flex: 1, position: 'relative', minWidth: 0 }}>
          <canvas
            ref={canvasRef}
            style={{
              display: 'block',
              width: '100%',
              height: '100%',
              touchAction: 'none',
              cursor: 'crosshair',
              borderRadius: '6px',
            }}
            onPointerDown={handlePointerDown}
            onPointerMove={handlePointerMove}
            onPointerUp={handlePointerUp}
            onPointerCancel={handlePointerUp}
          />

          {/* Y tick labels (interior lines only) */}
          {Array.from({ length: params.yDivisions - 1 }, (_, i) => {
            const tick = i + 1
            return (
              <span key={tick} style={{
                ...labelStyle,
                position: 'absolute',
                right: 3,
                top: `${(tick / params.yDivisions) * 100}%`,
                transform: 'translateY(-50%)',
              }}>
                {yLabel(tick)}
              </span>
            )
          })}

          {/* X tick labels (interior lines only) */}
          {Array.from({ length: params.xDivisions - 1 }, (_, i) => {
            const tick = i + 1
            return (
              <span key={tick} style={{
                ...labelStyle,
                position: 'absolute',
                bottom: 3,
                left: `${(tick / params.xDivisions) * 100}%`,
                transform: 'translateX(-50%)',
              }}>
                {xLabel(tick)}
              </span>
            )
          })}
        </div>
      </div>

      {/* Bottom row: corner # + X axis controls */}
      <div style={{ height: 26, display: 'flex', alignItems: 'center', gap: 3, paddingTop: 2 }}>

        {/* Corner: toggle both */}
        <div style={{ width: 26, flexShrink: 0, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
          <button
            style={{
              ...axisBtn,
              border: `1px solid ${bothLocked ? color : (dark ? '#444' : '#ccc')}`,
              color: bothLocked ? color : (dark ? '#666' : '#bbb'),
              fontWeight: bothLocked ? 700 : 400,
              fontSize: 11,
            }}
            onClick={() => update({ xQuantize: !bothLocked, yQuantize: !bothLocked })}
            title="Toggle both X + Y quantization"
          >
            #
          </button>
        </div>

        {/* X axis controls */}
        <button
          style={lockBtn(params.xQuantize)}
          onClick={() => update({ xQuantize: !params.xQuantize })}
          title={params.xQuantize ? 'Unlock X' : 'Lock X quantization'}
        >
          {params.xQuantize ? '⊠' : '⊡'}
        </button>
        <button style={axisBtn} onClick={() => update({ xDivisions: clampX(params.xDivisions - 1) })}>−</button>
        <span style={countStyle}>{params.xDivisions}</span>
        <button style={axisBtn} onClick={() => update({ xDivisions: clampX(params.xDivisions + 1) })}>+</button>
      </div>

    </div>
  )
}
