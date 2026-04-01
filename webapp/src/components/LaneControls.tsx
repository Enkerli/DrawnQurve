import { MessageType, type LaneParams } from '../engine/types'
import { ScaleLattice } from './ScaleLattice'

const LANE_LABELS = ['Lane 1', 'Lane 2', 'Lane 3']
const LANE_COLORS_DARK = ['#4a90e2', '#e8a838', '#5cb85c']
const LANE_COLORS_LIGHT = ['#1a60c8', '#c87010', '#228b22']

interface LaneControlsProps {
  laneParams: LaneParams[]
  focusedLane: number
  hasSnapshot: boolean[]
  theme: 'light' | 'dark'
  onFocusLane: (lane: number) => void
  onUpdateParams: (lane: number, params: Partial<LaneParams>) => void
  onClearLane: (lane: number) => void
}

export function LaneControls({
  laneParams,
  focusedLane,
  hasSnapshot,
  theme,
  onFocusLane,
  onUpdateParams,
  onClearLane,
}: LaneControlsProps) {
  const dark = theme === 'dark'
  const params = laneParams[focusedLane]
  const laneColors = dark ? LANE_COLORS_DARK : LANE_COLORS_LIGHT
  const color = laneColors[focusedLane]

  const update = (partial: Partial<LaneParams>) => onUpdateParams(focusedLane, partial)

  const inputStyle: React.CSSProperties = {
    background: dark ? '#2a2a2a' : '#fff',
    color: dark ? '#e0e0e0' : '#1a1a1a',
    border: `1px solid ${dark ? '#444' : '#ccc'}`,
    borderRadius: 4,
    padding: '3px 6px',
    fontSize: 12,
    width: '100%',
    boxSizing: 'border-box',
  }

  const labelStyle: React.CSSProperties = {
    fontSize: 11,
    color: dark ? '#888' : '#888',
    marginBottom: 2,
    display: 'block',
  }

  const rowStyle: React.CSSProperties = {
    display: 'flex',
    alignItems: 'center',
    gap: 8,
  }

  const sectionStyle: React.CSSProperties = {
    display: 'flex',
    flexDirection: 'column',
    gap: 10,
  }

  const dividerStyle: React.CSSProperties = {
    borderTop: `1px solid ${dark ? '#333' : '#e0e0e0'}`,
    margin: '4px 0',
  }

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        gap: 12,
        padding: 16,
        overflowY: 'auto',
        height: '100%',
        boxSizing: 'border-box',
      }}
    >
      {/* Lane selector tabs */}
      <div style={{ display: 'flex', gap: 4 }}>
        {[0, 1, 2].map(lane => (
          <button
            key={lane}
            onClick={() => onFocusLane(lane)}
            style={{
              flex: 1,
              padding: '6px 4px',
              borderRadius: 6,
              border: `2px solid ${lane === focusedLane ? laneColors[lane] : (dark ? '#333' : '#ddd')}`,
              background:
                lane === focusedLane
                  ? dark ? 'rgba(74,144,226,0.15)' : 'rgba(26,96,200,0.08)'
                  : 'transparent',
              color: lane === focusedLane ? laneColors[lane] : (dark ? '#666' : '#999'),
              cursor: 'pointer',
              fontSize: 12,
              fontWeight: lane === focusedLane ? 700 : 400,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: 4,
              transition: 'all 0.15s',
            }}
          >
            <span
              style={{
                width: 7,
                height: 7,
                borderRadius: '50%',
                background: hasSnapshot[lane] ? laneColors[lane] : (dark ? '#333' : '#ddd'),
                flexShrink: 0,
              }}
            />
            {LANE_LABELS[lane]}
          </button>
        ))}
      </div>

      <div style={dividerStyle} />

      {/* Message type */}
      <div style={sectionStyle}>
        <span style={labelStyle}>Type</span>
        <div style={{ display: 'flex', gap: 4 }}>
          {(['CC', 'AT', 'PB', '♩'] as const).map((label, i) => (
            <button
              key={label}
              onClick={() => update({ messageType: i as MessageType })}
              style={{
                flex: 1,
                padding: '5px 2px',
                borderRadius: 5,
                border: `1px solid ${params.messageType === i ? color : (dark ? '#444' : '#ccc')}`,
                background:
                  params.messageType === i
                    ? dark ? 'rgba(74,144,226,0.2)' : 'rgba(26,96,200,0.12)'
                    : 'transparent',
                color: params.messageType === i ? color : (dark ? '#aaa' : '#777'),
                cursor: 'pointer',
                fontSize: 12,
                fontWeight: params.messageType === i ? 700 : 400,
              }}
            >
              {label}
            </button>
          ))}
        </div>
      </div>

      <div style={dividerStyle} />

      {/* CC-specific controls */}
      {params.messageType === MessageType.CC && (
        <div style={sectionStyle}>
          <div style={rowStyle}>
            <div style={{ flex: 1 }}>
              <label style={labelStyle}>CC #</label>
              <input
                type="number"
                min={0}
                max={127}
                value={params.ccNumber}
                onChange={e => update({ ccNumber: Math.max(0, Math.min(127, Number(e.target.value))) })}
                style={inputStyle}
              />
            </div>
            <div style={{ flex: 1 }}>
              <label style={labelStyle}>Channel</label>
              <select
                value={params.midiChannel}
                onChange={e => update({ midiChannel: Number(e.target.value) })}
                style={inputStyle}
              >
                {Array.from({ length: 16 }, (_, i) => (
                  <option key={i} value={i}>Ch {i + 1}</option>
                ))}
              </select>
            </div>
          </div>
        </div>
      )}

      {/* AT / PB channel */}
      {(params.messageType === MessageType.ChannelPressure ||
        params.messageType === MessageType.PitchBend) && (
        <div style={sectionStyle}>
          <label style={labelStyle}>Channel</label>
          <select
            value={params.midiChannel}
            onChange={e => update({ midiChannel: Number(e.target.value) })}
            style={inputStyle}
          >
            {Array.from({ length: 16 }, (_, i) => (
              <option key={i} value={i}>Ch {i + 1}</option>
            ))}
          </select>
        </div>
      )}

      {/* Note mode controls */}
      {params.messageType === MessageType.Note && (
        <div style={sectionStyle}>
          <div style={rowStyle}>
            <div style={{ flex: 1 }}>
              <label style={labelStyle}>Channel</label>
              <select
                value={params.midiChannel}
                onChange={e => update({ midiChannel: Number(e.target.value) })}
                style={inputStyle}
              >
                {Array.from({ length: 16 }, (_, i) => (
                  <option key={i} value={i}>Ch {i + 1}</option>
                ))}
              </select>
            </div>
            <div style={{ flex: 1 }}>
              <label style={labelStyle}>Velocity</label>
              <input
                type="number"
                min={1}
                max={127}
                value={params.noteVelocity}
                onChange={e => update({ noteVelocity: Math.max(1, Math.min(127, Number(e.target.value))) })}
                style={inputStyle}
              />
            </div>
          </div>
        </div>
      )}

      <div style={dividerStyle} />

      {/* Output range */}
      <div style={sectionStyle}>
        <span style={labelStyle}>
          Range: {Math.round(params.minOut * 100)}% – {Math.round(params.maxOut * 100)}%
        </span>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
          <div style={rowStyle}>
            <span style={{ ...labelStyle, marginBottom: 0, width: 28 }}>Min</span>
            <input
              type="range"
              min={0}
              max={100}
              value={Math.round(params.minOut * 100)}
              onChange={e => {
                const v = Number(e.target.value) / 100
                update({ minOut: Math.min(v, params.maxOut - 0.01) })
              }}
              style={{ flex: 1, accentColor: color }}
            />
          </div>
          <div style={rowStyle}>
            <span style={{ ...labelStyle, marginBottom: 0, width: 28 }}>Max</span>
            <input
              type="range"
              min={0}
              max={100}
              value={Math.round(params.maxOut * 100)}
              onChange={e => {
                const v = Number(e.target.value) / 100
                update({ maxOut: Math.max(v, params.minOut + 0.01) })
              }}
              style={{ flex: 1, accentColor: color }}
            />
          </div>
        </div>
      </div>

      {/* Smoothing */}
      <div style={sectionStyle}>
        <span style={labelStyle}>Smooth: {params.smoothing.toFixed(3)}</span>
        <input
          type="range"
          min={0}
          max={50}
          value={Math.round(params.smoothing * 100)}
          onChange={e => update({ smoothing: Number(e.target.value) / 100 })}
          style={{ accentColor: color }}
        />
      </div>

      <div style={dividerStyle} />

      {/* Mute + Clear */}
      <div style={rowStyle}>
        <button
          onClick={() => update({ enabled: !params.enabled })}
          style={{
            flex: 1,
            padding: '6px 0',
            borderRadius: 5,
            border: `1px solid ${!params.enabled ? '#e0593a' : (dark ? '#444' : '#ccc')}`,
            background: !params.enabled ? 'rgba(224,89,58,0.15)' : 'transparent',
            color: !params.enabled ? '#e0593a' : (dark ? '#aaa' : '#777'),
            cursor: 'pointer',
            fontSize: 12,
            fontWeight: 600,
          }}
        >
          {params.enabled ? 'Mute' : 'Unmute'}
        </button>
        <button
          onClick={() => onClearLane(focusedLane)}
          style={{
            flex: 1,
            padding: '6px 0',
            borderRadius: 5,
            border: `1px solid ${dark ? '#444' : '#ccc'}`,
            background: 'transparent',
            color: dark ? '#aaa' : '#777',
            cursor: 'pointer',
            fontSize: 12,
          }}
        >
          Clear
        </button>
      </div>

      {/* Scale lattice (Note mode only) */}
      {params.messageType === MessageType.Note && (
        <>
          <div style={dividerStyle} />
          <div style={sectionStyle}>
            <span style={{ ...labelStyle, marginBottom: 4 }}>Scale</span>
            <ScaleLattice
              config={params.scaleConfig}
              onChange={sc => update({ scaleConfig: sc })}
              theme={theme}
            />
          </div>
        </>
      )}
    </div>
  )
}
