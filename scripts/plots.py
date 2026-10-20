"""Figures for the README, each in a light and a dark version (docs/figures/<name>-{light,dark}.png).

    python3 scripts/plots.py

Reads results/lookup.csv, memory.csv and steps.csv, written by `make bench`. Colours: three
categorical slots checked for colour-vision deficiency on both surfaces; text always uses the ink
colours, never a series colour.
"""
from __future__ import annotations

import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import font_manager  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle  # noqa: E402

BASE = Path(__file__).resolve().parent.parent
RES = BASE / "results"
OUT = BASE / "docs" / "figures"
DPI = 200
PX = DPI / 100  # figure pixels per screen pixel: the PNGs are drawn at twice the size they are shown
PT = 72 / DPI   # points per figure pixel

THEMES = {
    "light": {"surface": "#fcfcfb", "ink": "#0b0b0b", "ink2": "#52514e", "muted": "#898781", "grid": "#e1e0d9",
              "axis": "#c3c2b7", "fill": "#f0efec", "s1": "#2a78d6", "s2": "#eb6834", "s3": "#1baf7a"},
    "dark": {"surface": "#1a1a19", "ink": "#f0efec", "ink2": "#c3c2b7", "muted": "#898781", "grid": "#2c2c2a",
             "axis": "#383835", "fill": "#2c2c2a", "s1": "#3987e5", "s2": "#d95926", "s3": "#199e70"},
}

# structure -> (colour slot, label), the same in every figure
SERIES = {
    "trie": ("s1", "Binary trie"),
    "bspl_flat": ("s2", "BSPL, open addressing"),
    "bspl_chained": ("s3", "BSPL, std::unordered_map"),
}


def read(name: str) -> list[dict]:
    with open(RES / name, newline="") as f:
        return list(csv.DictReader(f))


def compact(n: float) -> str:
    return f"{n / 1e6:.2f}M" if n >= 1e6 else f"{n / 1e3:.0f}K" if n >= 1e3 else f"{n:.0f}"


# ---------------------------------------------------------------- drawing helpers

def heading(fig, t: dict, text: str, sub: str) -> None:
    fig.text(0.025, 1 - 0.18 / fig.get_figheight(), text, color=t["ink"], fontsize=12, fontweight="semibold", va="top")
    fig.text(0.025, 1 - 0.50 / fig.get_figheight(), sub, color=t["ink2"], fontsize=8.5, va="top")


def note(fig, t: dict, text: str) -> None:
    fig.text(0.025, 0.1 / fig.get_figheight(), text, color=t["muted"], fontsize=7.5, va="bottom")


def legend_row(fig, t: dict, items: list[tuple[str, str]], top: float) -> None:
    """Legend under the subtitle: a coloured key mark beside ink text, per series, left to right."""
    y = 1 - top / fig.get_figheight()
    x = 0.025
    renderer = fig.canvas.get_renderer()
    for color, label in items:
        fig.lines.append(plt.Line2D([x + 0.004, x + 0.022], [y, y], transform=fig.transFigure, color=color,
                                    linewidth=2 * PX * PT, solid_capstyle="round"))
        fig.lines.append(plt.Line2D([x + 0.013], [y], transform=fig.transFigure, marker="o", markersize=8 * PX * PT,
                                    color=color, markeredgecolor=t["surface"], markeredgewidth=2 * PX * PT))
        txt = fig.text(x + 0.03, y, label, color=t["ink2"], fontsize=8.5, va="center")
        x += 0.03 + txt.get_window_extent(renderer).width / fig.bbox.width + 0.035


def style_axes(ax, t: dict, grid_axis: str) -> None:
    for side in ("top", "right", "left", "bottom"):
        ax.spines[side].set_visible(False)
    ax.tick_params(colors=t["muted"], labelcolor=t["muted"], length=0, labelsize=8, pad=5)
    ax.grid(axis=grid_axis, color=t["grid"], linewidth=0.75)
    ax.set_axisbelow(True)


def baseline(ax, t: dict, axis: str) -> None:
    side = "bottom" if axis == "x" else "left"
    ax.spines[side].set_visible(True)
    ax.spines[side].set_color(t["axis"])
    ax.spines[side].set_linewidth(0.75)


def px_size(ax) -> tuple[float, float]:
    """Data units per figure pixel along x and y (the limits must be final; linear axes only)."""
    box = ax.get_window_extent()
    (x0, x1), (y0, y1) = ax.get_xlim(), ax.get_ylim()
    return (x1 - x0) / box.width, abs(y1 - y0) / box.height


def hbar(ax, y: float, value: float, color: str, thick: float = 14) -> None:
    """Horizontal bar from the baseline: square there, 4px rounded at the data end, at most 24px thick."""
    dx, dy = px_size(ax)
    h = min(thick, 24) * PX * dy
    r = min(4 * PX * dx, value / 2)
    ax.add_patch(FancyBboxPatch((0, y - h / 2), value, h, boxstyle=f"round,pad=0,rounding_size={r}",
                                mutation_aspect=dy / dx, facecolor=color, edgecolor="none", zorder=3))
    ax.add_patch(Rectangle((0, y - h / 2), min(value, r), h, facecolor=color, edgecolor="none", zorder=3))


def vbar(ax, x: float, value: float, color: str, thick: float = 12) -> None:
    """Column from the baseline: square there, 4px rounded at the top, at most 24px wide."""
    dx, dy = px_size(ax)
    w = min(thick, 24) * PX * dx
    r = min(4 * PX * dx, w / 2, value / 2 * dx / dy)  # in x units; the patch scales it by dy/dx vertically
    ax.add_patch(FancyBboxPatch((x - w / 2, 0), w, value, boxstyle=f"round,pad=0,rounding_size={r}",
                                mutation_aspect=dy / dx, facecolor=color, edgecolor="none", zorder=3))
    ax.add_patch(Rectangle((x - w / 2, 0), w, min(value, r * dy / dx), facecolor=color, edgecolor="none", zorder=3))


def save(fig, name: str, theme: str) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}-{theme}.png"
    fig.savefig(path, dpi=DPI, facecolor=fig.get_facecolor())
    plt.close(fig)
    return path


# ---------------------------------------------------------------- figures

def fig_lookup(t: dict):
    """Median time per lookup against table size, for independent and dependent lookups."""
    points = defaultdict(list)
    for r in read("lookup.csv"):
        if r["addresses"] == "routed":
            points[(r["structure"], r["mode"])].append((int(r["routes"]), float(r["ns_median"])))
    top = max(ns for pts in points.values() for _, ns in pts)
    ymax = 100 * (int(top // 100) + 1)
    full = max(n for pts in points.values() for n, _ in pts)

    fig = plt.figure(figsize=(7.2, 4.2), dpi=DPI, facecolor=t["surface"])
    for k, (mode, title) in enumerate([("independent", "Independent lookups"),
                                       ("dependent", "Dependent lookups (latency)")]):
        ax = fig.add_axes((0.085 + k * 0.475, 0.2, 0.36, 0.48), facecolor=t["surface"])
        style_axes(ax, t, "y")
        baseline(ax, t, "x")
        ax.set_xscale("log")
        ax.set_xlim(700, full * 1.6)
        ax.set_xticks([1e3, 1e4, 1e5, 1e6], ["1K", "10K", "100K", "1M"])
        ax.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
        ax.set_ylim(0, ymax)
        ax.set_yticks(range(0, ymax + 1, 100))
        ax.set_title(title, color=t["ink"], fontsize=9, loc="left", pad=8)
        ax.set_xlabel("prefixes in the table", color=t["muted"], fontsize=7.5, labelpad=4)
        if k == 0:
            ax.set_ylabel("ns per lookup", color=t["muted"], fontsize=7.5, labelpad=4)
        for structure, (slot, _) in SERIES.items():
            xs, ys = zip(*sorted(points[(structure, mode)]))
            ax.plot(xs, ys, color=t[slot], linewidth=2 * PX * PT, solid_joinstyle="round", solid_capstyle="round",
                    marker="o", markersize=8 * PX * PT, markeredgecolor=t["surface"], markeredgewidth=2 * PX * PT,
                    zorder=3, clip_on=False)
            ax.text(xs[-1] * 1.17, ys[-1], f"{ys[-1]:.0f}", color=t["ink"], fontsize=8, va="center")
    heading(fig, t, "Time per lookup as the routing table grows",
            "Median of 11 passes over 1M addresses inside the table's prefixes; RouteViews table and random subsets")
    legend_row(fig, t, [(t[slot], label) for slot, label in SERIES.values()], top=0.86)
    note(fig, t, "Dependent: each address waits for the previous result, so the CPU cannot overlap lookups. "
                 "One core of an Intel Core i5-1035G1, g++ -O2.")
    return fig


def fig_steps(t: dict):
    """Distribution of memory reads per lookup on the full table: trie nodes against BSPL hash probes."""
    counts = defaultdict(dict)
    for r in read("steps.csv"):
        if r["addresses"] == "routed":
            counts[r["structure"]][int(r["steps"])] = int(r["count"])
    total = sum(counts["trie"].values())
    share = {s: {k: 100 * v / total for k, v in c.items()} for s, c in counts.items()}
    mean = {s: sum(k * v for k, v in c.items()) / total for s, c in counts.items()}

    fig = plt.figure(figsize=(7.2, 3.6), dpi=DPI, facecolor=t["surface"])
    ax = fig.add_axes((0.085, 0.17, 0.87, 0.50), facecolor=t["surface"])
    style_axes(ax, t, "y")
    baseline(ax, t, "x")
    ax.set_xlim(0.2, 33.8)
    ax.set_xticks([1, 5, 10, 15, 20, 25, 30, 33])
    top = max(max(v.values()) for v in share.values())
    ax.set_ylim(0, 10 * (int(top // 10) + 1))
    ax.yaxis.set_major_formatter(lambda v, _: f"{v:.0f}%")
    ax.set_xlabel("memory reads per lookup (trie nodes or hash probes)", color=t["muted"], fontsize=7.5, labelpad=4)
    for structure, slot in (("trie", "s1"), ("bspl", "s2")):
        for steps, pct in share[structure].items():
            vbar(ax, steps, pct, t[slot])
        peak = max(share[structure], key=share[structure].get)
        label = "trie nodes" if structure == "trie" else "hash probes"
        ax.text(peak, share[structure][peak] + top * 0.04, f"mean {mean[structure]:.1f} {label}", color=t["ink"],
                fontsize=8, ha="center", va="bottom")
    heading(fig, t, "Memory reads per lookup on the full table",
            f"{compact(total)} addresses inside the table's {compact(1125729)} prefixes; BSPL makes the same probes "
            "with either hash table")
    legend_row(fig, t, [(t["s1"], "Binary trie: nodes read"), (t["s2"], "BSPL: hash tables probed")], top=0.86)
    return fig


def fig_memory(t: dict):
    """Memory and build time of the three structures on the full table."""
    rows = {r["structure"]: r for r in read("memory.csv") if int(r["routes"]) == max(int(x["routes"])
                                                                                  for x in read("memory.csv"))}
    order = ["trie", "bspl_flat", "bspl_chained"]
    fig = plt.figure(figsize=(7.2, 3.2), dpi=DPI, facecolor=t["surface"])
    panels = [("bytes", "Memory", lambda v: v / 1e6, "MB"), ("build_ms", "Build time", float, "ms")]
    for k, (field, title, scale, unit) in enumerate(panels):
        ax = fig.add_axes((0.25 + k * 0.385, 0.25, 0.28, 0.42), facecolor=t["surface"])
        style_axes(ax, t, "x")
        baseline(ax, t, "y")
        values = [scale(float(rows[s][field])) for s in order]
        ax.set_xlim(0, max(values) * 1.35)
        ax.set_ylim(len(order) - 0.45, -0.55)
        ax.set_yticks(range(len(order)), [SERIES[s][1] for s in order] if k == 0 else [])
        ax.tick_params(axis="y", labelcolor=t["ink2"], labelsize=8.5)
        ax.set_title(title, color=t["ink"], fontsize=9, loc="left", pad=8)
        ax.xaxis.set_major_locator(matplotlib.ticker.MaxNLocator(4))
        ax.set_xlabel(unit, color=t["muted"], fontsize=7.5, labelpad=4)
        for i, (s, v) in enumerate(zip(order, values)):
            hbar(ax, i, v, t[SERIES[s][0]])
            ax.text(v + max(values) * 0.03, i, f"{v:.0f} {unit}", color=t["ink"], fontsize=8, va="center")
    entries = int(rows["bspl_flat"]["items"])
    markers = int(rows["bspl_flat"]["markers"])
    heading(fig, t, "Memory and build time, full table",
            f"BSPL holds {compact(entries)} entries, {compact(markers)} of them markers; the trie has "
            f"{compact(int(rows['trie']['items']))} nodes of 12 bytes")
    note(fig, t, "std::unordered_map memory is estimated from node and bucket counts, without the allocator's "
                 "per-node overhead. Fastest of 3 builds.")
    return fig


def fig_bspl(t: dict):
    """How BSPL answers two lookups in the sample table: the marker and its best match."""
    fig = plt.figure(figsize=(7.2, 3.7), dpi=DPI, facecolor=t["surface"])
    ax = fig.add_axes((0.02, 0.02, 0.96, 0.78), facecolor=t["surface"])
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 56)
    ax.axis("off")

    def box(x, y, w, h, fill, edge=None, lw=0.0):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.2", mutation_aspect=0.6,
                                    facecolor=fill, edgecolor=edge or "none", linewidth=lw, zorder=2))

    tables = [
        ("/16", 2.5, [("192.168", "Router2", False), ("10.10", "Router4", False)]),
        ("/24", 35.5, [("192.168.1", "Router3", False), ("192.168.20", "marker, best: Router2", True)]),
        ("/28", 68.5, [("192.168.20.16", "Router1", False)]),
    ]
    for length, x, entries in tables:
        ax.text(x, 53, f"{length} table", color=t["ink"], fontsize=9, fontweight="semibold", va="center")
        for i, (key, value, marker) in enumerate(entries):
            y = 44 - i * 8.5
            if marker:
                box(x, y, 29.5, 6.5, t["surface"], t["s2"], lw=2 * PX * PT)
            else:
                box(x, y, 29.5, 6.5, t["fill"])
            ax.text(x + 1.5, y + 3.25, key, color=t["ink"], fontsize=8, va="center", family="monospace")
            ax.text(x + 28, y + 3.25, value, color=t["ink2"], fontsize=7.5 if marker else 8, va="center", ha="right")
    ax.text(35.5, 31.5, "search starts at the middle length", color=t["muted"], fontsize=7.5, va="center")

    traces = [
        ("192.168.20.20", [("/24", "hit", "marker"), ("/28", "hit", "192.168.20.16")], "Router1"),
        ("192.168.20.191", [("/24", "hit", "marker"), ("/28", "miss", "192.168.20.176")], "Router2 (the marker's best match)"),
    ]
    for i, (addr, steps, result) in enumerate(traces):
        y = 21 - i * 11
        ax.text(3, y, addr, color=t["ink"], fontsize=8.5, fontweight="semibold", va="center", family="monospace")
        x = 25
        for length, outcome, key in steps:
            hit = outcome == "hit"
            box(x, y - 3.2, 17, 6.4, t["fill"] if hit else t["surface"], None if hit else t["axis"],
                lw=0 if hit else 1.0)
            ax.text(x + 8.5, y, f"{length}: {outcome}", color=t["ink"] if hit else t["ink2"], fontsize=8,
                    ha="center", va="center")
            ax.add_patch(FancyArrowPatch((x + 17.5, y), (x + 21.5, y), arrowstyle="-|>", mutation_scale=7,
                                         color=t["muted"], linewidth=1.0, zorder=3))
            x += 22
        ax.text(x, y, result, color=t["ink"], fontsize=8.5, va="center")
    heading(fig, t, "How BSPL looks up an address",
            "The sample table has one hash table per prefix length. A hit sends the search to longer lengths, "
            "a miss to shorter ones.")
    note(fig, t, "The /24 entry for 192.168.20 is a marker: the /28 prefix needs it on its search path.\n"
                 "When the probe at /28 misses, the answer is the marker's precomputed best match.")
    return fig


def main() -> None:
    names = {f.name for f in font_manager.fontManager.ttflist}
    plt.rcParams["font.family"] = "Inter" if "Inter" in names else "DejaVu Sans"
    for theme, t in THEMES.items():
        for name, make in (("lookup", fig_lookup), ("steps", fig_steps), ("memory", fig_memory), ("bspl", fig_bspl)):
            print(save(make(t), name, theme).relative_to(BASE))


if __name__ == "__main__":
    main()
