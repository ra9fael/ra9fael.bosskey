"""Windows acceptance facts; never change desktop, token or sandbox settings."""
import ctypes
from ctypes import wintypes

u = ctypes.WinDLL("user32", use_last_error=True)
k = ctypes.WinDLL("kernel32", use_last_error=True)
a = ctypes.WinDLL("advapi32", use_last_error=True)
u.OpenInputDesktop.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
u.OpenInputDesktop.restype = wintypes.HANDLE
u.CloseDesktop.argtypes = [wintypes.HANDLE]
u.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
u.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
k.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
k.OpenProcess.restype = wintypes.HANDLE
k.CloseHandle.argtypes = [wintypes.HANDLE]
a.OpenProcessToken.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)]
a.GetTokenInformation.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                 wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
a.GetSidSubAuthorityCount.argtypes = [ctypes.c_void_p]
a.GetSidSubAuthorityCount.restype = ctypes.POINTER(ctypes.c_ubyte)
a.GetSidSubAuthority.argtypes = [ctypes.c_void_p, wintypes.DWORD]
a.GetSidSubAuthority.restype = ctypes.POINTER(wintypes.DWORD)


def require_input_desktop():
    handle = u.OpenInputDesktop(0, False, 0x100)
    if not handle:
        raise RuntimeError("interactive input desktop unavailable; acceptance cannot run")
    u.CloseDesktop(handle)


def window_pid(hwnd):
    pid = wintypes.DWORD()
    if not u.GetWindowThreadProcessId(hwnd, ctypes.byref(pid)):
        raise ctypes.WinError(ctypes.get_last_error())
    return pid.value


def token_facts(pid):
    process = k.OpenProcess(0x1000, False, pid)
    if not process:
        raise ctypes.WinError(ctypes.get_last_error())
    token = wintypes.HANDLE()
    try:
        if not a.OpenProcessToken(process, 8, ctypes.byref(token)):
            raise ctypes.WinError(ctypes.get_last_error())
        needed = wintypes.DWORD()
        a.GetTokenInformation(token, 25, None, 0, ctypes.byref(needed))
        data = ctypes.create_string_buffer(needed.value)
        if not a.GetTokenInformation(token, 25, data, len(data), ctypes.byref(needed)):
            raise ctypes.WinError(ctypes.get_last_error())
        sid = ctypes.cast(data, ctypes.POINTER(ctypes.c_void_p))[0]
        count = a.GetSidSubAuthorityCount(sid)[0]
        level = a.GetSidSubAuthority(sid, count - 1)[0]
        ui_access = wintypes.DWORD()
        if not a.GetTokenInformation(token, 26, ctypes.byref(ui_access), 4, ctypes.byref(needed)):
            raise ctypes.WinError(ctypes.get_last_error())
        restricted = wintypes.DWORD()
        if not a.GetTokenInformation(token, 21, ctypes.byref(restricted), 4, ctypes.byref(needed)):
            raise ctypes.WinError(ctypes.get_last_error())
        return {"integrity": level, "restricted": bool(restricted.value),
                "ui_access": bool(ui_access.value)}
    finally:
        if token:
            k.CloseHandle(token)
        k.CloseHandle(process)


def integrity(pid):
    return token_facts(pid)["integrity"]


def require_windowed(hwnd):
    rect = wintypes.RECT()
    if not u.GetWindowRect(hwnd, ctypes.byref(rect)):
        raise ctypes.WinError(ctypes.get_last_error())
    box = [rect.left, rect.top, rect.right, rect.bottom]
    if rect.left != 0 or rect.top != 0 or not (0 < rect.right <= 800 and 0 < rect.bottom <= 500):
        raise RuntimeError(f"verification requires a small top-left cover, found {box}")
    return box
