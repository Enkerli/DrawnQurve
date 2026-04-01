import { useState } from 'react'
import { NOTE_NAMES, SCALE_FAMILIES } from '../engine/scaleData'
import { type ScaleConfig } from '../engine/types'

interface ScaleLatticeProps {
  config: ScaleConfig
  onChange: (config: ScaleConfig) => void
  theme: 'light' | 'dark'
}

// Piano keyboard layout for pitch classes 0 (C) … 11 (B).
// xFrac: x-center as a fraction of the container width.
// black: true = accidental (top row), false = natural (bottom row).
const W = 1 / 7
const KEY_LAYOUT: { xFrac: number; black: boolean }[] = [
  { xFrac: 0.5 * W, black: false }, // C
  { xFrac: 1.0 * W, black: true  }, // C♯
  { xFrac: 1.5 * W, black: false }, // D
  { xFrac: 2.0 * W, black: true  }, // D♯
  { xFrac: 2.5 * W, black: false }, // E
  { xFrac: 3.5 * W, black: false }, // F
  { xFrac: 4.0 * W, black: true  }, // F♯
  { xFrac: 4.5 * W, black: false }, // G
  { xFrac: 5.0 * W, black: true  }, // G♯
  { xFrac: 5.5 * W, black: false }, // A
  { xFrac: 6.0 * W, black: true  }, // A♯
  { xFrac: 6.5 * W, black: false }, // B
]

const CD = 22          // circle diameter (px)
const BLK_H = 28       // height of accidental row
const WHT_H = 28       // height of natural row
const LBL_H = 12       // label row below naturals
const PIANO_H = BLK_H + WHT_H + LBL_H

export function ScaleLattice({ config, onChange, theme }: ScaleLatticeProps) {
  const dark = theme === 'dark'

  // Pre-select the family matching the current mask, if any
  const [familyIdx, setFamilyIdx] = useState<number | null>(() => {
    for (let fi = 0; fi < SCALE_FAMILIES.length; fi++) {
      if (SCALE_FAMILIES[fi].modes.some(m => m.mask === config.mask)) return fi
    }
    return null
  })

  const isActive = (interval: number) => !!((config.mask >> (11 - interval)) & 1)

  const togglePc = (pc: number) => {
    const interval = (pc - config.root + 12) % 12
    if (interval === 0) return  // root is always on
    const bit = 1 << (11 - interval)
    onChange({ ...config, mask: (config.mask ^ bit) & 0xfff })
  }

  const btnBase: React.CSSProperties = {
    padding: '3px 7px',
    fontSize: 10,
    borderRadius: 4,
    border: `1px solid ${dark ? '#444' : '#ccc'}`,
    background: 'transparent',
    color: dark ? '#aaa' : '#666',
    cursor: 'pointer',
    fontWeight: 400,
    whiteSpace: 'nowrap',
  }

  const activeBtnStyle = (active: boolean): React.CSSProperties => ({
    ...btnBase,
    border: `1px solid ${active ? (dark ? '#4a90e2' : '#1a60c8') : (dark ? '#444' : '#ccc')}`,
    background: active ? (dark ? 'rgba(74,144,226,0.2)' : 'rgba(26,96,200,0.1)') : 'transparent',
    color: active ? (dark ? '#4a90e2' : '#1a60c8') : (dark ? '#aaa' : '#666'),
    fontWeight: active ? 700 : 400,
  })

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>

      {/* Piano-style two-row lattice */}
      <div style={{ position: 'relative', height: PIANO_H, width: '100%' }}>
        {Array.from({ length: 12 }, (_, pc) => {
          const { xFrac, black } = KEY_LAYOUT[pc]
          const interval = (pc - config.root + 12) % 12
          const active = isActive(interval)
          const isRoot = pc === config.root
          const noteLabel = NOTE_NAMES[pc]

          const top = black
            ? (BLK_H - CD) / 2
            : BLK_H + (WHT_H - CD) / 2

          const circleColor = isRoot
            ? (dark ? '#4a90e2' : '#1a60c8')
            : active
              ? dark ? 'rgba(74,144,226,0.35)' : 'rgba(26,96,200,0.22)'
              : 'transparent'

          const borderColor = active
            ? (dark ? '#4a90e2' : '#1a60c8')
            : (dark ? '#444' : '#ccc')

          return (
            <div key={pc}>
              {/* Circle */}
              <div
                onClick={() => togglePc(pc)}
                style={{
                  position: 'absolute',
                  left: `calc(${xFrac * 100}% - ${CD / 2}px)`,
                  top,
                  width: CD,
                  height: CD,
                  borderRadius: '50%',
                  border: `2px solid ${borderColor}`,
                  background: circleColor,
                  cursor: interval === 0 ? 'default' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontSize: black ? 7 : 8,
                  fontWeight: 600,
                  color: active ? (dark ? '#e0e0e0' : '#111') : (dark ? '#555' : '#bbb'),
                  userSelect: 'none',
                  transition: 'background 0.1s, border-color 0.1s',
                  zIndex: black ? 2 : 1,
                }}
                title={noteLabel}
              >
                {black ? noteLabel : ''}
              </div>

              {/* Label below naturals */}
              {!black && (
                <span
                  style={{
                    position: 'absolute',
                    left: `calc(${xFrac * 100}% - 10px)`,
                    top: BLK_H + WHT_H,
                    width: 20,
                    textAlign: 'center',
                    fontSize: 8,
                    color: active ? (dark ? '#4a90e2' : '#1a60c8') : (dark ? '#666' : '#aaa'),
                    fontWeight: active ? 700 : 400,
                    userSelect: 'none',
                  }}
                >
                  {noteLabel}
                </span>
              )}
            </div>
          )
        })}
      </div>

      {/* Root note selector */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <span style={{ fontSize: 11, color: dark ? '#888' : '#888', minWidth: 30 }}>Root</span>
        <select
          value={config.root}
          onChange={e => onChange({ ...config, root: Number(e.target.value) })}
          style={{
            background: dark ? '#2a2a2a' : '#fff',
            color: dark ? '#e0e0e0' : '#1a1a1a',
            border: `1px solid ${dark ? '#444' : '#ccc'}`,
            borderRadius: 4,
            padding: '2px 6px',
            fontSize: 12,
          }}
        >
          {NOTE_NAMES.map((n, i) => (
            <option key={i} value={i}>{n}</option>
          ))}
        </select>
      </div>

      {/* Two-level scale family picker */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
        {/* Family row */}
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 3 }}>
          {SCALE_FAMILIES.map((fam, fi) => (
            <button
              key={fi}
              onClick={() => setFamilyIdx(fi === familyIdx ? null : fi)}
              style={activeBtnStyle(fi === familyIdx)}
            >
              {fam.name}
            </button>
          ))}
        </div>

        {/* Mode row */}
        {familyIdx !== null && (
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: 3 }}>
            {SCALE_FAMILIES[familyIdx].modes.map((mode, mi) => (
              <button
                key={mi}
                onClick={() => onChange({ ...config, mask: mode.mask })}
                style={activeBtnStyle(config.mask === mode.mask)}
              >
                {mode.name}
              </button>
            ))}
          </div>
        )}
      </div>

    </div>
  )
}
