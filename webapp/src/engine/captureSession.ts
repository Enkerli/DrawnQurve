import { type LaneSnapshot, type LaneParams, MessageType } from './types'

interface CapturePoint {
  t: number  // seconds since begin()
  x: number  // 0-1 left→right (for display only)
  y: number  // 0-1 top→bottom (UIKit convention)
}

/** Collects raw pointer points and resamples them into a 256-sample LaneSnapshot. */
export class CaptureSession {
  private points: CapturePoint[] = []
  private startTime = 0

  begin(): void {
    this.points = []
    this.startTime = performance.now()
  }

  addPoint(x: number, y: number): void {
    const t = (performance.now() - this.startTime) / 1000
    this.points.push({ t, x, y })
  }

  hasPoints(): boolean {
    return this.points.length >= 2
  }

  clear(): void {
    this.points = []
  }

  /** Raw points for in-progress drawing display (x, y in [0,1] each). */
  getRawPoints(): { x: number; y: number }[] {
    return this.points.map(p => ({ x: p.x, y: p.y }))
  }

  /** Resample gesture into a 256-sample LaneSnapshot. Returns null if too few points. */
  finalize(params: LaneParams): LaneSnapshot | null {
    if (this.points.length < 2) return null

    const t0 = this.points[0].t
    const t1 = this.points[this.points.length - 1].t
    const duration = Math.max(0.05, t1 - t0)

    const table = new Float32Array(256)
    for (let i = 0; i < 256; i++) {
      const phase = i / 255
      const targetT = t0 + phase * duration
      const y = this.interpolateYAtTime(targetT)
      // Invert: top (y=0) → value 1.0, bottom (y=1) → value 0.0
      table[i] = Math.max(0, Math.min(1, 1 - y))
    }

    // Flat-gesture guard: if y spread < ~4 semitones, fill table with midpoint.
    let yMin = this.points[0].y
    let yMax = this.points[0].y
    for (const pt of this.points) {
      yMin = Math.min(yMin, pt.y)
      yMax = Math.max(yMax, pt.y)
    }
    const kSemitoneFrac = 4 / 127
    if (yMax - yMin < kSemitoneFrac) {
      const flatVal = Math.max(0, Math.min(1, 1 - (yMin + yMax) * 0.5))
      table.fill(flatVal)
    }

    return {
      table,
      durationSeconds: duration,
      ccNumber: params.ccNumber,
      midiChannel: params.midiChannel,
      minOut: params.minOut,
      maxOut: params.maxOut,
      smoothing: params.smoothing,
      messageType: params.messageType,
      noteVelocity: params.noteVelocity,
      phaseOffset: 0,
      valid: true,
    }
  }

  private interpolateYAtTime(t: number): number {
    if (this.points.length === 0) return 0.5
    if (this.points.length === 1) return this.points[0].y
    if (t <= this.points[0].t) return this.points[0].y
    if (t >= this.points[this.points.length - 1].t) return this.points[this.points.length - 1].y

    for (let i = 1; i < this.points.length; i++) {
      if (this.points[i].t >= t) {
        const t0 = this.points[i - 1].t
        const t1 = this.points[i].t
        if (t1 <= t0) return this.points[i].y
        const frac = (t - t0) / (t1 - t0)
        return this.points[i - 1].y + frac * (this.points[i].y - this.points[i - 1].y)
      }
    }
    return this.points[this.points.length - 1].y
  }
}

export { MessageType }
