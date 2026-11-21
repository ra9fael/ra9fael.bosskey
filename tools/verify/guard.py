"""Check cover-scoped input in a small top-left window, never fullscreen.

An external process owns the focus target. Only received-character counts are
reported. A focused QML fallback exit is cleanup, not native-exit acceptance.
"""
import argparse
import ctypes
import importlib.util
import json
import os
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

import desktop

spec = importlib.util.spec_from_file_location("exit_check", Path(__file__).with_name("exit.py"))
exit_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exit_check)
u = exit_check.u
u.GetTopWindow.argtypes = [wintypes.HWND]
u.GetTopWindow.restype = wintypes.HWND
u.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
u.GetWindow.restype = wintypes.HWND
u.SetForegroundWindow.argtypes = [wintypes.HWND]
u.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
u.GetAncestor.restype = wintypes.HWND
u.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]


def chord(*keys):
    desktop.require_input_desktop()
    assert u.GetForegroundWindow(), "no foreground window"
    for flags, ordered in [(0, keys), (2, reversed(keys))]:
        for key in ordered:
            event = exit_check.INPUT(type=1)
            event.ki = exit_check.KEYBDINPUT(key, 0, flags, 0, None)
            assert u.SendInput(1, ctypes.byref(event), ctypes.sizeof(event)) == 1, "SendInput failed"
            time.sleep(0.03)


def taskbar_above(cover):
    probe = u.GetTopWindow(None)
    while probe and probe != cover:
        name = ctypes.create_unicode_buffer(256)
        u.GetClassNameW(probe, name, len(name))
        if name.value in ("Shell_TrayWnd", "Shell_SecondaryTrayWnd") and u.IsWindowVisible(probe):
            return True
        probe = u.GetWindow(probe, 2)
    return False


def wait_hidden(seconds=1.5):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if not exit_check.find_visible_cover():
            return True
        time.sleep(0.03)
    return False


def send_sequence():
    for char in "deadbeef":
        chord(ord(char.upper()))


def focus_cover(cover):
    u.SetForegroundWindow(cover)
    time.sleep(0.15)
    if u.GetForegroundWindow() != cover:
        point = wintypes.POINT()
        u.GetCursorPos(ctypes.byref(point))
        try:
            u.SetCursorPos(24, 24)
            u.mouse_event(2, 0, 0, 0, 0)
            u.mouse_event(4, 0, 0, 0, 0)
            time.sleep(0.15)
        finally:
            u.SetCursorPos(point.x, point.y)
    assert u.GetForegroundWindow() == cover, "cover is not foreground"


def visible_dialogs():
    result = set()
    hwnd = None
    while True:
        hwnd = u.FindWindowExW(None, hwnd, "#32770", None)
        if not hwnd:
            return result
        if u.IsWindowVisible(hwnd):
            result.add(hwnd)


def focus_target():
    import queue
    import threading
    import tkinter as tk

    root = tk.Tk()
    root.title("BossKeyFocusTarget")
    root.geometry("200x80+500+0")
    entry = tk.Entry(root)
    entry.pack()
    root.update()
    root.lift()
    entry.focus_force()
    commands = queue.Queue()

    def read_commands():
        for line in sys.stdin:
            commands.put(line.strip())

    def drain():
        while not commands.empty():
            command = commands.get()
            if command == "report":
                print(json.dumps({"received_characters": len(entry.get())}), flush=True)
            elif command == "quit":
                root.destroy()
                return
        root.after(20, drain)

    hwnd = u.GetAncestor(root.winfo_id(), 2)
    print(json.dumps({"hwnd": hwnd, "integrity": desktop.integrity(desktop.window_pid(hwnd))}), flush=True)
    threading.Thread(target=read_commands, daemon=True).start()
    root.after(20, drain)
    root.after(15000, root.destroy)
    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", action="store_true")
    parser.add_argument("--focus-only", action="store_true")
    parser.add_argument("--focused-exit", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--target-python", type=Path, default=Path(sys.executable))
    args = parser.parse_args()
    if args.target:
        focus_target()
        return 0
    desktop.require_input_desktop()
    cover = None
    for _ in range(160):
        cover = exit_check.find_visible_cover()
        if cover:
            break
        time.sleep(0.05)
    assert cover, "no visible cover"
    result = {"cover_rect": desktop.require_windowed(cover),
              "cover_integrity": desktop.integrity(desktop.window_pid(cover)), "shortcuts": []}
    time.sleep(0.5)
    focus_cover(cover)
    shortcuts = [
        ("Ctrl+Shift+Esc", (0xA2, 0xA0, 0x1B)),
        ("Shift+F10", (0xA0, 0x79)),
        ("Ctrl+Shift+Esc repeated", (0xA2, 0xA0, 0x1B)),
        ("Shift+F10 repeated", (0xA0, 0x79)),
        ("Alt+Tab", (0xA4, 0x09)), ("Alt+Esc", (0xA4, 0x1B)),
        ("Alt+F4", (0xA4, 0x73)), ("Win+D", (0x5B, 0x44)),
        ("Win+R", (0x5B, 0x52)), ("Win", (0x5B,)),
    ]
    for name, keys in ([] if args.focus_only else shortcuts):
        chord(*keys)
        time.sleep(0.15)
        ok = u.GetForegroundWindow() == cover and not taskbar_above(cover)
        result["shortcuts"].append({"name": name, "passed": ok})
        assert ok, name
    if args.focused_exit:
        send_sequence()
        result["focused_exit"] = wait_hidden()
        accepted = result["focused_exit"]
    else:
        env = dict(os.environ, PYTHONHOME=sys.base_prefix)
        target = subprocess.Popen([str(args.target_python), __file__, "--target"], text=True, env=env,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, encoding="utf-8")
        try:
            ready = json.loads(target.stdout.readline())
            hwnd = ready["hwnd"]
            u.SetForegroundWindow(hwnd)
            time.sleep(0.2)
            result["target_integrity"] = ready["integrity"]
            result["focus_removed"] = u.GetForegroundWindow() == hwnd and hwnd != cover
            assert result["focus_removed"], "external target did not obtain focus"
            chord(ord("X"))
            send_sequence()
            result["exit_without_focus"] = wait_hidden()
            target.stdin.write("report\n")
            target.stdin.flush()
            result.update(json.loads(target.stdout.readline()))
            accepted = result["exit_without_focus"] and result["received_characters"] == 0
        finally:
            if target.poll() is None:
                target.stdin.write("quit\n")
                target.stdin.flush()
                target.wait(timeout=5)
            target.stdin.close()
            target.stdout.close()
        if exit_check.find_visible_cover():
            focus_cover(cover)
            send_sequence()
            result["focused_cleanup_exit"] = wait_hidden()
    assert not exit_check.find_visible_cover(), "cover remains visible"
    before = visible_dialogs()
    chord(0x5B, 0x52)
    time.sleep(0.5)
    after = visible_dialogs() - before
    result["Win+R_restored_after_hide"] = u.GetForegroundWindow() in after
    if result["Win+R_restored_after_hide"]:
        chord(0x1B)
    result["accepted"] = accepted and result["Win+R_restored_after_hide"]
    text = json.dumps(result, indent=2) + "\n"
    print(text, end="")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    return 0 if result["accepted"] else 1


if __name__ == "__main__":
    sys.exit(main())
