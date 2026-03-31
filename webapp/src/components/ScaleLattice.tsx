import { NOTE_NAMES, QUICK_PRESETS, SCALE_FAMILIES } from '../engine/scaleData'
import { type ScaleConfig } from '../engine/types'

interface ScaleLatticeProps {
  config: ScaleConfig
  onChange: (config: ScaleConfig) => void
  theme: 'light' | 'dark'
}

export function ScaleLattice({ config, onChange, theme }: ScaleLatticeProps) {
  const dark = theme === 'dark'

  const toggleInterval = (interval: number) => {
    const bit = 1 << (11 - interval)
    // Don't allow removing the root (interval 0)
    if (interval === 0) return
    const newMask = config.mask ^ bit
    onChange({ ...config, mask: newMask & 0xfff })
  }

  const setRoot = (root: number) => {
    onChange({ ...config, root })
  }

  const setPreset = (mask: number) => {
    onChange({ ...config, mask })
  }

  const isActive = (interval: number) => {
    return !!((config.mask >> (11 - interval)) & 1)
  }

  const circleStyle = (interval: number): React.CSSProperties => {
    const active = isActive(interval)
    const isRoot = interval === 0
    const noteIdx = (config.root + interval) % 12
    const isCurrentRoot = noteIdx === config.root && interval === 0

    return {
      width: 28,
      height: 28,
      borderRadius: '50%',
      border: `2px solid ${active ? (dark ? '#4a90e2' : '#1a60c8') : (dark ? '#444' : '#ccc')}`,
      background: active
        ? isRoot || isCurrentRoot
          ? dark ? '#4a90e2' : '#1a60c8'
          : dark ? 'rgba(74,144,226,0.35)' : 'rgba(26,96,200,0.2)'
        : 'transparent',
      cursor: interval === 0 ? 'default' : 'pointer',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      fontSize: 9,
      fontWeight: 600,
      color: active ? (dark ? '#e0e0e0' : '#1a1a1a') : (dark ? '#555' : '#bbb'),
      userSelect: 'none',
      flexShrink: 0,
      transition: 'background 0.1s, border-color 0.1s',
    }
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
      {/* Quick preset buttons */}
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4 }}>
        {QUICK_PRESETS.map(p => (
          <button
            key={p.name}
            onClick={() => setPreset(p.mask)}
            style={{
              padding: '3px 8px',
              fontSize: 11,
              borderRadius: 4,
              border: `1px solid ${config.mask === p.mask ? (dark ? '#4a90e2' : '#1a60c8') : (dark ? '#444' : '#ccc')}`,
              background:
                config.mask === p.mask
                  ? dark ? 'rgba(74,144,226,0.25)' : 'rgba(26,96,200,0.12)'
                  : 'transparent',
              color: config.mask === p.mask
                ? dark ? '#4a90e2' : '#1a60c8'
                : dark ? '#aaa' : '#666',
              cursor: 'pointer',
              fontWeight: config.mask === p.mask ? 700 : 400,
            }}
          >
            {p.name}
          </button>
        ))}
      </div>

      {/* 12-note lattice */}
      <div style={{ display: 'flex', gap: 4, alignItems: 'center', flexWrap: 'wrap' }}>
        {Array.from({ length: 12 }, (_, interval) => {
          const noteIdx = (config.root + interval) % 12
          return (
            <div key={interval} style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 2 }}>
              <div
                style={circleStyle(interval)}
                onClick={() => toggleInterval(interval)}
              >
                {interval === 0 ? '●' : ''}
              </div>
              <span style={{ fontSize: 9, color: dark ? '#888' : '#888', lineHeight: 1 }}>
                {NOTE_NAMES[noteIdx]}
              </span>
            </div>
          )
        })}
      </div>

      {/* Root note selector */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <span style={{ fontSize: 11, color: dark ? '#888' : '#888', minWidth: 30 }}>Root</span>
        <select
          value={config.root}
          onChange={e => setRoot(Number(e.target.value))}
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

      {/* Scale family browser */}
      <ScaleFamilyPicker config={config} onChange={onChange} theme={theme} />
    </div>
  )
}

function ScaleFamilyPicker({
  config,
  onChange,
  theme,
}: {
  config: ScaleConfig
  onChange: (c: ScaleConfig) => void
  theme: 'light' | 'dark'
}) {
  const dark = theme === 'dark'
  return (
    <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
      <select
        defaultValue=""
        value=""
        onChange={e => {
          if (!e.target.value) return
          const [f, m] = e.target.value.split(':').map(Number)
          const entry = SCALE_FAMILIES[f]?.modes[m]
          if (entry) onChange({ ...config, mask: entry.mask })
          e.target.value = ""
        }}
        style={{
          background: dark ? '#2a2a2a' : '#fff',
          color: dark ? '#e0e0e0' : '#1a1a1a',
          border: `1px solid ${dark ? '#444' : '#ccc'}`,
          borderRadius: 4,
          padding: '2px 6px',
          fontSize: 11,
          flex: 1,
        }}
      >
        <option value="">Browse scales…</option>
        {SCALE_FAMILIES.map((fam, fi) => (
          <optgroup key={fi} label={fam.name}>
            {fam.modes.map((mode, mi) => (
              <option key={mi} value={`${fi}:${mi}`}>{mode.name}</option>
            ))}
          </optgroup>
        ))}
      </select>
    </div>
  )
}
