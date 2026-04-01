import { useRef, useEffect, useCallback } from 'react'
import { CaptureSession } from '../engine/captureSession'
import { GestureEngine } from '../engine/gestureEngine'
import { type LaneSnapshot, type LaneParams, type ScaleConfig, MessageType } from '../engine/types'
import { NOTE_NAMES } from '../engine/scaleData'

// ── Constants ────────────────────────────────────────────────────────────────

const LANE_COLORS_DARK  = ['#4a90e2', '#e8a838', '#5cb85c']
const LANE_COLORS_LIGHT = ['#1a60c8', '#c87010', '#228b22']
const GRID_COLOR_DARK   = 'rgba(255,255,255,0.07)'
const GRID_COLOR_LIGHT  = 'rgba(0,0,0,0.08)'

// Plot area margins (CSS px) — left/bottom reserve space for axis labels
const PL = 34   // left   — Y labels
const PT = 2    // top
const PR = 2    // right
const PB = 16   // bottom — X labels

// ── Helpers ──────────────────────────────────────────────────────────────────

function midiNoteName(note: number): string {
  return NOTE_NAMES[note % 12] + (Math.floor(note / 12) - 1)
}

/** Snap a normalised value [0,1] to the nearest of yDiv equally-spaced levels. */
function snapToYGrid(val: number, yDiv: number): number {
  return Math.max(0, Math.min(1, Math.round(val * (yDiv - 1)) / (yDiv - 1)))
}

/** Map a normalised curve value to canvas Y, applying Note+scale snapping. */
function toVisualY(
  val: number,
  snap: LaneSnapshot,
  sc: ScaleConfig,
  plotY: number,
  plotH: number,
): number {
  if (snap.messageType === MessageType.Note) {
    const rawNoteF = (snap.minOut + val * (snap.maxOut - snap.minOut)) * 127
    const rawNote  = Math.max(0, Math.min(127, Math.round(rawNoteF)))
    const snapped  = GestureEngine.quantizeNote(rawNote, sc, true)
    const snNorm   = (snapped / 127 - snap.minOut) / Math.max(snap.maxOut - snap.minOut, 0.001)
    return plotY + (1 - Math.max(0, Math.min(1, snNorm))) * plotH
  }
  return plotY + (1 - val) * plotH
}

// ── Types ────────────────────────────────────────────────────────────────────

interface CurveDisplayProps {
  snapshots: (LaneSnapshot | null)[]
  laneParams: LaneParams[]
  focusedLane: number
  theme: 'light' | 'dark'
  engineRef: React.RefObject<GestureEngine>
  onCurveDrawn: (lane: number, snapshot: LaneSnapshot) => void
  onUpdateParams: (lane: number, partial: Partial<LaneParams>) => void
}

// ── Component ─────────────────────────────────────────────────────────────────

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
  const canvasRef      = useRef<HTMLCanvasElement>(null)
  const captureRef     = useRef(new CaptureSession())
  const isDrawingRef   = useRef(false)
  const drawingPtsRef  = useRef<{ x: number; y: number }[]>([])
  const rafRef         = useRef<number>(0)

  // Stable refs so rAF closure never goes stale
  const snapshotsRef   = useRef(snapshots)
  const focusedRef     = useRef(focusedLane)
  const themeRef       = useRef(theme)
  const paramsRef      = useRef(laneParams)
  snapshotsRef.current = snapshots
  focusedRef.current   = focusedLane
  themeRef.current     = theme
  paramsRef.current    = laneParams

  const params = laneParams[focusedLane]
  const colors = dark ? LANE_COLORS_DARK : LANE_COLORS_LIGHT
  const color  = colors[focusedLane]

  const update = useCallback(
    (partial: Partial<LaneParams>) => onUpdateParams(focusedLane, partial),
    [onUpdateParams, focusedLane],
  )

  // ── Main draw loop ────────────────────────────────────────────────────────

  const draw = useCallback(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    const dpr   = window.devicePixelRatio || 1
    const W     = canvas.width  / dpr
    const H     = canvas.height / dpr
    const plotX = PL
    const plotY = PT
    const plotW = W - PL - PR
    const plotH = H - PT - PB
    if (plotW < 1 || plotH < 1) { rafRef.current = requestAnimationFrame(draw); return }

    const dark       = themeRef.current === 'dark'
    const laneColors = dark ? LANE_COLORS_DARK : LANE_COLORS_LIGHT
    const gridColor  = dark ? GRID_COLOR_DARK  : GRID_COLOR_LIGHT
    const snaps      = snapshotsRef.current
    const fp         = paramsRef.current[focusedRef.current]
    const gx         = fp.xDivisions
    const gy         = fp.yDivisions
    const xQ         = fp.xQuantize
    const yQ         = fp.yQuantize
    const fColor     = laneColors[focusedRef.current]

    // ── Background ────────────────────────────────────────────────────────
    ctx.fillStyle = dark ? '#111' : '#f8f8f8'
    ctx.fillRect(0, 0, W, H)
    // Darker margins for labels
    ctx.fillStyle = dark ? '#0c0c0c' : '#ebebeb'
    ctx.fillRect(0, 0, plotX, H)
    ctx.fillRect(0, plotY + plotH, W, PB)

    // ── Note-band visualization (focused lane, Note + non-chromatic scale) ─
    const bands: { note: number; y: number }[] = []
    if (fp.messageType === MessageType.Note && fp.scaleConfig.mask !== 0xfff) {
      const sc   = fp.scaleConfig
      const minN = Math.round(fp.minOut * 127)
      const maxN = Math.round(fp.maxOut * 127)
      for (let n = maxN; n >= minN; n--) {
        const interval = ((n % 12) - sc.root + 12) % 12
        if ((sc.mask >> (11 - interval)) & 1) {
          const norm = (n / 127 - fp.minOut) / Math.max(fp.maxOut - fp.minOut, 0.001)
          bands.push({ note: n, y: plotY + (1 - Math.max(0, Math.min(1, norm))) * plotH })
        }
      }
      if (bands.length >= 2) {
        for (let i = 0; i < bands.length; i++) {
          const noteY  = bands[i].y
          const halfUp = i === 0               ? (noteY - plotY)              * 0.5 : (noteY - bands[i - 1].y) * 0.5
          const halfDn = i + 1 < bands.length  ? (bands[i + 1].y - noteY)    * 0.5 : (plotY + plotH - noteY)  * 0.5
          const bandH  = halfUp + halfDn
          if (bandH < 0.5) continue
          ctx.fillStyle = (i & 1)
            ? (dark ? 'rgba(255,255,255,0.04)' : 'rgba(0,0,0,0.03)')
            : (dark ? 'rgba(255,255,255,0.10)' : 'rgba(0,0,0,0.07)')
          ctx.fillRect(plotX, noteY - halfUp, plotW, bandH)
        }
      }
    }

    // ── Grid lines ───────────────────────────────────────────────────────
    ctx.lineWidth = 1
    for (let i = 1; i < gx; i++) {
      ctx.strokeStyle = xQ ? (dark ? 'rgba(255,255,255,0.20)' : 'rgba(0,0,0,0.16)') : gridColor
      const x = plotX + (i / gx) * plotW
      ctx.beginPath(); ctx.moveTo(x, plotY); ctx.lineTo(x, plotY + plotH); ctx.stroke()
    }
    for (let i = 1; i < gy; i++) {
      ctx.strokeStyle = yQ ? (dark ? 'rgba(255,255,255,0.20)' : 'rgba(0,0,0,0.16)') : gridColor
      const y = plotY + (i / gy) * plotH
      ctx.beginPath(); ctx.moveTo(plotX, y); ctx.lineTo(plotX + plotW, y); ctx.stroke()
    }

    // ── Quantize emphasis (thicker, lane-coloured) ───────────────────────
    if (xQ) {
      ctx.save(); ctx.fillStyle = fColor; ctx.globalAlpha = 0.22
      for (let i = 1; i < gx; i++) {
        const x = plotX + (i / gx) * plotW
        ctx.fillRect(x - 0.75, plotY, 1.5, plotH)
      }
      ctx.restore()
    }
    if (yQ) {
      ctx.save(); ctx.fillStyle = fColor; ctx.globalAlpha = 0.22
      for (let i = 1; i < gy; i++) {
        const y = plotY + (i / gy) * plotH
        ctx.fillRect(plotX, y - 0.75, plotW, 1.5)
      }
      ctx.restore()
    }

    // ── Plot area border ─────────────────────────────────────────────────
    ctx.strokeStyle = dark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.15)'
    ctx.lineWidth = 1
    ctx.strokeRect(plotX + 0.5, plotY + 0.5, plotW - 1, plotH - 1)

    // ── Raw curves ────────────────────────────────────────────────────────
    for (let lane = 0; lane < 3; lane++) {
      const snap = snaps[lane]
      if (!snap?.valid) continue
      const focused = lane === focusedRef.current
      ctx.save()
      ctx.globalAlpha  = focused ? 1 : 0.3
      ctx.strokeStyle  = laneColors[lane]
      ctx.lineWidth    = focused ? 2.5 : 1.5
      ctx.lineJoin     = 'round'
      ctx.beginPath()
      for (let i = 0; i < 256; i++) {
        const x = plotX + (i / 255) * plotW
        const y = plotY + (1 - snap.table[i]) * plotH
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
      }
      ctx.stroke()
      ctx.restore()
    }

    // ── In-progress gesture ───────────────────────────────────────────────
    if (isDrawingRef.current && drawingPtsRef.current.length >= 2) {
      ctx.save()
      ctx.strokeStyle = laneColors[focusedRef.current]
      ctx.lineWidth   = 2.5
      ctx.globalAlpha = 0.7
      ctx.lineJoin    = 'round'
      ctx.beginPath()
      const pts = drawingPtsRef.current
      for (let i = 0; i < pts.length; i++) {
        const x = plotX + pts[i].x * plotW
        const y = plotY + pts[i].y * plotH
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
      }
      ctx.stroke()
      ctx.restore()
    }

    // ── Staircase overlay ─────────────────────────────────────────────────
    // Drawn over the raw curve for any lane with X or Y quantize active.
    // X-only  → S&H: value sampled at tick start, held for full step width.
    // Y-only  → 256-sample path, each value snapped to nearest grid level.
    // X + Y   → S&H with Y snap applied to each held value.
    // Note    → toVisualY applies quantizeNote so stair positions match engine.
    for (let stairLane = 0; stairLane < 3; stairLane++) {
      const snap = snaps[stairLane]
      if (!snap?.valid) continue
      const lp  = paramsRef.current[stairLane]
      const sxQ = lp.xQuantize && lp.xDivisions >= 2
      const syQ = lp.yQuantize && lp.yDivisions >= 2
      if (!sxQ && !syQ) continue

      const sc        = lp.scaleConfig
      const isFocused = stairLane === focusedRef.current

      ctx.save()
      ctx.strokeStyle = laneColors[stairLane]
      ctx.lineWidth   = isFocused ? 2.5 : 1.8
      ctx.globalAlpha = isFocused ? 0.75 : 0.45
      ctx.lineJoin    = 'round'
      ctx.beginPath()

      if (sxQ) {
        // Sample-and-hold staircase across X divisions
        let started = false
        for (let i = 0; i < lp.xDivisions; i++) {
          const x1   = plotX + (i / lp.xDivisions) * plotW
          const x2   = plotX + ((i + 1) / lp.xDivisions) * plotW
          const tidx = Math.max(0, Math.min(255, Math.round(i / lp.xDivisions * 255)))
          const raw  = snap.table[tidx]
          const val  = syQ ? snapToYGrid(raw, lp.yDivisions) : raw
          const cy   = toVisualY(val, snap, sc, plotY, plotH)
          if (!started) { ctx.moveTo(x1, cy); started = true }
          else            ctx.lineTo(x1, cy)   // vertical riser
          ctx.lineTo(x2, cy)                   // horizontal tread
        }
      } else {
        // Y-only: full 256-sample path, value snapped to grid
        for (let i = 0; i < 256; i++) {
          const cx  = plotX + (i / 255) * plotW
          const val = snapToYGrid(snap.table[i], lp.yDivisions)
          const cy  = toVisualY(val, snap, sc, plotY, plotH)
          if (i === 0) ctx.moveTo(cx, cy); else ctx.lineTo(cx, cy)
        }
      }

      ctx.stroke()
      ctx.restore()
    }

    // ── Playheads ─────────────────────────────────────────────────────────
    const engine = engineRef.current
    if (engine) {
      const phases = engine.getPhases()
      for (let lane = 0; lane < 3; lane++) {
        if (!snaps[lane]?.valid) continue
        const phase = phases[lane]
        const x     = plotX + phase * plotW
        const clr   = laneColors[lane]
        ctx.save()
        ctx.strokeStyle = clr
        ctx.lineWidth   = 1.5
        ctx.globalAlpha = lane === focusedRef.current ? 0.9 : 0.4
        ctx.setLineDash([4, 4])
        ctx.beginPath(); ctx.moveTo(x, plotY); ctx.lineTo(x, plotY + plotH); ctx.stroke()
        ctx.setLineDash([])
        const snap = snaps[lane]
        if (snap) {
          const idx = Math.round(phase * 255) & 255
          const y   = plotY + (1 - snap.table[idx]) * plotH
          ctx.globalAlpha = lane === focusedRef.current ? 1 : 0.5
          ctx.fillStyle   = clr
          ctx.beginPath()
          ctx.arc(x, y, lane === focusedRef.current ? 5 : 3.5, 0, Math.PI * 2)
          ctx.fill()
          ctx.strokeStyle = dark ? '#111' : '#fff'
          ctx.lineWidth   = 1.5
          ctx.globalAlpha = 1
          ctx.stroke()
        }
        ctx.restore()
      }
    }

    // ── Y-axis labels (left margin) ───────────────────────────────────────
    ctx.font          = '9px -apple-system, BlinkMacSystemFont, sans-serif'
    ctx.textAlign     = 'right'
    ctx.textBaseline  = 'middle'
    ctx.fillStyle     = dark ? 'rgba(255,255,255,0.42)' : 'rgba(0,0,0,0.38)'

    if (bands.length >= 2) {
      // Note + scale: one label per visible scale note
      let lastY = -99
      for (const { note, y } of bands) {
        const ly = Math.max(plotY + 1, Math.min(plotY + plotH - 5, y))
        if (ly < lastY + 10) continue
        ctx.fillText(midiNoteName(note), plotX - 3, ly)
        lastY = ly
      }
    } else {
      // CC / PB / Note-chromatic: labels at each Y grid division
      let lastY = -99
      for (let i = gy; i >= 0; i--) {
        const norm   = i / gy
        const y      = plotY + (1 - norm) * plotH
        const ly     = Math.max(plotY + 1, Math.min(plotY + plotH - 5, y))
        if (ly < lastY + 10) continue
        const ranged = fp.minOut + norm * (fp.maxOut - fp.minOut)
        let label    = ''
        switch (fp.messageType) {
          case MessageType.Note:
            label = midiNoteName(Math.max(0, Math.min(127, Math.round(ranged * 127))))
            break
          case MessageType.PitchBend: {
            const pb = Math.round((ranged - 0.5) * 200)
            label = (pb >= 0 ? '+' : '') + pb + '%'
            break
          }
          default:
            label = String(Math.round(ranged * 127))
        }
        ctx.fillText(label, plotX - 3, ly)
        lastY = ly
      }
    }

    // ── X-axis labels (bottom margin) ─────────────────────────────────────
    ctx.textAlign    = 'center'
    ctx.textBaseline = 'top'
    ctx.fillStyle    = dark ? 'rgba(255,255,255,0.35)' : 'rgba(0,0,0,0.30)'
    for (let i = 1; i < gx; i++) {
      const x   = plotX + (i / gx) * plotW
      const pct = Math.round((i / gx) * 100)
      ctx.fillText(pct + '%', x, plotY + plotH + 3)
    }

    rafRef.current = requestAnimationFrame(draw)
  }, [engineRef])

  // ── Effects ───────────────────────────────────────────────────────────────

  useEffect(() => {
    rafRef.current = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(rafRef.current)
  }, [draw])

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const observer = new ResizeObserver(() => {
      const dpr  = window.devicePixelRatio || 1
      const rect = canvas.getBoundingClientRect()
      canvas.width  = Math.round(rect.width  * dpr)
      canvas.height = Math.round(rect.height * dpr)
      const ctx = canvas.getContext('2d')
      ctx?.scale(dpr, dpr)
    })
    observer.observe(canvas)
    return () => observer.disconnect()
  }, [])

  // ── Pointer handling (plot-area relative) ─────────────────────────────────

  const getPos = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const rect = canvasRef.current!.getBoundingClientRect()
    const pW   = rect.width  - PL - PR
    const pH   = rect.height - PT - PB
    return {
      x: Math.max(0, Math.min(1, (e.clientX - rect.left  - PL) / pW)),
      y: Math.max(0, Math.min(1, (e.clientY - rect.top   - PT) / pH)),
    }
  }

  const handlePointerDown = useCallback(
    (e: React.PointerEvent<HTMLCanvasElement>) => {
      e.currentTarget.setPointerCapture(e.pointerId)
      isDrawingRef.current  = true
      drawingPtsRef.current = []
      captureRef.current.begin()
      engineRef.current?.stopLane(focusedLane)
      const pos = getPos(e)
      captureRef.current.addPoint(pos.x, pos.y)
      drawingPtsRef.current.push(pos)
    },
    [engineRef, focusedLane],
  )

  const handlePointerMove = useCallback((e: React.PointerEvent<HTMLCanvasElement>) => {
    if (!isDrawingRef.current) return
    const pos = getPos(e)
    captureRef.current.addPoint(pos.x, pos.y)
    drawingPtsRef.current.push(pos)
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
      drawingPtsRef.current = []
    },
    [engineRef, focusedLane, laneParams, onCurveDrawn],
  )

  // ── Axis control styles ───────────────────────────────────────────────────

  const axisBtn: React.CSSProperties = {
    width: 22, height: 18, padding: 0, fontSize: 13, lineHeight: '18px',
    borderRadius: 3,
    border: `1px solid ${dark ? '#444' : '#ccc'}`,
    background: 'transparent',
    color: dark ? '#888' : '#999',
    cursor: 'pointer', flexShrink: 0,
    display: 'flex', alignItems: 'center', justifyContent: 'center',
  }

  const lockBtnStyle = (active: boolean): React.CSSProperties => ({
    ...axisBtn,
    border:     `1px solid ${active ? color : (dark ? '#444' : '#ccc')}`,
    color:      active ? color : (dark ? '#555' : '#c0c0c0'),
    fontWeight: active ? 700 : 400,
    fontSize:   11,
  })

  const countStyle: React.CSSProperties = {
    fontSize: 10, fontWeight: 600, textAlign: 'center',
    color: dark ? '#ccc' : '#555', minWidth: 16, userSelect: 'none',
  }

  const bothLocked = params.xQuantize && params.yQuantize

  // ── Render ────────────────────────────────────────────────────────────────

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>

      {/* Main row: Y controls + canvas */}
      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>

        {/* Y axis controls */}
        <div style={{
          width: 26, flexShrink: 0,
          display: 'flex', flexDirection: 'column', alignItems: 'center',
          justifyContent: 'center', gap: 3, paddingRight: 2, paddingBottom: 4,
        }}>
          <button style={axisBtn} onClick={() => update({ yDivisions: Math.min(24, params.yDivisions + 1) })}>+</button>
          <span style={countStyle}>{params.yDivisions}</span>
          <button style={axisBtn} onClick={() => update({ yDivisions: Math.max(2, params.yDivisions - 1) })}>−</button>
          <div style={{ flex: 1 }} />
          <button
            style={lockBtnStyle(params.yQuantize)}
            onClick={() => update({ yQuantize: !params.yQuantize })}
            title={params.yQuantize ? 'Unlock Y' : 'Lock Y quantization'}
          >
            {params.yQuantize ? '⊠' : '⊡'}
          </button>
        </div>

        {/* Canvas */}
        <div style={{ flex: 1, position: 'relative', minWidth: 0 }}>
          <canvas
            ref={canvasRef}
            style={{
              display: 'block', width: '100%', height: '100%',
              touchAction: 'none', cursor: 'crosshair', borderRadius: '6px',
            }}
            onPointerDown={handlePointerDown}
            onPointerMove={handlePointerMove}
            onPointerUp={handlePointerUp}
            onPointerCancel={handlePointerUp}
          />
        </div>
      </div>

      {/* Bottom row: corner # + X axis controls */}
      <div style={{ height: 26, display: 'flex', alignItems: 'center', gap: 3, paddingTop: 2 }}>

        {/* Corner: toggle both X + Y */}
        <div style={{ width: 26, flexShrink: 0, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
          <button
            style={{
              ...axisBtn,
              border:     `1px solid ${bothLocked ? color : (dark ? '#444' : '#ccc')}`,
              color:      bothLocked ? color : (dark ? '#555' : '#c0c0c0'),
              fontWeight: bothLocked ? 700 : 400, fontSize: 11,
            }}
            onClick={() => update({ xQuantize: !bothLocked, yQuantize: !bothLocked })}
            title="Toggle both X + Y quantization"
          >
            #
          </button>
        </div>

        {/* X axis controls */}
        <button
          style={lockBtnStyle(params.xQuantize)}
          onClick={() => update({ xQuantize: !params.xQuantize })}
          title={params.xQuantize ? 'Unlock X' : 'Lock X quantization'}
        >
          {params.xQuantize ? '⊠' : '⊡'}
        </button>
        <button style={axisBtn} onClick={() => update({ xDivisions: Math.max(2, params.xDivisions - 1) })}>−</button>
        <span style={countStyle}>{params.xDivisions}</span>
        <button style={axisBtn} onClick={() => update({ xDivisions: Math.min(32, params.xDivisions + 1) })}>+</button>
      </div>

    </div>
  )
}
