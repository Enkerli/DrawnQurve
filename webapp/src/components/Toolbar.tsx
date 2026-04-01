import { PlaybackDirection } from '../engine/types'
import { type MidiPort } from '../midi/webMidi'

const SPEED_OPTIONS = [
  { label: '¼×', value: 0.25 },
  { label: '½×', value: 0.5 },
  { label: '1×', value: 1 },
  { label: '2×', value: 2 },
  { label: '4×', value: 4 },
]

interface ToolbarProps {
  speedRatio: number
  direction: PlaybackDirection
  theme: 'light' | 'dark'
  midiPorts: MidiPort[]
  selectedMidiPort: string | null
  midiSupported: boolean
  onSpeedChange: (speed: number) => void
  onDirectionChange: (dir: PlaybackDirection) => void
  onThemeToggle: () => void
  onClearAll: () => void
  onPanic: () => void
  onRequestMidi: () => void
  onSelectMidiPort: (id: string | null) => void
}

export function Toolbar({
  speedRatio,
  direction,
  theme,
  midiPorts,
  selectedMidiPort,
  midiSupported,
  onSpeedChange,
  onDirectionChange,
  onThemeToggle,
  onClearAll,
  onPanic,
  onRequestMidi,
  onSelectMidiPort,
}: ToolbarProps) {
  const dark = theme === 'dark'

  const btnBase: React.CSSProperties = {
    padding: '5px 10px',
    borderRadius: 5,
    border: `1px solid ${dark ? '#444' : '#ccc'}`,
    background: 'transparent',
    color: dark ? '#ccc' : '#555',
    cursor: 'pointer',
    fontSize: 12,
    fontWeight: 500,
    whiteSpace: 'nowrap',
    display: 'flex',
    alignItems: 'center',
    gap: 4,
  }

  const activeBtnStyle = (active: boolean, accent?: string): React.CSSProperties => ({
    ...btnBase,
    border: `1px solid ${active ? (accent ?? (dark ? '#4a90e2' : '#1a60c8')) : (dark ? '#444' : '#ccc')}`,
    background: active
      ? dark ? 'rgba(74,144,226,0.2)' : 'rgba(26,96,200,0.1)'
      : 'transparent',
    color: active ? (accent ?? (dark ? '#4a90e2' : '#1a60c8')) : (dark ? '#ccc' : '#555'),
    fontWeight: active ? 700 : 500,
  })

  const selectStyle: React.CSSProperties = {
    background: dark ? '#2a2a2a' : '#fff',
    color: dark ? '#e0e0e0' : '#1a1a1a',
    border: `1px solid ${dark ? '#444' : '#ccc'}`,
    borderRadius: 5,
    padding: '5px 8px',
    fontSize: 12,
    cursor: 'pointer',
  }

  const dividerStyle: React.CSSProperties = {
    width: 1,
    height: 24,
    background: dark ? '#333' : '#ddd',
    flexShrink: 0,
  }

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: 8,
        padding: '8px 16px',
        borderBottom: `1px solid ${dark ? '#2a2a2a' : '#e0e0e0'}`,
        flexWrap: 'wrap',
        flexShrink: 0,
      }}
    >
      {/* Logo */}
      <span
        style={{
          fontWeight: 700,
          fontSize: 15,
          letterSpacing: '-0.5px',
          color: dark ? '#e0e0e0' : '#1a1a1a',
          marginRight: 4,
          fontStyle: 'italic',
        }}
      >
        DrawnQurve
      </span>

      <div style={dividerStyle} />

      {/* Speed */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
        <span style={{ fontSize: 11, color: dark ? '#888' : '#888' }}>Speed</span>
        {SPEED_OPTIONS.map(opt => (
          <button
            key={opt.value}
            onClick={() => onSpeedChange(opt.value)}
            style={activeBtnStyle(speedRatio === opt.value)}
          >
            {opt.label}
          </button>
        ))}
      </div>

      <div style={dividerStyle} />

      {/* Direction */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
        <span style={{ fontSize: 11, color: dark ? '#888' : '#888' }}>Dir</span>
        <button
          onClick={() => onDirectionChange(PlaybackDirection.Reverse)}
          style={activeBtnStyle(direction === PlaybackDirection.Reverse)}
          title="Reverse"
        >
          ◀
        </button>
        <button
          onClick={() => onDirectionChange(PlaybackDirection.PingPong)}
          style={activeBtnStyle(direction === PlaybackDirection.PingPong)}
          title="Ping-Pong"
        >
          ↔
        </button>
        <button
          onClick={() => onDirectionChange(PlaybackDirection.Forward)}
          style={activeBtnStyle(direction === PlaybackDirection.Forward)}
          title="Forward"
        >
          ▶
        </button>
      </div>

      <div style={dividerStyle} />

      {/* Clear + Panic */}
      <button onClick={onClearAll} style={btnBase}>
        Clear All
      </button>
      <button
        onClick={onPanic}
        style={{ ...btnBase, color: '#e0593a', borderColor: dark ? '#444' : '#ccc' }}
        title="All Notes Off"
      >
        Panic
      </button>

      <div style={{ flex: 1 }} />

      {/* MIDI output */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
        {!midiSupported ? (
          <span style={{ fontSize: 11, color: '#e0593a' }}>No Web MIDI</span>
        ) : midiPorts.length === 0 ? (
          <button onClick={onRequestMidi} style={btnBase}>
            🎹 Enable MIDI
          </button>
        ) : (
          <>
            <span style={{ fontSize: 11, color: dark ? '#888' : '#888' }}>MIDI Out</span>
            <select
              value={selectedMidiPort ?? ''}
              onChange={e => onSelectMidiPort(e.target.value || null)}
              style={selectStyle}
            >
              <option value="">— None —</option>
              {midiPorts.map(p => (
                <option key={p.id} value={p.id}>{p.name}</option>
              ))}
            </select>
          </>
        )}
        <span
          style={{
            width: 8,
            height: 8,
            borderRadius: '50%',
            background: selectedMidiPort ? '#5cb85c' : (dark ? '#333' : '#ccc'),
            flexShrink: 0,
          }}
        />
      </div>

      <div style={dividerStyle} />

      {/* Theme toggle */}
      <button onClick={onThemeToggle} style={btnBase} title="Toggle theme">
        {dark ? '☀︎' : '☾'}
      </button>
    </div>
  )
}
