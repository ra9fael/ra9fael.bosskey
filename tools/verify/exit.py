# exit.py - verify the exit sequence: send `deadbeef` as real keypresses to
# the visible cover and check it hides. The cover must have focus (the preview
# brings itself to front); SendInput feeds the focused window.
#
# Usage: python tools/verify/exit.py [sequence]   (default deadbeef)
import ctypes
import sys
import time
from ctypes import wintypes

u = ctypes.windll.user32
u.SetProcessDPIAware()

u.FindWindowExW.restype = ctypes.c_void_p
u.FindWindowExW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_wchar_p]
u.IsWindowVisible.argtypes = [ctypes.c_void_p]
u.GetForegroundWindow.restype = ctypes.c_void_p


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD),
                ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD),
                ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [("ki", KEYBDINPUT), ("padding", ctypes.c_ubyte * 32)]
    _anonymous_ = ("u",)
    _fields_ = [("type", wintypes.DWORD), ("u", _U)]


def find_visible_cover():
    probe = None
    while True:
        probe = u.FindWindowExW(None, probe, None, "WinShunBossKeyCover")
        if not probe:
            return None
        if u.IsWindowVisible(probe):
            return probe


def send_text(text):
    # The matcher only listens for [a-z0-9]: the events' key texts carry them.
    for ch in text:
        vk = u.VkKeyScanW(ctypes.wintypes.WCHAR(ch)) & 0xFF
        down = INPUT(type=1)
        down.ki = KEYBDINPUT(vk, 0, 0, 0, None)
        up = INPUT(type=1)
        up.ki = KEYBDINPUT(vk, 0, 2, 0, None)  # KEYEVENTF_KEYUP
        assert u.SendInput(1, ctypes.byref(down), ctypes.sizeof(INPUT)) == 1, "SendInput keydown failed"
        time.sleep(0.02)
        assert u.SendInput(1, ctypes.byref(up), ctypes.sizeof(INPUT)) == 1, "SendInput keyup failed"
        time.sleep(0.02)


def main():
    sequence = sys.argv[1] if len(sys.argv) > 1 else "deadbeef"
    hwnd = None
    for _ in range(160):
        hwnd = find_visible_cover()
        if hwnd:
            break
        time.sleep(0.05)
    if not hwnd:
        print("FAIL: no visible cover window", file=sys.stderr)
        return 1
    time.sleep(0.5)  # let the focus settle on the cover
    fg = u.GetForegroundWindow()
    print(f"cover={hwnd:#x} foreground={fg:#x} focused={fg == hwnd}")
    send_text(sequence)
    time.sleep(1.0)
    if find_visible_cover():
        print("FAIL: cover still visible after typing " + sequence)
        return 1
    print("PASS: cover exited after typing " + sequence)
    return 0


if __name__ == "__main__":
    sys.exit(main())
