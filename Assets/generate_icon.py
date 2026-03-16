#!/usr/bin/env python3
"""
Generate DrawnCurve app icon (1024x1024 PNG).

Usage:
    python3 Assets/generate_icon.py

Requires matplotlib:
    pip install matplotlib
or:
    brew install python && pip3 install matplotlib
"""

import math
import os
import sys

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.path import Path
    import matplotlib.patheffects as pe
    import numpy as np
except ImportError:
    print("ERROR: matplotlib not found. Install it with:  pip install matplotlib")
    sys.exit(1)

# ── Colours (matching plugin UI) ──────────────────────────────────────────────
BG      = "#12121f"   # dark navy
CYAN    = "#00e5ff"   # curve colour
GRID    = "#ffffff18" # grid line (very faint)
ORANGE  = "#ff6b35"   # capture trail / accent

SIZE_PX = 1024
DPI     = 100         # figure size = SIZE_PX / DPI inches

fig, ax = plt.subplots(figsize=(SIZE_PX / DPI, SIZE_PX / DPI), dpi=DPI)
fig.patch.set_facecolor(BG)
ax.set_facecolor(BG)
ax.set_xlim(0, 1)
ax.set_ylim(0, 1)
ax.set_aspect("equal")
ax.axis("off")
plt.subplots_adjust(left=0, right=1, top=1, bottom=0)

# ── Subtle grid ────────────────────────────────────────────────────────────────
for frac in (0.25, 0.5, 0.75):
    ax.axvline(frac, color="white", alpha=0.07, linewidth=1)
    ax.axhline(frac, color="white", alpha=0.07, linewidth=1)

# ── Draw a flowing S-curve (the defining gesture) ─────────────────────────────
t  = np.linspace(0, 1, 600)
# S-curve: low → high with a natural drawn feel (no perfectly symmetric)
y  = 0.12 + 0.76 * (0.5 + 0.48 * np.tanh(7.0 * (t - 0.52)))
# Tiny organic wobble so it looks hand-drawn
y += 0.012 * np.sin(t * math.pi * 3.0) * (1 - t)

# Glow pass (wider, dimmer)
ax.plot(t, y, color=CYAN, linewidth=22, alpha=0.15,
        solid_capstyle="round", solid_joinstyle="round")
ax.plot(t, y, color=CYAN, linewidth=10, alpha=0.30,
        solid_capstyle="round", solid_joinstyle="round")
# Main stroke
ax.plot(t, y, color=CYAN, linewidth=4.5, alpha=1.0,
        solid_capstyle="round", solid_joinstyle="round")

# ── Playhead dot on the curve (at ~62 % along) ────────────────────────────────
ph_t = 0.62
ph_y = float(np.interp(ph_t, t, y))
ax.plot(ph_t, ph_y, "o", color=CYAN, markersize=22, zorder=5,
        markeredgewidth=0)
# Inner bright dot
ax.plot(ph_t, ph_y, "o", color="white", markersize=9, zorder=6,
        markeredgewidth=0)

# ── Playhead vertical line ─────────────────────────────────────────────────────
ax.axvline(ph_t, color="white", alpha=0.45, linewidth=1.5, zorder=4)

# ── Output ─────────────────────────────────────────────────────────────────────
out_path = os.path.join(os.path.dirname(__file__), "icon_1024.png")
plt.savefig(out_path, dpi=DPI, bbox_inches="tight",
            facecolor=BG, edgecolor="none")
plt.close(fig)
print(f"Generated {out_path}")
