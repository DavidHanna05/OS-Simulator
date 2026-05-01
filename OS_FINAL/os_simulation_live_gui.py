import os
import re
import shlex
import queue
import threading
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from tkinter.scrolledtext import ScrolledText

# ─────────────────────────────────────────────────────────────────────────────
#  COLOUR PALETTE
# ─────────────────────────────────────────────────────────────────────────────
C = {
    "bg":         "#0A1F1F",   # Deep teal-black — page bg
    "bg2":        "#0F2A2A",   # Dark teal — panel bg
    "bg3":        "#153535",   # Rich teal — card surfaces
    "bg4":        "#1C4040",   # Teal hover — active surfaces
    "border":     "#1E5C5C",   # Teal steel — dividers
    "border2":    "#00C9B1",   # Bright teal — active borders
    "text":       "#F0FFFE",   # Ice white — primary text
    "text2":      "#F0FFFE",   # Soft teal — body text
    "text3":      "#F0FFFE",   # Muted teal — labels
    "green":      "#00E5C8",   # Electric teal — success/running
    "green_bg":   "#082020",
    "green_dim":  "#0F2E2E",
    "blue":       "#00C9B1",   # Bright teal — info/ready
    "blue_bg":    "#0A1F1F",
    "blue_dim":   "#153535",
    "amber":      "#FF6B00",   # Vivid orange — warnings/input
    "amber_bg":   "#2A1400",
    "amber_dim":  "#381B00",
    "red":        "#FF3D00",   # Hot red-orange — errors/stop
    "red_bg":     "#2A0A00",
    "purple":     "#FF8C42",   # Warm orange — disk/misc
    "purple_bg":  "#1C0D00",
    "purple_dim": "#2A1400",
    "teal":       "#00E5C8",   # Electric teal — output/exec
    "teal_bg":    "#082020",
}

PROC_PAL = {
    1: {"bg": "#082020", "border": "#00E5C8", "fg": "#00E5C8"},   # Electric teal
    2: {"bg": "#2A1400", "border": "#FF6B00", "fg": "#FF6B00"},   # Vivid orange
    3: {"bg": "#1C2800", "border": "#C8E500", "fg": "#C8E500"},   # Electric lime
}

STATE_INFO = {
    "RUNNING":  ("#082020", "#00E5C8", "▶ RUNNING"),    # Dark teal bg, electric teal
    "READY":    ("#0A1F1F", "#00C9B1", "● READY"),       # Deep bg, bright teal
    "BLOCKED":  ("#2A1400", "#FF6B00", "◈ BLOCKED"),     # Dark orange bg, vivid orange
    "FINISHED": ("#153535", "#4FA8A3", "✓ FINISHED"),    # Teal card, muted teal
    "ON_DISK":  ("#1C0D00", "#FF8C42", "⬡ ON DISK"),    # Dark bg, warm orange
    "-":        ("#0F2A2A", "#1E5C5C", "—"),
}

MONO   = ("Consolas", 9)
MONO_S = ("Consolas", 8)
MONO_L = ("Consolas", 10)
UI     = ("Segoe UI", 9)
UI_B   = ("Segoe UI", 9, "bold")


# ─────────────────────────────────────────────────────────────────────────────
#  SMALL HELPERS
# ─────────────────────────────────────────────────────────────────────────────

def sep(parent, orient="h", color=None):
    color = color or C["border"]
    if orient == "h":
        return tk.Frame(parent, height=1, bg=color)
    return tk.Frame(parent, width=1, bg=color)


def lbl(parent, text="", fg=None, font=UI, bg=None, **kw):
    return tk.Label(parent, text=text, fg=fg or C["text"],
                    font=font, bg=bg or C["bg2"], **kw)


def btn(parent, text, cmd, fg=None, bg=None):
    fg = fg or C["text"]
    bg = bg or C["bg3"]
    b = tk.Button(parent, text=text, command=cmd,
                  fg=fg, bg=bg, font=UI_B,
                  relief="flat", cursor="hand2",
                  activeforeground=fg, activebackground=C["bg4"],
                  padx=10, pady=5, bd=0,
                  highlightthickness=1,
                  highlightbackground=C["border2"],
                  highlightcolor=C["border2"])
    b.bind("<Enter>", lambda e: b.config(bg=C["bg4"]))
    b.bind("<Leave>", lambda e: b.config(bg=bg))
    return b


def panel(parent, title, dot_color, bg_inner=None):
    """Returns (outer_frame, inner_frame). Pack outer_frame."""
    bg_inner = bg_inner or C["bg2"]
    outer = tk.Frame(parent, bg=C["border"])
    inner = tk.Frame(outer, bg=bg_inner)
    inner.pack(fill="both", expand=True, padx=1, pady=1)
    hdr = tk.Frame(inner, bg=C["bg3"], pady=5, padx=10)
    hdr.pack(fill="x")
    tk.Label(hdr, text="●", fg=dot_color, bg=C["bg3"], font=MONO_S).pack(side="left", padx=(0, 5))
    tk.Label(hdr, text=title.upper(), fg=C["text3"], bg=C["bg3"],
             font=("Segoe UI", 8, "bold")).pack(side="left")
    sep(inner).pack(fill="x")
    return outer, inner


# ─────────────────────────────────────────────────────────────────────────────
#  PROCESS CARD
# ─────────────────────────────────────────────────────────────────────────────

class ProcessCard(tk.Frame):
    def __init__(self, parent, pid):
        pal = PROC_PAL.get(pid, {"bg": C["bg3"], "border": C["border2"], "fg": C["text2"]})
        super().__init__(parent, bg=pal["border"], padx=1, pady=1)
        self.pid = pid
        self.pal = pal

        self.inner = tk.Frame(self, bg=C["bg2"])
        self.inner.pack(fill="both", expand=True)

        self.accent = tk.Frame(self.inner, height=3, bg=pal["fg"])
        self.accent.pack(fill="x")

        head = tk.Frame(self.inner, bg=C["bg2"], padx=8, pady=6)
        head.pack(fill="x")
        tk.Label(head, text=f"P{pid}", fg=pal["fg"], bg=C["bg2"],
                 font=("Consolas", 14, "bold")).pack(side="left")
        self.state_badge = tk.Label(head, text="—", fg=C["text3"], bg=C["bg3"],
                                    font=("Segoe UI", 8, "bold"), padx=6, pady=2)
        self.state_badge.pack(side="right")

        sep(self.inner).pack(fill="x")

        stats = tk.Frame(self.inner, bg=C["bg2"], padx=8, pady=6)
        stats.pack(fill="x")
        self.sv = {}
        fields = [("PC", "pc"), ("Burst", "burst"), ("Wait", "wait"), ("Mem", "mem"), ("Disk", "disk")]
        for i, (label, key) in enumerate(fields):
            r, c = divmod(i, 2)
            cell = tk.Frame(stats, bg=C["bg2"])
            cell.grid(row=r, column=c, sticky="w", padx=(0, 14), pady=1)
            tk.Label(cell, text=label, fg=C["text3"], bg=C["bg2"], font=("Segoe UI", 8)).pack(side="left")
            v = tk.StringVar(value="—")
            self.sv[key] = v
            tk.Label(cell, textvariable=v, fg=C["text2"], bg=C["bg2"], font=MONO_S).pack(side="left", padx=(3, 0))

        pb_wrap = tk.Frame(self.inner, bg=C["bg2"])
        pb_wrap.pack(fill="x", padx=8, pady=(0, 8))
        self.pb_bg = tk.Frame(pb_wrap, height=3, bg=C["bg4"])
        self.pb_bg.pack(fill="x")
        self.pb_fill = tk.Frame(pb_wrap, height=3, bg=pal["fg"])
        self.pb_fill.place(in_=self.pb_bg, x=0, y=0, relheight=1, relwidth=0)

    def update(self, state, pc, burst, wait, mem, disk):
        key = state.upper() if state else "-"
        if key not in STATE_INFO:
            key = "-"
        ibg, ifg, itext = STATE_INFO[key]
        self.state_badge.config(text=itext, fg=ifg, bg=ibg)
        self.inner.config(bg=C["bg3"] if key == "RUNNING" else C["bg2"])
        self.config(bg=self.pal["fg"] if key == "RUNNING" else self.pal["border"])
        self.sv["pc"].set(str(pc))
        self.sv["burst"].set(str(burst))
        self.sv["wait"].set(str(wait))
        self.sv["mem"].set(mem)
        self.sv["disk"].set(disk)
        try:
            if int(burst) > 0:
                self.pb_fill.place(relwidth=min(1.0, int(pc) / int(burst)))
        except Exception:
            pass


# ─────────────────────────────────────────────────────────────────────────────
#  MEMORY CELL
# ─────────────────────────────────────────────────────────────────────────────

class MemCell(tk.Frame):
    _gui = None

    def __init__(self, parent, idx):
        super().__init__(parent, bg=C["bg4"], padx=1, pady=1, width=56, height=46)
        self.pack_propagate(False)
        self.idx = idx
        self._tip = ""
        self.inner = tk.Frame(self, bg=C["bg3"])
        self.inner.pack(fill="both", expand=True)
        tk.Label(self.inner, text=str(idx), fg=C["text3"], bg=C["bg3"],
                 font=("Consolas", 7)).pack(fill="x", pady=(2, 0), padx=2)
        self.val = tk.Label(self.inner, text="—", fg=C["text3"], bg=C["bg3"],
                            font=("Consolas", 8), wraplength=50, justify="center")
        self.val.pack(fill="both", expand=True, padx=2, pady=(0, 2))
        for w in (self, self.inner, self.val):
            w.bind("<Enter>", self._enter)
            w.bind("<Leave>", self._leave)

    def _enter(self, e):
        if self._tip and MemCell._gui:
            MemCell._gui.show_tip(self._tip, e)

    def _leave(self, e):
        if MemCell._gui:
            MemCell._gui.hide_tip()

    def set_free(self):
        self.config(bg=C["bg4"])
        self.inner.config(bg=C["bg3"])
        self.val.config(text="—", fg=C["text3"], bg=C["bg3"])
        self._tip = ""

    def set_owned(self, pid, value, is_pcb=False, active=False):
        pal = PROC_PAL.get(pid, {"bg": C["bg3"], "border": C["border2"], "fg": C["text2"]})
        ibg = pal["bg"]
        border_col = C["amber"] if active else pal["border"]
        self.config(bg=border_col)
        self.inner.config(bg=ibg)
        short = value[:6] + "…" if len(str(value)) > 7 else str(value)
        fg = C["text2"] if is_pcb else pal["fg"]
        self.val.config(text=short, fg=fg, bg=ibg)
        self._tip = f"[{self.idx}]  {value}\nP{pid}  {'PCB' if is_pcb else 'instr/var'}"


# ─────────────────────────────────────────────────────────────────────────────
#  MAIN GUI
# ─────────────────────────────────────────────────────────────────────────────

class LiveOSGUI:
    MEM = 40

    def __init__(self, root):
        MemCell._gui = self
        self.root = root
        self.root.title("CSEN 602 — OS Simulation Visualizer")
        self.root.geometry("1720x1020")
        self.root.minsize(1400, 860)
        self.root.configure(bg=C["bg"])

        # backend state
        self.process       = None
        self.reader_thread = None
        self.out_q         = queue.Queue()
        self.reader_alive  = False

        # playback state
        self.paused          = True
        self.step_mode       = False
        self.play_job        = None
        self.mem_capture     = False
        self.pending_buf     = []
        self.snap            = self._new_snap()
        self.last_clock      = None
        self.waiting_input   = False
        self._blink_job      = None
        self._blink_state    = True
        self._tip_win        = None
        self._console_done   = 0

        # tk vars
        self.cmd_var    = tk.StringVar()
        self.sched_var  = tk.StringVar(value="1")
        self.status_var = tk.StringVar(value="Idle")
        self.speed_var  = tk.IntVar(value=700)

        self._build_ui()
        self._seed_exe()

    # ── TOOLTIP ──────────────────────────────────────────────────────────────

    def show_tip(self, text, event):
        self.hide_tip()
        w = tk.Toplevel(self.root)
        w.wm_overrideredirect(True)
        w.configure(bg=C["border2"])
        f = tk.Frame(w, bg=C["bg2"], padx=8, pady=5)
        f.pack(padx=1, pady=1)
        tk.Label(f, text=text, fg=C["text"], bg=C["bg2"],
                 font=MONO, justify="left").pack()
        w.geometry(f"+{event.x_root+14}+{event.y_root+14}")
        self._tip_win = w

    def hide_tip(self):
        if self._tip_win:
            self._tip_win.destroy()
            self._tip_win = None

    # ── SNAPSHOT ─────────────────────────────────────────────────────────────

    def _new_snap(self):
        return {
            "clock": 0, "algorithm": "", "running_pid": "-",
            "running_pc": "-", "current_instr": "-",
            "ready": [], "blocked": [],
            "mlfq": [[], [], [], []],
            "mutexes": {"userOutput": "free", "userInput": "free", "file": "free"},
            "disk_last": "-", "disk_lines": [],
            "memory": ["---"] * self.MEM,
            "processes": {}, "output_lines": [],
        }

    # ── BUILD UI ─────────────────────────────────────────────────────────────

    def _build_ui(self):
        self._build_title()
        sep(self.root).pack(fill="x")
        self._build_controls()
        sep(self.root).pack(fill="x")
        self._build_input_row()
        sep(self.root).pack(fill="x")
        self._build_body()

    # ── TITLE BAR ────────────────────────────────────────────────────────────

    def _build_title(self):
        bar = tk.Frame(self.root, bg=C["bg2"], pady=10, padx=16)
        bar.pack(fill="x")
        tk.Label(bar, text="♡", fg=C["green"], bg=C["bg2"],
                 font=("Consolas", 18, "bold")).pack(side="left")
        tk.Label(bar, text=" OS-SIM", fg=C["text"], bg=C["bg2"],
                 font=("Consolas", 15, "bold")).pack(side="left")
        tk.Label(bar, text="  CSEN 602 — Spring 2026", fg=C["text3"],
                 bg=C["bg2"], font=("Segoe UI", 10)).pack(side="left")

        self.status_lbl = tk.Label(bar, textvariable=self.status_var,
                                   fg=C["text2"], bg=C["bg2"], font=UI)
        self.status_lbl.pack(side="right", padx=16)

        self.algo_lbl = tk.Label(bar, text="—", fg=C["amber"], bg=C["bg3"],
                                 font=("Consolas", 9, "bold"), padx=10, pady=4)
        self.algo_lbl.pack(side="right", padx=(0, 6))

        self.clock_lbl = tk.Label(bar, text="CLOCK: —", fg=C["amber"], bg=C["bg3"],
                                  font=("Consolas", 11, "bold"), padx=12, pady=4)
        self.clock_lbl.pack(side="right")

    # ── CONTROLS ─────────────────────────────────────────────────────────────

    def _build_controls(self):
        bar = tk.Frame(self.root, bg=C["bg2"], pady=7, padx=12)
        bar.pack(fill="x")

        # Row 1: exe + scheduler
        r1 = tk.Frame(bar, bg=C["bg2"])
        r1.pack(fill="x", pady=(0, 6))

        tk.Label(r1, text="Executable:", fg=C["text3"], bg=C["bg2"],
                 font=UI, width=11, anchor="w").pack(side="left")
        tk.Entry(r1, textvariable=self.cmd_var, fg=C["text"], bg=C["bg3"],
                 insertbackground=C["text"], relief="flat", font=MONO_L,
                 highlightthickness=1, highlightbackground=C["border2"],
                 highlightcolor=C["green"]).pack(side="left", fill="x",
                 expand=True, padx=(0, 6), ipady=4)
        btn(r1, "Browse…", self.browse_exe, fg=C["blue"]).pack(side="left", padx=(0, 16))

        tk.Label(r1, text="Scheduler:", fg=C["text3"], bg=C["bg2"], font=UI).pack(side="left", padx=(0, 6))
        for val, name in [("1", "RR"), ("2", "HRRN"), ("3", "MLFQ")]:
            tk.Radiobutton(r1, text=name, variable=self.sched_var, value=val,
                           fg=C["text2"], bg=C["bg2"], selectcolor=C["bg4"],
                           activeforeground=C["text"], activebackground=C["bg2"],
                           font=UI_B, relief="flat",
                           command=self._sched_changed).pack(side="left", padx=4)

        # Row 2: buttons + speed + current instr
        r3 = tk.Frame(bar, bg=C["bg2"])
        r3.pack(fill="x")

        btn(r3, "▶  Start",  self.start_backend, fg=C["green"], bg=C["green_bg"]).pack(side="left", padx=(0, 4))
        btn(r3, "⏸  Pause",  self.pause_play).pack(side="left", padx=4)
        btn(r3, "▶  Resume", self.resume_play, fg=C["blue"]).pack(side="left", padx=4)
        btn(r3, "⏭  Step",   self.step_cycle, fg=C["amber"]).pack(side="left", padx=4)
        btn(r3, "⏹  Stop",   self.stop_backend, fg=C["red"]).pack(side="left", padx=4)

        sep(r3, orient="v").pack(side="left", fill="y", padx=12)

        tk.Label(r3, text="Speed:", fg=C["text3"], bg=C["bg2"], font=UI).pack(side="left")
        tk.Scale(r3, from_=150, to=2000, orient="horizontal", variable=self.speed_var,
                 length=180, bg=C["bg2"], fg=C["text2"], troughcolor=C["bg4"],
                 highlightthickness=0, sliderrelief="flat", showvalue=False).pack(side="left", padx=6)
        self.speed_lbl = tk.Label(r3, text="700 ms", fg=C["text2"], bg=C["bg2"],
                                  font=MONO_S, width=6)
        self.speed_lbl.pack(side="left")
        self.speed_var.trace_add("write", lambda *_: self.speed_lbl.config(
            text=f"{self.speed_var.get()} ms"))

        self.instr_lbl = tk.Label(r3, text="—", fg=C["teal"], bg=C["bg2"],
                                  font=("Consolas", 10, "bold"))
        self.instr_lbl.pack(side="right", padx=12)
        tk.Label(r3, text="Executing:", fg=C["text3"], bg=C["bg2"], font=UI).pack(side="right")


    # ── INPUT ROW (always visible) ────────────────────────────────────────────

    def _build_input_row(self):
        row = tk.Frame(self.root, bg=C["bg2"], pady=7, padx=14)
        row.pack(fill="x")

        self.inp_dot = tk.Label(row, text="●", fg=C["text3"], bg=C["bg2"],
                                font=("Consolas", 11))
        self.inp_dot.pack(side="left", padx=(0, 8))

        self.inp_lbl = tk.Label(row, text="Program Input:", fg=C["text3"],
                                bg=C["bg2"], font=UI_B, width=16, anchor="w")
        self.inp_lbl.pack(side="left")

        self.inp_var = tk.StringVar()
        self.inp_entry = tk.Entry(row, textvariable=self.inp_var,
                                  fg=C["text"], bg=C["bg3"],
                                  insertbackground=C["text"], relief="flat",
                                  font=MONO_L,
                                  highlightthickness=2,
                                  highlightbackground=C["border"],
                                  highlightcolor=C["amber"],
                                  width=30)
        self.inp_entry.pack(side="left", ipady=5, padx=(0, 8))
        self.inp_entry.bind("<Return>", lambda e: self._send_input())

        self.inp_btn = btn(row, "Send ↵", self._send_input, fg=C["text3"], bg=C["bg3"])
        self.inp_btn.pack(side="left", padx=(0, 12))

        self.inp_hint = tk.Label(row, text="— waiting for simulation to request input —",
                                 fg=C["text3"], bg=C["bg2"], font=("Segoe UI", 8))
        self.inp_hint.pack(side="left")

    # ── BODY ─────────────────────────────────────────────────────────────────

    def _build_body(self):
        body = tk.Frame(self.root, bg=C["bg"])
        body.pack(fill="both", expand=True)

        left = tk.Frame(body, bg=C["bg"], width=310)
        left.pack(side="left", fill="y", padx=(12, 6), pady=12)
        left.pack_propagate(False)
        self._build_left(left)

        sep(body, orient="v").pack(side="left", fill="y", pady=12)

        center = tk.Frame(body, bg=C["bg"])
        center.pack(side="left", fill="both", expand=True, padx=6, pady=12)
        self._build_center(center)

        sep(body, orient="v").pack(side="left", fill="y", pady=12)

        right = tk.Frame(body, bg=C["bg"], width=370)
        right.pack(side="left", fill="y", padx=(6, 12), pady=12)
        right.pack_propagate(False)
        self._build_right(right)

    # ── LEFT COLUMN ──────────────────────────────────────────────────────────

    def _build_left(self, parent):
        # Running process
        po, pi = panel(parent, "▶  Running", C["green"])
        po.pack(fill="x", pady=(0, 6))
        self.run_pid_lbl = tk.Label(pi, text="None", fg=C["text3"],
                                    bg=C["bg2"], font=("Consolas", 18, "bold"))
        self.run_pid_lbl.pack(anchor="w", padx=10, pady=(6, 2))
        self.run_instr_lbl = tk.Label(pi, text="—", fg=C["teal"], bg=C["bg2"],
                                      font=MONO_L, wraplength=270, justify="left")
        self.run_instr_lbl.pack(anchor="w", padx=10, pady=(0, 6))

        # Queues
        qo, qi = panel(parent, "◈  Queues", C["amber"])
        qo.pack(fill="x", pady=(0, 6))
        self.ready_lbl   = self._q_row(qi, "Ready",   C["blue"])
        self.blocked_lbl = self._q_row(qi, "Blocked", C["amber"])
        self.mlfq_frame  = tk.Frame(qi, bg=C["bg2"])
        self.mlfq_frame.pack(fill="x", pady=(2, 0))
        self.mlfq_lbls = [self._q_row(self.mlfq_frame, f"Q{i} (q={2**i})", C["purple"])
                          for i in range(4)]

        # Mutexes
        mo, mi = panel(parent, "⬡  Mutexes", C["purple"])
        mo.pack(fill="x", pady=(0, 6))
        self.mx_lbls = {}
        for name in ("userInput", "userOutput", "file"):
            row = tk.Frame(mi, bg=C["bg2"])
            row.pack(fill="x", padx=10, pady=2)
            tk.Label(row, text=name, fg=C["text3"], bg=C["bg2"],
                     font=UI, width=12, anchor="w").pack(side="left")
            ml = tk.Label(row, text="free", fg=C["green"], bg=C["bg2"], font=MONO_S)
            ml.pack(side="left")
            self.mx_lbls[name] = ml
        tk.Frame(mi, height=4, bg=C["bg2"]).pack()

        # Disk
        do, di = panel(parent, "⬡  Disk Files", C["purple"])
        do.pack(fill="both", expand=True)
        tk.Label(di, text="Files on disk:", fg=C["text3"], bg=C["bg2"],
                 font=UI, anchor="w").pack(anchor="w", padx=10, pady=(6, 2))
        self.disk_box = ScrolledText(di, height=7, wrap="word", font=MONO_S,
                                     bg=C["bg"], fg=C["purple"],
                                     insertbackground=C["text"], relief="flat",
                                     padx=6, pady=6)
        self.disk_box.pack(fill="both", expand=True, padx=6, pady=(0, 6))
        self.disk_box.configure(state="disabled")

    def _q_row(self, parent, label, color):
        row = tk.Frame(parent, bg=C["bg2"])
        row.pack(fill="x", padx=10, pady=2)
        tk.Label(row, text=label, fg=C["text3"], bg=C["bg2"],
                 font=UI, width=12, anchor="w").pack(side="left")
        ml = tk.Label(row, text="[ ]", fg=color, bg=C["bg2"], font=MONO_S)
        ml.pack(side="left")
        return ml

    # ── CENTER COLUMN ─────────────────────────────────────────────────────────

    def _build_center(self, parent):
        # Process cards
        po, pi = panel(parent, "◉  Processes", C["blue"])
        po.pack(fill="x", pady=(0, 6))
        cg = tk.Frame(pi, bg=C["bg2"])
        cg.pack(fill="x", padx=8, pady=8)
        self.cards = {}
        for i, pid in enumerate([1, 2, 3]):
            c = ProcessCard(cg, pid)
            c.grid(row=0, column=i, sticky="nsew", padx=4)
            cg.grid_columnconfigure(i, weight=1)
            self.cards[pid] = c

        # Memory
        mo, mi = panel(parent, "▦  Memory (40 words)", C["amber"])
        mo.pack(fill="both", expand=True, pady=(0, 6))
        mg = tk.Frame(mi, bg=C["bg2"])
        mg.pack(fill="x", padx=8, pady=8)
        COLS = 8
        self.mem_cells = []
        for i in range(self.MEM):
            cell = MemCell(mg, i)
            r, c = divmod(i, COLS)
            cell.grid(row=r, column=c, padx=2, pady=2, sticky="nsew")
            mg.grid_columnconfigure(c, weight=1)
            self.mem_cells.append(cell)

        leg = tk.Frame(mi, bg=C["bg2"])
        leg.pack(anchor="w", padx=10, pady=(0, 6))
        for pid, label in [(1, "P1"), (2, "P2"), (3, "P3")]:
            tk.Label(leg, text="■", fg=PROC_PAL[pid]["fg"], bg=C["bg2"],
                     font=MONO_S).pack(side="left")
            tk.Label(leg, text=label, fg=C["text3"], bg=C["bg2"],
                     font=UI).pack(side="left", padx=(1, 10))
        tk.Label(leg, text="□  Free   │  Dashed border = PCB field   │  Active = highlighted",
                 fg=C["text3"], bg=C["bg2"], font=("Segoe UI", 8)).pack(side="left", padx=(4, 0))

        # Console output
        co, ci = panel(parent, "▸  Program Output", C["teal"])
        co.pack(fill="x")
        self.console = ScrolledText(ci, height=5, wrap="word", font=MONO,
                                    bg=C["bg"], fg=C["teal"],
                                    insertbackground=C["text"], relief="flat",
                                    padx=8, pady=6)
        self.console.pack(fill="x", padx=6, pady=6)
        self.console.configure(state="disabled")

    # ── RIGHT COLUMN ──────────────────────────────────────────────────────────

    def _build_right(self, parent):
        style = ttk.Style()
        style.theme_use("default")
        style.configure("T.TNotebook", background=C["bg2"], borderwidth=0)
        style.configure("T.TNotebook.Tab", background=C["bg3"], foreground=C["text3"],
                        padding=[10, 5], font=UI_B)
        style.map("T.TNotebook.Tab",
                  background=[("selected", C["bg2"])],
                  foreground=[("selected", C["text"])])

        nb = ttk.Notebook(parent, style="T.TNotebook")
        nb.pack(fill="both", expand=True)

        log_tab  = tk.Frame(nb, bg=C["bg2"])
        proc_tab = tk.Frame(nb, bg=C["bg2"])
        help_tab = tk.Frame(nb, bg=C["bg2"])
        nb.add(log_tab,  text="  Event Log  ")
        nb.add(proc_tab, text="  Process Table  ")
        nb.add(help_tab, text="  How to Use  ")

        # Log
        self.log_box = ScrolledText(log_tab, wrap="word", font=MONO,
                                    bg=C["bg"], fg=C["text2"],
                                    insertbackground=C["text"], relief="flat",
                                    padx=6, pady=6)
        self.log_box.pack(fill="both", expand=True, padx=6, pady=6)
        for tag, color in [("clock", C["amber"]), ("exec", C["green"]),
                            ("sync", C["blue"]), ("disk", C["purple"]),
                            ("inp", C["amber"]), ("out", C["teal"]),
                            ("arrive", C["teal"]), ("dim", C["text3"]),
                            ("err", C["red"])]:
            self.log_box.tag_configure(tag, foreground=color)

        # Process table
        cols = ("pid", "state", "pc", "arr", "burst", "wait", "mem", "disk")
        hdrs = {"pid": "PID", "state": "State", "pc": "PC", "arr": "Arr",
                "burst": "Burst", "wait": "Wait", "mem": "Memory", "disk": "Disk"}
        wids = {"pid": 40, "state": 80, "pc": 45, "arr": 45,
                "burst": 50, "wait": 50, "mem": 100, "disk": 50}
        style.configure("OS.Treeview",
                        background=C["bg"], foreground=C["text2"],
                        fieldbackground=C["bg"], rowheight=22,
                        font=MONO, borderwidth=0)
        style.configure("OS.Treeview.Heading",
                        background=C["bg3"], foreground=C["text3"],
                        font=UI_B, relief="flat")
        style.map("OS.Treeview",
                  background=[("selected", C["bg4"])],
                  foreground=[("selected", C["text"])])
        self.tree = ttk.Treeview(proc_tab, columns=cols, show="headings",
                                 height=10, style="OS.Treeview")
        for c in cols:
            self.tree.heading(c, text=hdrs[c])
            self.tree.column(c, width=wids[c], anchor="center")
        for pid in [1, 2, 3]:
            self.tree.tag_configure(f"p{pid}",
                                    background=PROC_PAL[pid]["bg"],
                                    foreground=PROC_PAL[pid]["fg"])
        self.tree.tag_configure("running", background=C["bg4"])
        self.tree.pack(fill="both", expand=True, padx=6, pady=6)

        # Help
        ht = ScrolledText(help_tab, wrap="word", font=MONO,
                          bg=C["bg"], fg=C["text2"], relief="flat",
                          padx=10, pady=10)
        ht.pack(fill="both", expand=True, padx=6, pady=6)
        ht.insert("1.0", (
            "  CSEN 602 OS Simulation Visualizer\n"
            "  ────────────────────────────────────\n\n"
            "  QUICK START\n"
            "  1. Paste your compiled exe path in Executable.\n"
            "  2. Pick scheduler: RR / HRRN / MLFQ.\n"
            "  3. Click ▶ Start.\n"
            "  4. When the simulation asks for input, the amber\n"
            "     bar lights up — type your value and press Enter.\n\n"
            "  HOW INPUTS WORK\n"
            "  When your C program prints \"Enter value:\", the GUI\n"
            "  detects it instantly (byte-by-byte reader) and shows\n"
            "  the amber input bar. Type your value and press Enter\n"
            "  or Send — it is written directly to the process stdin.\n\n"
            "  CONTROLS\n"
            "  ⏸ Pause  — freeze display after current cycle\n"
            "  ▶ Resume — continue playback\n"
            "  ⏭ Step   — advance exactly one cycle\n"
            "  ⏹ Stop   — terminate the backend process\n\n"
            "  EXAMPLE PATHS\n"
            "  Windows : simulation.exe\n"
            "  Full    : C:\\Users\\you\\project\\simulation.exe\n"
        ))
        ht.tag_configure("h", foreground=C["green"], font=("Consolas", 10, "bold"))
        ht.tag_add("h", "1.0", "2.0")
        ht.configure(state="disabled")

    # ── BACKEND CONTROL ───────────────────────────────────────────────────────

    def _sched_changed(self):
        name = {"1": "RR", "2": "HRRN", "3": "MLFQ"}.get(self.sched_var.get(), "—")
        self.algo_lbl.config(text=name)

    def browse_exe(self):
        path = filedialog.askopenfilename(title="Choose simulator executable")
        if path:
            self.cmd_var.set(path)

    def start_backend(self):
        if self.process and self.process.poll() is None:
            messagebox.showinfo("Already running", "Backend is already running.")
            return
        cmd = self.cmd_var.get().strip()
        if not cmd:
            messagebox.showerror("Missing", "Enter the executable path.")
            return
        try:
            args = shlex.split(cmd, posix=False)
            self.process = subprocess.Popen(
                args,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,          # no buffering — raw bytes
            )
        except Exception as e:
            messagebox.showerror("Launch failed", str(e))
            return

        self.reader_alive = True
        self.reader_thread = threading.Thread(target=self._reader, daemon=True)
        self.reader_thread.start()

        self.paused        = False
        self.step_mode     = False
        self.snap          = self._new_snap()
        self.last_clock    = None
        self.mem_capture   = False
        self.pending_buf   = []
        self.waiting_input = False
        self._console_done = 0
        self._inp_dim()
        self._clear_ui()

        try:
            self.process.stdin.write((self.sched_var.get() + "\n").encode())
            self.process.stdin.flush()
        except Exception:
            pass

        self.status_var.set("Running")
        self.status_lbl.config(fg=C["green"])
        self._sched_changed()
        self._sched_consume()

    def stop_backend(self):
        self.pause_play()
        self.reader_alive  = False
        self.waiting_input = False
        self._inp_dim()
        if self.process and self.process.poll() is None:
            try:
                self.process.terminate()
            except Exception:
                pass
        self.process = None
        self.status_var.set("Stopped")
        self.status_lbl.config(fg=C["red"])

    def pause_play(self):
        self.paused = True
        self.status_var.set("Paused")
        self.status_lbl.config(fg=C["amber"])
        if self.play_job:
            self.root.after_cancel(self.play_job)
            self.play_job = None

    def resume_play(self):
        if not self.process:
            return
        self.paused    = False
        self.step_mode = False
        self.status_var.set("Running")
        self.status_lbl.config(fg=C["green"])
        self._sched_consume()

    def step_cycle(self):
        if not self.process:
            return
        self.paused    = False
        self.step_mode = True
        self.status_var.set("Stepping…")
        self.status_lbl.config(fg=C["blue"])
        self._sched_consume(immediate=True)

    def _reader(self):
        buf = ""
        try:
            while self.reader_alive and self.process and self.process.stdout:
                raw = self.process.stdout.read(1)
                if not raw:
                    break
                ch = raw.decode("utf-8", errors="replace")
                if ch == "\n":
                    if buf:
                        self.out_q.put(buf)
                    buf = ""
                else:
                    buf += ch
                    stripped = buf.strip().lower()
                    if re.search(r'(please enter|enter value|enter a value|enter a number|enter your|input:|enter:|type a|type your)', stripped) and buf.rstrip().endswith(":"):
                        self.out_q.put(buf.rstrip())
                        buf = ""
        finally:
            if buf.strip():
                self.out_q.put(buf)
            self.out_q.put("__END__")

    def _sched_consume(self, immediate=False):
        if self.play_job:
            self.root.after_cancel(self.play_job)
            self.play_job = None
        delay = 10 if immediate else max(50, self.speed_var.get() // 4)
        self.play_job = self.root.after(delay, self._consume)

    # ── CONSUME + PARSE ───────────────────────────────────────────────────────

    def _consume(self):
        if self.paused:
            return

        processed = False
        flushed   = False

        while not self.out_q.empty():
            line = self.out_q.get()
            processed = True

            if line == "__END__":
                if not self.waiting_input:
                    self.status_var.set("Finished ✓")
                    self.status_lbl.config(fg=C["green"])
                    self.paused = True
                    self._flush()
                else:
                    self.out_q.put("__END__")
                return

            self._log(line)
            self.pending_buf.append(line)
            flushed = self._parse(line) or flushed

            if self.step_mode and flushed and not self.waiting_input:
                self.paused    = False
                self.step_mode = False
                self.status_var.set("Paused")
                self.status_lbl.config(fg=C["amber"])
                self.paused = True
                return

        if processed:
            self._render()

        if self.waiting_input:
            # _trigger_input will restart us via play_job when input is sent
            return

        if self.process and self.process.poll() is None and not self.paused:
            self.play_job = self.root.after(max(80, self.speed_var.get() // 3), self._consume)
        elif not processed and not self.paused:
            self.play_job = self.root.after(50, self._consume)

    def _parse(self, line):
        # Fix Windows encoding mojibake
        line = line.replace("ΓÇö", "—").replace("\x80\x94", "—")
        s = line.strip()

        # ── input prompt ──
        if re.search(r'(please enter|enter value|enter a value|enter a number|enter your|input:|enter:|type a|type your)', s, re.I):
            # Call directly — not deferred — so waiting_input is set before
            # _consume checks it at the end of its loop
            self._trigger_input(s)
            return False

        # ── config ──
        if "[CONFIG]" in s:
            self.snap["algorithm"] = s.replace("[CONFIG]", "").strip()
            return False

        # ── clock cycle header ──
        m = re.search(r'CLOCK CYCLE\s+(\d+)', s)
        if m:
            nc = int(m.group(1))
            if self.last_clock is not None and nc != self.last_clock:
                self._flush()
            self.snap["clock"] = nc
            self.last_clock = nc
            return False

        # ── queues line ──
        if s.startswith("Queues  | ") or s.startswith("Queues | "):
            self._parse_queues(s)
            return False

        # ── running line ──
        m = re.search(r'Running\s+\|\s+P(\d+)\s+\(pc=(\d+)\)', s)
        if m:
            self.snap["running_pid"] = f"P{m.group(1)}"
            self.snap["running_pc"]  = m.group(2)
            return False

        # ── exec line ──
        m = re.search(r'\[EXEC\]\s+P(\d+)\s+\|\s+instr\s+(\d+)/(\d+)\s+\|\s+(.*)', s)
        if m:
            self.snap["running_pid"]   = f"P{m.group(1)}"
            self.snap["current_instr"] = m.group(4).strip()
            return False

        # ── mutex sync ──
        if "[SYNC]" in s:
            self._parse_mutex(s)
            return False

        # ── disk / mem events ──
        if "[DISK]" in s or "[MEM]" in s:
            self.snap["disk_last"] = s
            self.snap["disk_lines"].append(s)
            return False

        # ── output lines ──
        m = re.search(r'\[OUT\]\s+(.*)', s)
        if m:
            self.snap["output_lines"].append(m.group(1).strip())
            return False
        if re.match(r'^\s{8,}', line) and s and not re.match(r'[+\[=]', s):
            self.snap["output_lines"].append(s)
            return False

        # ── memory block ──
        if "MEMORY STATE" in s:
            self.mem_capture = True
            self.snap["memory"] = ["---"] * self.MEM
            return False

        if "==" * 4 in s and self.mem_capture:
            self.mem_capture = False
            self._derive_procs()
            self._flush()
            return True

        if self.mem_capture:
            m = re.match(r'\[\s*(\d+)\]\s+(.*)', s)
            if m:
                idx = int(m.group(1))
                if 0 <= idx < self.MEM:
                    self.snap["memory"][idx] = m.group(2)
            return False

        return False

    def _parse_queues(self, line):
        rm = re.search(r'Ready:\[(.*?)\]', line)
        bm = re.search(r'Blocked:\[(.*?)\]', line)
        if rm:
            self.snap["ready"]   = self._items(rm.group(1))
        if bm:
            self.snap["blocked"] = self._items(bm.group(1))
        qm = re.findall(r'Q(\d)\(q=\d+\):\[(.*?)\]', line)
        if qm:
            mlfq = [[], [], [], []]
            for lvl, items in qm:
                i = int(lvl)
                if 0 <= i < 4:
                    mlfq[i] = self._items(items)
            self.snap["mlfq"] = mlfq

    def _items(self, s):
        s = s.strip()
        if not s or s == "-":
            return []
        return [x.strip() for x in s.split(",") if x.strip()]

    def _parse_mutex(self, line):
        acq = re.search(r'P(\d+)\s+acquired mutex "(.*?)"', line)
        rel = re.search(r'P(\d+)\s+released mutex "(.*?)"', line)
        blk = re.search(r'P(\d+)\s+blocked.*"(.*?)".*held by P(\d+)', line)
        if acq:
            n = acq.group(2)
            if n in self.snap["mutexes"]:
                self.snap["mutexes"][n] = f"P{acq.group(1)} holds"
        elif rel:
            n = rel.group(2)
            if n in self.snap["mutexes"]:
                self.snap["mutexes"][n] = "free"
        elif blk:
            n = blk.group(2)
            if n in self.snap["mutexes"]:
                self.snap["mutexes"][n] = f"P{blk.group(3)} holds, P{blk.group(1)} waiting"

    def _derive_procs(self):
        procs = {}
        cur   = None
        for cell in self.snap["memory"]:
            if cell == "---":
                continue
            if cell.startswith("pid "):
                try:
                    cur = int(cell.split()[1])
                    procs.setdefault(cur, {"pid": cur, "state": "-", "pc": "-",
                                           "arr": "-", "burst": "-", "wait": "-",
                                           "lower": None, "upper": None, "on_disk": "No"})
                except Exception:
                    cur = None
            elif cur is not None:
                p = procs[cur]
                for prefix, key in [("state ", "state"), ("pc ", "pc"),
                                     ("arrival ", "arr"), ("burst ", "burst"),
                                     ("waiting ", "wait")]:
                    if cell.startswith(prefix):
                        p[key] = cell[len(prefix):]
                if cell.startswith("lower "):
                    try: p["lower"] = int(cell[6:])
                    except Exception: pass
                if cell.startswith("upper "):
                    try: p["upper"] = int(cell[6:])
                    except Exception: pass
                if cell.startswith("onDisk "):
                    p["on_disk"] = "Yes" if cell.endswith("1") else "No"
        for pid, pd in self.snap["processes"].items():
            if pid not in procs:
                procs[pid] = pd
        self.snap["processes"] = procs

    def _flush(self, force=False):
        self._derive_procs()
        self._render()
        self.pending_buf = []

    # ── INPUT HANDLING ────────────────────────────────────────────────────────

    def _trigger_input(self, prompt):
        # Cancel any pending consume so it doesn't race with the input wait
        if self.play_job:
            self.root.after_cancel(self.play_job)
            self.play_job = None
        self.waiting_input = True
        display = prompt.strip() if prompt.strip() else "Enter value:"
        self.inp_lbl.config(text=display[:45], fg=C["amber"])
        self.inp_hint.config(text="⟵  type a value and press Enter or Send", fg=C["amber"])
        self.inp_dot.config(fg=C["amber"])
        self.inp_entry.config(highlightbackground=C["amber"])
        self.inp_btn.config(fg=C["amber"], bg=C["amber_bg"])
        self.inp_var.set("")
        self.inp_entry.focus_set()
        self.status_var.set("Waiting for input…")
        self.status_lbl.config(fg=C["amber"])
        self._start_blink()

    def _send_input(self):
        if not self.process or self.process.poll() is not None:
            return
        if not self.waiting_input:
            return
        value = self.inp_var.get().strip()
        if not value:
            self.inp_entry.config(highlightbackground=C["red"])
            self.root.after(600, lambda: self.inp_entry.config(highlightbackground=C["amber"]))
            return
        try:
            self.process.stdin.write((value + "\n").encode())
            self.process.stdin.flush()
        except Exception as e:
            messagebox.showerror("Input error", str(e))
            return
        self._log(f"  [INPUT] >>> {value}")
        self._inp_dim()
        self.status_var.set("Running")
        self.status_lbl.config(fg=C["green"])
        # Give C process time to process the input and write its response
        self.play_job = self.root.after(200, self._consume)

    def _inp_dim(self):
        self.waiting_input = False
        self._stop_blink()
        self.inp_dot.config(fg=C["text3"])
        self.inp_lbl.config(text="Program Input:", fg=C["text3"])
        self.inp_hint.config(text="— waiting for simulation to request input —", fg=C["text3"])
        self.inp_entry.config(highlightbackground=C["border"])
        self.inp_btn.config(fg=C["text3"], bg=C["bg3"])

    def _start_blink(self):
        self._stop_blink()
        self._blink_state = True
        def blink():
            if not self.waiting_input:
                return
            self.inp_dot.config(fg=C["amber"] if self._blink_state else C["amber_dim"])
            self._blink_state = not self._blink_state
            self._blink_job = self.root.after(500, blink)
        blink()

    def _stop_blink(self):
        if self._blink_job:
            self.root.after_cancel(self._blink_job)
            self._blink_job = None

    # ── LOG ───────────────────────────────────────────────────────────────────

    def _log(self, line):
        line = line.replace("ΓÇö", "—").replace("\x80\x94", "—")
        s = line.strip()
        if re.search(r'CLOCK CYCLE', s):
            tag = "clock"
        elif "[EXEC]" in s:
            tag = "exec"
        elif "[SYNC]" in s:
            tag = "sync"
        elif "[DISK]" in s or "[MEM]" in s:
            tag = "disk"
        elif "[INPUT]" in s:
            tag = "inp"
        elif "[OUT]" in s or re.match(r'^\s{8,}', line):
            tag = "out"
        elif "arrived" in s or ">> P" in s:
            tag = "arrive"
        elif re.search(r'(FINISHED|blocked|BLOCKED)', s):
            tag = "err"
        else:
            tag = "dim"
        self.log_box.insert("end", line + "\n", tag)
        self.log_box.see("end")

    # ── RENDER ────────────────────────────────────────────────────────────────

    def _render(self):
        snap = self.snap

        self.clock_lbl.config(text=f"CLOCK: {snap['clock']}")
        self.instr_lbl.config(text=snap.get("current_instr", "—") or "—")

        rpid_str = snap["running_pid"]
        rpid_num = None
        if rpid_str.startswith("P"):
            try: rpid_num = int(rpid_str[1:])
            except Exception: pass

        self.run_pid_lbl.config(
            text=rpid_str if rpid_str != "-" else "None",
            fg=PROC_PAL.get(rpid_num, {"fg": C["text3"]})["fg"])
        self.run_instr_lbl.config(text=snap.get("current_instr", "—") or "—")

        self.ready_lbl.config(text=self._fmt(snap["ready"]))
        self.blocked_lbl.config(text=self._fmt(snap["blocked"]))

        show_mlfq = self.sched_var.get() == "3" or any(snap["mlfq"])
        if show_mlfq:
            self.mlfq_frame.pack(fill="x", pady=(2, 0))
            for i, ml in enumerate(self.mlfq_lbls):
                ml.config(text=self._fmt(snap["mlfq"][i]))
        else:
            self.mlfq_frame.pack_forget()

        for name, ml in self.mx_lbls.items():
            val = snap["mutexes"].get(name, "free")
            if "waiting" in val:
                ml.config(text=val, fg=C["red"])
            elif "holds" in val:
                ml.config(text=val, fg=C["amber"])
            else:
                ml.config(text=val, fg=C["green"])

        self.disk_box.configure(state="normal")
        self.disk_box.delete("1.0", "end")
        # Show actual files present in the disk/ directory
        disk_dir = os.path.join(os.path.dirname(self.cmd_var.get().strip()), "disk")
        if not os.path.isdir(disk_dir):
            disk_dir = os.path.join(os.getcwd(), "disk")
        if os.path.isdir(disk_dir):
            files = sorted(f for f in os.listdir(disk_dir)
                           if os.path.isfile(os.path.join(disk_dir, f)))
            if files:
                self.disk_box.insert("1.0", "\n".join(f"  {f}" for f in files))
            else:
                self.disk_box.insert("1.0", "  (empty)")
        else:
            self.disk_box.insert("1.0", "  disk/ not found")
        self.disk_box.configure(state="disabled")

        # Console
        lines = snap.get("output_lines", [])
        self.console.configure(state="normal")
        for line in lines[self._console_done:]:
            self.console.insert("end", f"  {line}\n")
        self._console_done = len(lines)
        self.console.see("end")
        self.console.configure(state="disabled")

        # Process cards
        for pid, card in self.cards.items():
            p = snap["processes"].get(pid)
            if p:
                state = p.get("state", "-").upper()
                if p.get("on_disk") == "Yes":
                    state = "ON_DISK"
                elif pid == rpid_num:
                    state = "RUNNING"
                lo, hi = p.get("lower"), p.get("upper")
                mem = f"[{lo}–{hi}]" if lo is not None and hi is not None else "—"
                card.update(state, p.get("pc", "—"), p.get("burst", "—"),
                            p.get("wait", "—"), mem, p.get("on_disk", "No"))
            else:
                card.update("-", "—", "—", "—", "—", "No")

        # Tree
        for item in self.tree.get_children():
            self.tree.delete(item)
        for pid in sorted(snap["processes"].keys()):
            p = snap["processes"][pid]
            lo, hi = p.get("lower"), p.get("upper")
            mem = f"[{lo}–{hi}]" if lo is not None and hi is not None else "—"
            tags = ("running",) if pid == rpid_num else (f"p{pid}",)
            self.tree.insert("", "end", tags=tags,
                             values=(f"P{p['pid']}", p["state"], p["pc"],
                                     p["arr"], p["burst"], p["wait"],
                                     mem, p["on_disk"]))

        # Memory
        mem = snap["memory"]
        run_range = None
        if rpid_num:
            p = snap["processes"].get(rpid_num)
            if p and p.get("lower") is not None:
                run_range = (p["lower"], p["upper"])

        cur_pid   = None
        pcb_left  = 0
        for i, value in enumerate(mem):
            cell = self.mem_cells[i]
            if value == "---":
                cell.set_free()
                cur_pid  = None
                pcb_left = 0
                continue
            if value.startswith("pid "):
                try:
                    cur_pid  = int(value.split()[1])
                    pcb_left = 9
                except Exception:
                    cur_pid  = None
                    pcb_left = 0

            is_pcb = pcb_left > 0
            if pcb_left > 0:
                pcb_left -= 1

            active = run_range and run_range[0] <= i <= run_range[1]
            short  = value.split()[-1] if value.split() else "—"
            if cur_pid:
                cell.set_owned(cur_pid, short, is_pcb=is_pcb, active=active)
            else:
                cell.set_free()

    def _fmt(self, xs):
        return "[ — ]" if not xs else "[ " + "  ".join(xs) + " ]"

    # ── CLEAR UI ─────────────────────────────────────────────────────────────

    def _clear_ui(self):
        self.clock_lbl.config(text="CLOCK: —")
        self.instr_lbl.config(text="—")
        self.run_pid_lbl.config(text="None", fg=C["text3"])
        self.run_instr_lbl.config(text="—")
        self.ready_lbl.config(text="[ ]")
        self.blocked_lbl.config(text="[ ]")
        for ml in self.mlfq_lbls:
            ml.config(text="[ ]")
        for ml in self.mx_lbls.values():
            ml.config(text="free", fg=C["green"])
        self.disk_box.configure(state="normal")
        self.disk_box.delete("1.0", "end")
        self.disk_box.insert("1.0", "  (empty)")
        self.disk_box.configure(state="disabled")
        self.console.configure(state="normal")
        self.console.delete("1.0", "end")
        self.console.configure(state="disabled")
        self.log_box.delete("1.0", "end")
        for item in self.tree.get_children():
            self.tree.delete(item)
        for cell in self.mem_cells:
            cell.set_free()
        for card in self.cards.values():
            card.update("-", "—", "—", "—", "—", "No")

    # ── MISC ──────────────────────────────────────────────────────────────────

    def _seed_exe(self):
        for name in ["simulation", "os_simulator", "OS_Project", "main", "a.out"]:
            for ext in ["", ".exe"]:
                guess = os.path.join(os.getcwd(), name + ext)
                if os.path.exists(guess):
                    self.cmd_var.set(guess)
                    return

    def on_close(self):
        self.stop_backend()
        self.root.destroy()


# ─────────────────────────────────────────────────────────────────────────────

def main():
    root = tk.Tk()
    app = LiveOSGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()