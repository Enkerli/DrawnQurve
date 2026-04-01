import { useRef, useEffect, useCallback } from 'react'
import { CaptureSession } from '../engine/captureSession'
import { type LaneSnapshot, type LaneParams } from '../engine/types'
import { type GestureEngine } from '../engine/gestureEngine'

const LANE_COLORS_DARK = ['#4a90e2', '#e8a838', '#5cb85c']
const LANE_COLORS_LIGHT = ['#1a60c8', '#c87010', '#228b22']
const GRID_COLOR_DARK = 'rgba(255,255,255,0.07)'
const GRID_COLOR_LIGHT = 'rgba(0,0,0,0.08)'

interface CurveDisplayProps {
  snapshots: (LaneSnapshot | null)[]
  laneParams: LaneParams[]
  focusedLane: number
  theme: 'light' | 'dark'
  engineRef: React.RefObject<GestureEngine>
  onCurveDrawn: (lane: number, snapshot: LaneSnapshot) => void
  gridX?: number      // number of vertical divisions
  gridY?: number      // number of horizontal divisions
  xQuantized?: boolean  // true = grid is an active step grid
  yQuantized?: boolean
}

export function CurveDisplay({
  snapshots,
  laneParams,
  focusedLane,
  theme,
  engineRef,
  onCurveDrawn,
  gridX = 8,
  gridY = 4,
  xQuantized = false,
  yQuantized = false,
}: CurveDisplayProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const captureRef = useRef(new CaptureSession())
  const isDrawingRef = useRef(false)
  const drawingPointsRef = useRef<{ x: number; y: number }[]>([])
  const rafRef = useRef<number>(0)

  // Keep refs to avoid stale closures in rAF
  const snapshotsRef = useRef(snapshots)
  const focusedLaneRef = useRef(focusedLane)
  const themeRef = useRef(theme)
  const gridXRef = useRef(gridX)
  const gridYRef = useRef(gridY)
  const xQuantizedRef = useRef(xQuantized)
  const yQuantizedRef = useRef(yQuantized)
  snapshotsRef.current = snapshots
  focusedLaneRef.current = focusedLane
  themeRef.current = theme
  gridXRef.current = gridX
  gridYRef.current = gridY
  xQuantizedRef.current = xQuantized
  yQuantizedRef.current = yQuantized

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

    // Background
    ctx.fillStyle = dark ? '#111' : '#f8f8f8'
    ctx.fillRect(0, 0, W, H)

    // Grid
    const gx = gridXRef.current
    const gy = gridYRef.current
    const xQ = xQuantizedRef.current
    const yQ = yQuantizedRef.current

    ctx.lineWidth = 1
    for (let i = 1; i < gx; i++) {
      const x = (i / gx) * W
      ctx.strokeStyle = xQ
        ? dark ? 'rgba(255,255,255,0.18)' : 'rgba(0,0,0,0.14)'
        : gridColor
      ctx.beginPath()
      ctx.moveTo(x, 0)
      ctx.lineTo(x, H)
      ctx.stroke()
    }
    for (let i = 1; i < gy; i++) {
      const y = (i / gy) * H
      ctx.strokeStyle = yQ
        ? dark ? 'rgba(255,255,255,0.18)' : 'rgba(0,0,0,0.14)'
        : gridColor
      ctx.beginPath()
      ctx.moveTo(0, y)
      ctx.lineTo(W, y)
      ctx.stroke()
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

    // Draw in-progress curve (live during drawing)
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
        const color = laneColors[lane]

        // Vertical line
        ctx.save()
        ctx.strokeStyle = color
        ctx.lineWidth = 1.5
        ctx.globalAlpha = lane === focusedLaneRef.current ? 0.9 : 0.4
        ctx.setLineDash([4, 4])
        ctx.beginPath()
        ctx.moveTo(x, 0)
        ctx.lineTo(x, H)
        ctx.stroke()
        ctx.setLineDash([])

        // Dot at current curve value
        const snap = snaps[lane]
        if (snap) {
          const idx = Math.round(phase * 255) & 255
          const y = (1 - snap.table[idx]) * H
          ctx.globalAlpha = lane === focusedLaneRef.current ? 1 : 0.5
          ctx.fillStyle = color
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

  // Start/stop rAF loop
  useEffect(() => {
    rafRef.current = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(rafRef.current)
  }, [draw])

  // Handle HiDPI
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

  // Pointer events for drawing
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
      // Stop any held note on this lane before new capture
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

      const params = laneParams[focusedLane]
      const snapshot = captureRef.current.finalize(params)
      if (snapshot) {
        engineRef.current?.setSnapshot(focusedLane, snapshot)
        onCurveDrawn(focusedLane, snapshot)
        engineRef.current?.resetLane(focusedLane)
      }
      drawingPointsRef.current = []
    },
    [engineRef, focusedLane, laneParams, onCurveDrawn],
  )

  return (
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
  )
}
