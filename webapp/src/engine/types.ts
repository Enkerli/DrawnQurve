// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

export enum MessageType {
  CC = 0,             // Control Change (0xB0)
  ChannelPressure = 1, // Channel Pressure / Aftertouch (0xD0)
  PitchBend = 2,      // Pitch Bend (0xE0)
  Note = 3,           // Note On/Off (0x90 / 0x80)
}

export enum PlaybackDirection {
  Forward = 0,
  Reverse = 1,
  PingPong = 2,
}

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

/** 12-bit scale bitmask + root note.
 *  Bit (11 - interval) = 1 means that interval is in the scale.
 *  0xFFF = chromatic = no quantization.
 */
export interface ScaleConfig {
  mask: number  // 12-bit integer
  root: number  // 0=C … 11=B
}

/** Immutable curve snapshot — created on gesture finalize, fed to engine. */
export interface LaneSnapshot {
  table: Float32Array        // 256 normalised curve samples [0, 1]
  durationSeconds: number   // original gesture duration
  ccNumber: number          // 0-127
  midiChannel: number       // 0-indexed (0 = ch 1)
  minOut: number            // output range lower bound [0, 1]
  maxOut: number            // output range upper bound [0, 1]
  smoothing: number         // time constant (0 = off, ~0.08 = light)
  messageType: MessageType
  noteVelocity: number      // 1-127, used in Note mode
  phaseOffset: number       // [0, 1) shifts start position
  valid: boolean
}

export function defaultSnapshot(): LaneSnapshot {
  return {
    table: new Float32Array(256).fill(0.5),
    durationSeconds: 1,
    ccNumber: 74,
    midiChannel: 0,
    minOut: 0,
    maxOut: 1,
    smoothing: 0.08,
    messageType: MessageType.CC,
    noteVelocity: 100,
    phaseOffset: 0,
    valid: false,
  }
}

/** Per-lane UI parameters (separate from snapshot so UI can update without re-capture). */
export interface LaneParams {
  messageType: MessageType
  ccNumber: number      // 0-127
  midiChannel: number   // 0-15
  minOut: number        // 0-1
  maxOut: number        // 0-1
  smoothing: number     // 0 = off
  noteVelocity: number  // 1-127
  scaleConfig: ScaleConfig
  enabled: boolean      // false = muted
}

export function defaultLaneParams(ccNumber = 74): LaneParams {
  return {
    messageType: MessageType.Note,
    ccNumber,
    midiChannel: 0,
    minOut: 0,
    maxOut: 1,
    smoothing: 0.08,
    noteVelocity: 100,
    scaleConfig: { mask: 0xad5, root: 0 },  // C Major
    enabled: true,
  }
}

export type MidiOutFn = (status: number, data1: number, data2: number) => void
