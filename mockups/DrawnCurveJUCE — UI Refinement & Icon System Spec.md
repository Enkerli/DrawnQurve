# DrawnCurveJUCE — UI Refinement & Icon System Spec

Purpose

This document defines UI refinements for DrawnCurveJUCE, focusing on:
	•	Semantic clarity
	•	Reduction of cognitive load
	•	Consistent iconography
	•	Stronger “instrument-like” interaction model
	•	Explicit lane-focused workflow

This is intended as a direct implementation guide for JUCE development.

⸻

1. Core Design Principle

The plugin is a drawable modulation instrument, not a parameter editor.

Primary user flow:
	1.	Select lane
	2.	Draw gesture
	3.	Route output
	4.	Refine behavior

All UI decisions should reinforce this sequence.

⸻

2. Critical Fixes (Priority 0)

2.1 Transport Unification

Problem

Two competing models:
	•	Direction control (← → ping-pong)
	•	Hidden Play button

Solution

Remove Play button entirely.

Behavior

Action	Result
Tap inactive direction	Start playback
Tap active direction	Pause
Tap again	Resume

UI Requirement
	•	Overlay pause indicator on active direction
	•	Active state must be visually dominant

⸻

2.2 Speed vs Sync Semantic Split

Problem

Single control changes meaning:
	•	FREE → rate multiplier
	•	SYNC → loop length

Solution

Dynamic labeling + icon swap

Mode	Label	Icon	Value Format
FREE	Speed	orbit dot	3.75×
SYNC	Length	tick ring	2 bars

Implementation
	•	Same parameter internally
	•	UI label + icon must switch with mode

⸻

3. Focused Lane Model (Core Interaction Spec)

3.1 Principle

Only one lane is editable at any time.

⸻

3.2 Visual Ownership

When a lane is focused:

Canvas
	•	Border glow = lane color
	•	Cursor = lane color (before touch-down)
	•	New strokes = lane color (full opacity)
	•	Other lanes = dimmed (~40%)

Playheads
	•	Focused lane → thick + opaque
	•	Others → thin + translucent

Grid
	•	Subtle tint toward lane color

⸻

3.3 Input Routing
	•	All gesture input → focused lane only
	•	No implicit lane switching

⸻

3.4 Focus Triggers

Action	Result
Tap lane header	Focus lane
Tap curve	Focus owning lane
Routing interaction	Focus lane
Drawing	Uses current focus


⸻

3.5 Persistence
	•	Focus persists across:
	•	playback
	•	sync changes
	•	note mode

⸻

3.6 Failure Case to Prevent

❌ User unsure which lane they are editing

Required Fix
	•	Cursor must reflect lane color before drawing begins

⸻

3.7 Optional Enhancement

Lane Solo (long press):
	•	Temporarily isolate lane
	•	Restore on release

⸻

4. Icon System Specification

4.1 Constraints
	•	Stroke-only icons
	•	Single stroke width
	•	Square bounds (e.g. 24×24)
	•	Built from:
	•	circles
	•	arcs
	•	lines
	•	dots
	•	No skeuomorphic metaphors

⸻

4.2 Icon Definitions

NOTE MODE
	•	Circle + 3 diagonal dots
	•	ON = dots visible
	•	OFF = empty circle

⸻

CLEAR
	•	Circle + sweeping arc + trailing dots
	•	Represents gesture wipe (not deletion)

⸻

SPEED (FREE)
	•	Arc + orbiting dot
	•	Dot distance = speed

⸻

LENGTH (SYNC)
	•	Circle + evenly spaced ticks
	•	Same base form as Speed

⸻

SYNC
	•	Two vertical guides + aligned dots
	•	ON = perfectly aligned
	•	OFF = offset

⸻

TEACH (Corrected)

Intent
Assist external MIDI Learn (not capture input)

Geometry
	•	Source dot
	•	Outward arrow
	•	Destination ring

States
	•	Idle → static
	•	Active → pulse on arrow or destination

Avoid
	•	Microphone / recording metaphors
	•	Inward arrows
	•	“Listening” visual language

⸻

MUTE
	•	Circle + horizontal occlusion band

⸻

TARGET
	•	One source dot branching to endpoints

⸻

RANGE
	•	Vertical bracket with top/bottom caps

⸻

GRID DENSITY

X Density
	•	Horizontal lines + +/- indicator

Y Density
	•	Vertical lines + +/- indicator

⸻

PLAYBACK DIRECTION
	•	← / → / ping-pong
	•	Active = highlighted
	•	Paused = overlay pause bars

⸻

4.3 Implementation Notes
	•	Use juce::Path
	•	Cache icons as DrawablePath
	•	Support states:
	•	normal
	•	hover
	•	active
	•	disabled

⸻

5. Control Taxonomy (Visual Separation)

5.1 Categories

Type	Examples	Behavior	UI Style
State	Sync, Mute	toggle	persistent highlight
Action	Clear, Panic, Teach	momentary	flash feedback
Mode	Note Mode	exclusive toggle	segmented
Routing	Target	structured	matrix/grid


⸻

5.2 Requirement

Do not style all controls the same.

⸻

6. Note Mode Behavior

Problem

Too much complexity exposed at once.

Solution

When OFF
	•	Hide note editor completely

When ON
	•	Animate expansion
	•	Dim unrelated controls
	•	Emphasize pitch interaction

⸻

7. Routing UX Improvements

7.1 Progressive Disclosure
	•	Show only active lane routing by default
	•	Expand others on demand

⸻

7.2 Teach Behavior Clarification

Definition

Teach = assist mapping in another plugin’s MIDI Learn

Expected Behavior Options

(implementation decision required)
	•	Send identifiable modulation pattern
	•	Temporarily isolate lane output
	•	Highlight active mapping signal

⸻

8. Grid Control Redesign

Replace

X-  X+
Y-  Y+

With
	•	Two icons:
	•	X density
	•	Y density
	•	Each supports:
	•	tap zones or +/- controls

⸻

9. First-Run Experience

Add onboarding overlay

Sequence:
	1.	Select lane
	2.	Draw gesture
	3.	Choose target
	4.	Press play

Must be dismissible.

⸻

10. Motion & Feedback

Required Animations
	•	Playhead movement (smooth, continuous)
	•	Sync snapping
	•	Teach activation pulse

⸻

Optional Enhancements
	•	Button tap easing
	•	Subtle motion on parameter change

⸻

11. Color System

Each lane has:
	•	Unique hue
	•	Applied consistently to:
	•	curve
	•	playhead
	•	focus border
	•	routing highlight

⸻

12. Summary of Required Changes

Must Implement
	•	Remove Play button
	•	Merge transport into direction control
	•	Split Speed / Length semantics
	•	Implement focused lane visuals
	•	Replace key labels with icons
	•	Redesign Teach semantics and icon
	•	Separate control types visually

Should Implement
	•	Note mode isolation
	•	Routing progressive disclosure
	•	Grid control redesign
	•	First-run onboarding

Nice to Have
	•	Lane solo (long press)
	•	Micro-interactions
	•	Motion polish

⸻

13. Final Design Target

A visually coherent, lane-aware, drawable MIDI modulation instrument
with minimal text, strong gesture ownership, and predictable behavior.

⸻

If needed next, I can provide:
	•	IconFactory.h (JUCE-ready code)
	•	SVG export set
	•	Updated layout wireframe reflecting these changes