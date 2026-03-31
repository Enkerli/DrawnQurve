import {
  type LaneSnapshot,
  type ScaleConfig,
  type MidiOutFn,
  MessageType,
  PlaybackDirection,
} from './types'

const MAX_LANES = 3

interface LaneRuntime {
  playheadSeconds: number
  lastSentValue: number   // -1 = nothing sent yet
  smoothedValue: number
}

/** Real-time MIDI playback engine — TypeScript port of GestureEngine.cpp. */
export class GestureEngine {
  private snapshots: (LaneSnapshot | null)[] = new Array(MAX_LANES).fill(null)
  private runtimes: LaneRuntime[] = Array.from({ length: MAX_LANES }, () => ({
    playheadSeconds: 0,
    lastSentValue: -1,
    smoothedValue: 0,
  }))
  private noteOffNeeded: boolean[] = new Array(MAX_LANES).fill(false)
  private scaleConfigs: ScaleConfig[] = Array.from({ length: MAX_LANES }, () => ({
    mask: 0xfff,
    root: 0,
  }))
  private laneEnabled: boolean[] = new Array(MAX_LANES).fill(true)
  private lanePhases: number[] = new Array(MAX_LANES).fill(0)
  private isPlaying = false

  // ── UI-thread API ────────────────────────────────────────────────────────

  setSnapshot(lane: number, snapshot: LaneSnapshot): void {
    if (lane < 0 || lane >= MAX_LANES) return
    this.snapshots[lane] = snapshot
    this.isPlaying = true
  }

  clearSnapshot(lane: number): void {
    if (lane < 0 || lane >= MAX_LANES) return
    this.snapshots[lane] = null
    this.noteOffNeeded[lane] = true
  }

  clearAllSnapshots(): void {
    for (let i = 0; i < MAX_LANES; i++) {
      this.snapshots[i] = null
      this.noteOffNeeded[i] = true
    }
  }

  setPlaying(playing: boolean): void {
    this.isPlaying = playing
    if (!playing) {
      for (let i = 0; i < MAX_LANES; i++) this.noteOffNeeded[i] = true
    }
  }

  reset(): void {
    for (let i = 0; i < MAX_LANES; i++) {
      if (!this.noteOffNeeded[i]) this.runtimes[i].lastSentValue = -1
      this.runtimes[i].playheadSeconds = 0
      this.runtimes[i].smoothedValue = 0
      this.lanePhases[i] = 0
    }
  }

  resetLane(lane: number): void {
    if (lane < 0 || lane >= MAX_LANES) return
    const rt = this.runtimes[lane]
    rt.playheadSeconds = 0
    const snap = this.snapshots[lane]
    rt.smoothedValue = snap?.valid ? this.sampleCurve(snap, snap.phaseOffset) : 0
    this.lanePhases[lane] = 0
  }

  stopLane(lane: number): void {
    if (lane < 0 || lane >= MAX_LANES) return
    this.noteOffNeeded[lane] = true
  }

  setScaleConfig(lane: number, config: ScaleConfig): void {
    if (lane < 0 || lane >= MAX_LANES) return
    this.scaleConfigs[lane] = { ...config }
  }

  setLaneEnabled(lane: number, enabled: boolean): void {
    if (lane < 0 || lane >= MAX_LANES) return
    const was = this.laneEnabled[lane]
    this.laneEnabled[lane] = enabled
    if (was && !enabled) this.stopLane(lane)
  }

  // ── Query API ────────────────────────────────────────────────────────────

  getPhases(): number[] {
    return [...this.lanePhases]
  }

  getPhase(lane: number): number {
    return this.lanePhases[lane] ?? 0
  }

  hasSnapshot(lane: number): boolean {
    return !!(this.snapshots[lane]?.valid)
  }

  // ── Render tick ──────────────────────────────────────────────────────────

  /** Call this periodically (e.g. every 10 ms) to advance playheads and emit MIDI. */
  tick(
    deltaSeconds: number,
    speedRatio: number,
    direction: PlaybackDirection,
    midiOut: MidiOutFn,
  ): void {
    for (let lane = 0; lane < MAX_LANES; lane++) {
      this.processLane(lane, deltaSeconds, speedRatio, direction, midiOut)
    }
  }

  // ── Static utilities ─────────────────────────────────────────────────────

  static quantizeNote(rawNote: number, sc: ScaleConfig, movingUp: boolean): number {
    if (sc.mask === 0xfff) return rawNote
    rawNote = Math.max(0, Math.min(127, Math.round(rawNote)))

    const pc = rawNote % 12
    const interval = ((pc - sc.root) % 12 + 12) % 12

    if ((sc.mask >> (11 - interval)) & 1) return rawNote

    let downNote = -1
    let upNote = -1
    for (let d = 1; d <= 6; d++) {
      if (downNote < 0 && rawNote - d >= 0) {
        const di = ((interval - d) % 12 + 12) % 12
        if ((sc.mask >> (11 - di)) & 1) downNote = rawNote - d
      }
      if (upNote < 0 && rawNote + d <= 127) {
        const ui = (interval + d) % 12
        if ((sc.mask >> (11 - ui)) & 1) upNote = rawNote + d
      }
      if (downNote >= 0 && upNote >= 0) break
    }

    if (downNote < 0 && upNote < 0) return rawNote
    if (downNote < 0) return upNote!
    if (upNote < 0) return downNote

    const dDown = rawNote - downNote
    const dUp = upNote - rawNote
    if (dDown === dUp) return movingUp ? upNote : downNote
    return dDown < dUp ? downNote : upNote
  }

  // ── Private ──────────────────────────────────────────────────────────────

  private sampleCurve(snap: LaneSnapshot, phase: number): number {
    const idx = phase * 255
    const i0 = Math.floor(idx) & 255
    const i1 = (i0 + 1) & 255
    const frac = idx - Math.floor(idx)
    return snap.table[i0] + frac * (snap.table[i1] - snap.table[i0])
  }

  private processLane(
    lane: number,
    deltaSeconds: number,
    speedRatio: number,
    direction: PlaybackDirection,
    midiOut: MidiOutFn,
  ): void {
    const rt = this.runtimes[lane]
    const snap = this.snapshots[lane]

    // ── Note Off cleanup ────────────────────────────────────────────────────
    if (this.noteOffNeeded[lane]) {
      this.noteOffNeeded[lane] = false
      if (snap?.valid && snap.messageType === MessageType.Note && rt.lastSentValue >= 0) {
        midiOut(0x80 | (snap.midiChannel & 0x0f), rt.lastSentValue, 0)
      }
      rt.lastSentValue = -1
    }

    if (!snap?.valid) return
    if (!this.isPlaying) return
    if (!this.laneEnabled[lane]) return

    const effectiveDur = snap.durationSeconds / Math.max(speedRatio, 0.001)

    // ── Advance playhead ────────────────────────────────────────────────────
    rt.playheadSeconds += deltaSeconds

    // ── Phase (direction-dependent) ─────────────────────────────────────────
    let phase: number
    if (direction === PlaybackDirection.Reverse) {
      if (rt.playheadSeconds >= effectiveDur)
        rt.playheadSeconds = rt.playheadSeconds % effectiveDur
      phase = 1 - rt.playheadSeconds / effectiveDur
    } else if (direction === PlaybackDirection.PingPong) {
      const ppDur = 2 * effectiveDur
      if (rt.playheadSeconds >= ppDur)
        rt.playheadSeconds = rt.playheadSeconds % ppDur
      const frac = rt.playheadSeconds / effectiveDur
      phase = frac <= 1 ? frac : 2 - frac
    } else {
      // Forward
      if (rt.playheadSeconds >= effectiveDur)
        rt.playheadSeconds = rt.playheadSeconds % effectiveDur
      phase = rt.playheadSeconds / effectiveDur
    }

    this.lanePhases[lane] = phase

    // Apply phase offset
    const sampledPhase = ((phase + snap.phaseOffset) % 1 + 1) % 1
    const target = this.sampleCurve(snap, sampledPhase)

    // ── One-pole smoother ────────────────────────────────────────────────────
    const alpha =
      snap.smoothing <= 0
        ? 1
        : 1 - Math.exp(-deltaSeconds / (snap.smoothing * 2))
    rt.smoothedValue += alpha * (target - rt.smoothedValue)

    // ── Output range mapping ─────────────────────────────────────────────────
    const ranged = snap.minOut + rt.smoothedValue * (snap.maxOut - snap.minOut)

    // ── Emit MIDI ────────────────────────────────────────────────────────────
    switch (snap.messageType) {
      case MessageType.CC: {
        const v = Math.max(0, Math.min(127, Math.round(ranged * 127)))
        if (v !== rt.lastSentValue) {
          midiOut(0xb0 | (snap.midiChannel & 0x0f), snap.ccNumber, v)
          rt.lastSentValue = v
        }
        break
      }

      case MessageType.ChannelPressure: {
        const v = Math.max(0, Math.min(127, Math.round(ranged * 127)))
        if (v !== rt.lastSentValue) {
          midiOut(0xd0 | (snap.midiChannel & 0x0f), v, 0)
          rt.lastSentValue = v
        }
        break
      }

      case MessageType.PitchBend: {
        const v = Math.max(0, Math.min(16383, Math.round(ranged * 16383)))
        if (v !== rt.lastSentValue) {
          midiOut(0xe0 | (snap.midiChannel & 0x0f), v & 0x7f, (v >> 7) & 0x7f)
          rt.lastSentValue = v
        }
        break
      }

      case MessageType.Note: {
        // Use raw target (not smoothed) for note detection — avoids glissando artefacts.
        const rawNoteF = (snap.minOut + target * (snap.maxOut - snap.minOut)) * 127
        const committed = rt.lastSentValue
        const sc = this.scaleConfigs[lane]
        const movingUp = committed < 0 || rawNoteF > committed
        const candidate = GestureEngine.quantizeNote(Math.round(rawNoteF), sc, movingUp)

        if (candidate !== committed) {
          const kClearance = 0.35
          if (committed >= 0) {
            const mid = (committed + candidate) * 0.5
            const crossedClearly =
              (candidate > committed && rawNoteF >= mid + kClearance) ||
              (candidate < committed && rawNoteF <= mid - kClearance)
            if (!crossedClearly) break
          }
          if (committed >= 0)
            midiOut(0x80 | (snap.midiChannel & 0x0f), committed, 0)
          midiOut(0x90 | (snap.midiChannel & 0x0f), candidate, snap.noteVelocity)
          rt.lastSentValue = candidate
        }
        break
      }
    }
  }
}
