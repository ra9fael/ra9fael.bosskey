# shot.py - capture the bosskey preview cover to a PNG.
#
# The fullscreen cover cannot be captured with PrintWindow (the compositor
# gets the real pixels only on screen), so capture the screen region of the
# window with BitBlt, as binarylab's spike_shot.py does. No third-party
# modules: the PNG is assembled with zlib from the standard library.
#
# Usage: python tools/verify/shot.py <scene>.png   (run while a --autohide
# preview is up; the window is found by its fixed title).
#
# `--at 0.8,3,8` captures at those wall-clock offsets after the cover appears
# (one file per offset: <scene>-0.8s.png, <scene>-3.0s.png, ...) — how a scene
# that unfolds over time is documented frame by frame.
import ctypes
import struct
import sys
import time
import zlib
from ctypes import wintypes

u = ctypes.windll.user32
g = ctypes.windll.gdi32
u.SetProcessDPIAware()

u.FindWindowW.restype = ctypes.c_void_p
u.FindWindowW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p]
u.FindWindowExW.restype = ctypes.c_void_p
u.FindWindowExW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_wchar_p]
u.IsWindowVisible.argtypes = [ctypes.c_void_p]
u.GetWindowRect.argtypes = [ctypes.c_void_p, ctypes.POINTER(wintypes.RECT)]
u.GetDC.restype = ctypes.c_void_p
u.GetDC.argtypes = [ctypes.c_void_p]
u.ReleaseDC.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
g.CreateCompatibleDC.restype = ctypes.c_void_p
g.CreateCompatibleDC.argtypes = [ctypes.c_void_p]
g.CreateCompatibleBitmap.restype = ctypes.c_void_p
g.CreateCompatibleBitmap.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
g.SelectObject.restype = ctypes.c_void_p
g.SelectObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
g.BitBlt.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                     ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_uint32]
g.GetDIBits.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint]
g.DeleteObject.argtypes = [ctypes.c_void_p]
g.DeleteDC.argtypes = [ctypes.c_void_p]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wintypes.DWORD), ("biWidth", ctypes.c_long),
                ("biHeight", ctypes.c_long), ("biPlanes", wintypes.WORD),
                ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wintypes.DWORD),
                ("biClrImportant", wintypes.DWORD)]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def find_visible_cover():
    # One cover window per screen, all with the same title; the precreated ones
    # on other screens stay hidden. Walk Z-order for the visible one.
    hwnd = None
    for _ in range(160):  # up to 8 s: first start loads Qt and compiles QML
        probe = None
        while True:
            probe = u.FindWindowExW(None, probe, None, "WinShunBossKeyCover")
            if not probe:
                break
            if u.IsWindowVisible(probe):
                hwnd = probe
                break
        if hwnd:
            return hwnd
        time.sleep(0.05)
    return None


def capture(hwnd, path):
    wr = wintypes.RECT()
    u.GetWindowRect(hwnd, ctypes.byref(wr))
    w, h = wr.right - wr.left, wr.bottom - wr.top
    if w <= 0 or h <= 0:
        print("empty window rect", file=sys.stderr)
        return False

    screen = u.GetDC(None)
    memdc = g.CreateCompatibleDC(screen)
    bmp = g.CreateCompatibleBitmap(screen, w, h)
    old = g.SelectObject(memdc, bmp)
    g.BitBlt(memdc, 0, 0, w, h, screen, wr.left, wr.top, 0x00CC0020)  # SRCCOPY

    bi = BITMAPINFO()
    bi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.bmiHeader.biWidth = w
    bi.bmiHeader.biHeight = -h  # top-down rows
    bi.bmiHeader.biPlanes = 1
    bi.bmiHeader.biBitCount = 32
    bi.bmiHeader.biCompression = 0
    buf = (ctypes.c_ubyte * (w * h * 4))()
    g.GetDIBits(memdc, bmp, 0, h, buf, ctypes.byref(bi), 0)

    px = bytearray(bytes(buf))
    for i in range(0, len(px), 4):  # BGRA -> RGBA
        px[i], px[i + 2] = px[i + 2], px[i]
    row = w * 4
    raw = b"".join(b"\x00" + bytes(px[y * row:(y + 1) * row]) for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)

    g.SelectObject(memdc, old)
    g.DeleteObject(bmp)
    g.DeleteDC(memdc)
    u.ReleaseDC(None, screen)
    print(f"captured {w}x{h} -> {path}")
    return True


def main(argv):
    path = argv[0] if argv else "bosskey_cover.png"
    offsets = []
    for i, arg in enumerate(argv):
        if arg == "--at" and i + 1 < len(argv):
            offsets = sorted(float(v) for v in argv[i + 1].split(",") if v)
    hwnd = find_visible_cover()
    if not hwnd:
        print("no visible bosskey cover window", file=sys.stderr)
        return 1

    appeared = time.time()
    if not offsets:  # let the first frame settle
        time.sleep(0.6)
        return 0 if capture(hwnd, path) else 1

    stem = path[:path.rfind(".")] if "." in path else path
    ok = True
    for offset in offsets:
        remaining = offset - (time.time() - appeared)
        if remaining > 0:
            time.sleep(remaining)
        ok = capture(hwnd, f"{stem}-{offset:g}s.png") and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
