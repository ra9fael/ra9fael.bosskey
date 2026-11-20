# hostsim.py - drive the plugin the way the WinShun host does: pipes instead
# of a console, WS_APP_DIR set, then the JSON-RPC sequence (initialize, query,
# action). Reproduces crashes that the standalone preview cannot, and reports
# the exit code.
#
# Usage: python tools/verify/hostsim.py [show|noshow]   (default: show; the
# action below covers the screen — the script kills the plugin after 6 s.)
import json
import os
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(ROOT, "bosskey.exe")
WS_DIR = os.environ.get("BOSSKEY_WS_DIR", r"C:\Program Files\WinShun")


def reader(pipe, tag, out):
    for line in iter(pipe.readline, b""):
        out.append(line)
        print(f"[{tag}] {line.decode('utf-8', 'replace').rstrip()}")


def send(proc, obj):
    text = json.dumps(obj, ensure_ascii=False)
    print(f">>> {text}")
    proc.stdin.write((text + "\n").encode("utf-8"))
    proc.stdin.flush()


def main():
    do_show = "noshow" not in sys.argv
    settings = {}
    for arg in sys.argv[1:]:
        if arg.startswith("settings="):
            settings = json.loads(arg.split("=", 1)[1])
    env = dict(os.environ)
    env["WS_APP_DIR"] = WS_DIR
    proc = subprocess.Popen([EXE], env=env, cwd=ROOT,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    out, err = [], []
    threading.Thread(target=reader, args=(proc.stdout, "out", out), daemon=True).start()
    threading.Thread(target=reader, args=(proc.stderr, "err", err), daemon=True).start()

    send(proc, {"id": 1, "method": "initialize", "params": {
        "settings": settings, "locale": "zh-CN", "theme": "light",
        "pluginDir": ROOT, "dataDir": os.path.join(os.environ["LOCALAPPDATA"], "WinShun")}})
    time.sleep(1.0)  # the eager idle that crashed at startup
    send(proc, {"id": 2, "method": "query", "params": {
        "queryId": 1, "command": "cover", "trigger": "", "keyword": "boss",
        "text": "", "raw": "boss ", "limit": 200, "match": []}})
    time.sleep(0.5)
    if do_show:
        send(proc, {"id": 3, "method": "action", "params": {
            "command": "cover", "action": "cover", "text": "",
            "item": {"id": "cover:b", "data": {"scene": "bsod", "token": "b"}}}})
        time.sleep(6.0)  # cover stays up (autoHideSec=0): kill afterwards

    if proc.poll() is None:
        print(f"alive after the sequence, killing (cover must go)")
        proc.kill()
    code = proc.wait()
    print(f"exit: {code} ({hex(code & 0xFFFFFFFF)})")
    time.sleep(0.2)
    return 0 if (code & 0xFFFFFFFF) in (0, 0x40010004) else 1  # killed is fine


if __name__ == "__main__":
    sys.exit(main())
