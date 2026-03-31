/** Web MIDI API wrapper with output port management. */

export interface MidiPort {
  id: string
  name: string
}

export class WebMidiManager {
  private access: MIDIAccess | null = null
  private selectedOutputId: string | null = null
  private onPortsChanged?: (ports: MidiPort[]) => void

  get isSupported(): boolean {
    return typeof navigator !== 'undefined' && 'requestMIDIAccess' in navigator
  }

  async requestAccess(): Promise<MidiPort[]> {
    if (!this.isSupported) throw new Error('Web MIDI API not supported in this browser.')

    this.access = await navigator.requestMIDIAccess({ sysex: false })
    this.access.onstatechange = () => {
      this.onPortsChanged?.(this.getOutputPorts())
    }
    return this.getOutputPorts()
  }

  getOutputPorts(): MidiPort[] {
    if (!this.access) return []
    const ports: MidiPort[] = []
    this.access.outputs.forEach(output => {
      ports.push({ id: output.id, name: output.name ?? `Output ${output.id}` })
    })
    return ports
  }

  setSelectedOutput(id: string | null): void {
    this.selectedOutputId = id
  }

  getSelectedOutputId(): string | null {
    return this.selectedOutputId
  }

  onPortsChange(cb: (ports: MidiPort[]) => void): void {
    this.onPortsChanged = cb
  }

  /** Send a raw MIDI message to the selected output. */
  send(status: number, data1: number, data2: number): void {
    if (!this.access || !this.selectedOutputId) return
    const output = this.access.outputs.get(this.selectedOutputId)
    if (!output) return

    // Channel pressure and pitch bend have special byte layouts
    const statusByte = status & 0xf0
    if (statusByte === 0xd0) {
      // Channel pressure: 2 bytes
      output.send([status, data1])
    } else {
      output.send([status, data1, data2])
    }
  }

  /** Send All Notes Off + All Sound Off on all channels. */
  panic(): void {
    if (!this.access || !this.selectedOutputId) return
    const output = this.access.outputs.get(this.selectedOutputId)
    if (!output) return
    for (let ch = 0; ch < 16; ch++) {
      output.send([0xb0 | ch, 123, 0]) // All Notes Off
      output.send([0xb0 | ch, 120, 0]) // All Sound Off
    }
  }
}
