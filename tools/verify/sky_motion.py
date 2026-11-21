"""Unscaled far-sky crops and matched-star centroids, two seconds apart."""
import argparse
import json
import math
import time
from pathlib import Path

from PIL import Image, ImageChops, ImageStat
import desktop
import shot


def stars(image):
    width, height = image.size
    data = image.tobytes()
    brightness = [max(data[i:i + 3]) for i in range(0, len(data), 3)]
    result = []
    for y in range(4, height - 4):
        for x in range(4, width - 4):
            peak = brightness[y * width + x]
            if peak < 8:
                continue
            neighbours = [(brightness[yy * width + xx], -yy, -xx)
                          for yy in range(y - 2, y + 3) for xx in range(x - 2, x + 3)]
            if (peak, -y, -x) != max(neighbours):
                continue
            weights = [(xx, yy, max(0, brightness[yy * width + xx] - 3))
                       for yy in range(y - 2, y + 3) for xx in range(x - 2, x + 3)]
            total = sum(weight for _, _, weight in weights)
            result.append({"x": sum(xx * weight for xx, _, weight in weights) / total,
                           "y": sum(yy * weight for _, yy, weight in weights) / total,
                           "peak": peak})
    return result


def match_stars(first, second):
    a, b = stars(first), stars(second)
    candidates = sorted((math.hypot(left["x"] - right["x"], left["y"] - right["y"]), i, j)
                        for i, left in enumerate(a) for j, right in enumerate(b)
                        if math.hypot(left["x"] - right["x"], left["y"] - right["y"]) <= 7)
    used_a, used_b, matches = set(), set(), []
    for distance, i, j in candidates:
        if i in used_a or j in used_b:
            continue
        used_a.add(i)
        used_b.add(j)
        matches.append({"from": [round(a[i]["x"], 3), round(a[i]["y"], 3)],
                        "to": [round(b[j]["x"], 3), round(b[j]["y"], 3)],
                        "dx": round(b[j]["x"] - a[i]["x"], 3),
                        "dy": round(b[j]["y"] - a[i]["y"], 3),
                        "distance": round(distance, 3),
                        "peaks": [a[i]["peak"], b[j]["peak"]]})
    return matches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fixed-center", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cover = shot.find_visible_cover()
    assert cover, "no visible cover"
    geometry = desktop.require_windowed(cover)
    time.sleep(12)
    # Native physical pixels, same coordinates; the lower/right edge stays far
    # from the settled shadow and disk in the 480x270 verification window.
    box = (8, 8, 88, 232)
    shot.capture(cover, str(args.output / "settled.png"))
    first_at = shot.capture(cover, str(args.output / "sky-a.png"), box)
    time.sleep(max(0, first_at + 2 - time.monotonic()))
    second_at = shot.capture(cover, str(args.output / "sky-b.png"), box)
    first = Image.open(args.output / "sky-a.png").convert("RGB")
    second = Image.open(args.output / "sky-b.png").convert("RGB")
    difference = ImageChops.difference(first, second)
    data = difference.tobytes()
    changed = sum(max(data[i:i + 3]) >= 3 for i in range(0, len(data), 3))
    matches = match_stars(first, second)
    moving = [match for match in matches if match["distance"] >= 0.3]
    result = {"cover_rect": geometry, "crop": list(box), "scaled": False,
              "fixed_center_control": args.fixed_center,
              "interval_seconds": round(second_at - first_at, 4),
              "changed_pixels_at_least_3": changed,
              "mean_absolute_rgb_difference": ImageStat.Stat(difference).mean,
              "moving_matched_stars": len(moving), "matches": matches}
    print(json.dumps({key: value for key, value in result.items() if key != "matches"}, indent=2))
    (args.output / "motion.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    assert len(moving) >= 2, "not enough matched stars with measurable displacement"


if __name__ == "__main__":
    main()
