// ---------------------------------------------------------------------------
// Scale database — direct port of ScaleData.h
//
// Bitmask convention: bit (11 - interval) = 1 → interval is in the scale.
// bit 11 = root (interval 0), bit 0 = major 7th (interval 11).
// ---------------------------------------------------------------------------

export interface ScaleEntry {
  name: string
  mask: number
}

export interface ScaleFamily {
  name: string
  modes: ScaleEntry[]
}

export const SCALE_FAMILIES: ScaleFamily[] = [
  {
    name: 'Diatonic',
    modes: [
      { name: 'Ionian',      mask: 0xad5 },  // 0 2 4 5 7 9 11 — Major
      { name: 'Dorian',      mask: 0xb56 },  // 0 2 3 5 7 9 10
      { name: 'Phrygian',    mask: 0xd5a },  // 0 1 3 5 7 8 10
      { name: 'Lydian',      mask: 0xab5 },  // 0 2 4 6 7 9 11
      { name: 'Mixolydian',  mask: 0xad6 },  // 0 2 4 5 7 9 10
      { name: 'Aeolian',     mask: 0xb5a },  // 0 2 3 5 7 8 10 — Natural Minor
      { name: 'Locrian',     mask: 0xd6a },  // 0 1 3 5 6 8 10
    ],
  },
  {
    name: 'Pentatonic',
    modes: [
      { name: 'Major',       mask: 0xa94 },  // 0 2 4 7 9
      { name: 'Suspended',   mask: 0xa52 },  // 0 2 5 7 10
      { name: 'Man Gong',    mask: 0x94a },  // 0 3 5 8 10
      { name: 'Ritusen',     mask: 0xa54 },  // 0 2 5 7 9
      { name: 'Minor',       mask: 0x952 },  // 0 3 5 7 10
    ],
  },
  {
    name: 'Jazz Minor',
    modes: [
      { name: 'Jazz Minor',       mask: 0xb55 },
      { name: 'Dorian ♭2',        mask: 0xd56 },
      { name: 'Lydian Aug.',       mask: 0xaad },
      { name: 'Lydian Dom.',       mask: 0xab6 },
      { name: 'Mixo. ♭6',         mask: 0xada },
      { name: 'Half-Dim.',         mask: 0xb6a },
      { name: 'Altered',           mask: 0xdaa },
    ],
  },
  {
    name: 'Harm. Minor',
    modes: [
      { name: 'Harmonic Minor',    mask: 0xb59 },
      { name: 'Locrian ♮6',        mask: 0xd66 },
      { name: 'Ionian ♯5',         mask: 0xacd },
      { name: 'Ukrainian Dor.',    mask: 0xb36 },
      { name: 'Phrygian Dom.',     mask: 0xcda },
      { name: 'Lydian ♯2',         mask: 0x9b5 },
      { name: 'Ultra Locrian',     mask: 0xdac },
    ],
  },
  {
    name: 'Symmetric',
    modes: [
      { name: 'Whole Tone',        mask: 0xaaa },
      { name: 'Dim. WH',           mask: 0xb6d },
      { name: 'Dim. HW',           mask: 0xdb6 },
      { name: 'Augmented',         mask: 0x999 },
    ],
  },
  {
    name: 'Bebop',
    modes: [
      { name: 'Dominant',   mask: 0xad7 },
      { name: 'Major',      mask: 0xadd },
      { name: 'Minor',      mask: 0xb5b },
      { name: 'Mel. Minor', mask: 0xb57 },
    ],
  },
  {
    name: 'Blues',
    modes: [
      { name: 'Blues',       mask: 0x972 },
      { name: 'Major Blues', mask: 0xb94 },
    ],
  },
  {
    name: 'Chordal',
    modes: [
      { name: 'Major',      mask: 0x890 },
      { name: 'Minor',      mask: 0x910 },
      { name: 'Diminished', mask: 0x920 },
      { name: 'Augmented',  mask: 0x888 },
      { name: 'Sus 2',      mask: 0xa10 },
      { name: 'Sus 4',      mask: 0x850 },
      { name: 'Maj 7',      mask: 0x891 },
      { name: 'Min 7',      mask: 0x912 },
      { name: 'Dom 7',      mask: 0x892 },
      { name: 'Half-Dim 7', mask: 0x922 },
      { name: 'Dim 7',      mask: 0x924 },
    ],
  },
]

export const NOTE_NAMES = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B']

/** Quick presets shown in the UI above the lattice. */
export const QUICK_PRESETS: { name: string; mask: number }[] = [
  { name: 'Chrom.', mask: 0xfff },
  { name: 'Major',  mask: 0xad5 },
  { name: 'Minor',  mask: 0xb5a },
  { name: 'Penta',  mask: 0xa94 },
  { name: 'Blues',  mask: 0x972 },
  { name: 'WTone',  mask: 0xaaa },
]

/** Find a mask in the database; returns family/mode name or null. */
export function recogniseMask(mask: number): string | null {
  if (mask === 0xfff) return 'Chromatic'
  for (const family of SCALE_FAMILIES) {
    for (const mode of family.modes) {
      if (mode.mask === mask) return `${family.name} / ${mode.name}`
    }
  }
  return null
}

/** Rotate a 12-bit mask left by semitones (for mode transposition). */
export function pcsRotate(mask: number, semitones: number): number {
  semitones = ((semitones % 12) + 12) % 12
  if (semitones === 0) return mask
  return ((mask << semitones) | (mask >> (12 - semitones))) & 0xfff
}
