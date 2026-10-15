#!/usr/bin/env python3
"""Compare retained original/recompiled frames without silently changing geometry."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

try:
    import numpy as np
    from PIL import Image
except ImportError as error:  # pragma: no cover - dependency error path
    raise SystemExit("render_diff.py requires Pillow and numpy") from error


def parse_crop(value: str) -> tuple[int, int, int, int]:
    try:
        x, y, width, height = (int(part) for part in value.split(","))
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("crop must be x,y,width,height") from error
    if min(x, y) < 0 or width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("crop coordinates must be non-negative and dimensions positive")
    return x, y, width, height


def crop(image: Image.Image, spec: tuple[int, int, int, int] | None) -> Image.Image:
    if spec is None:
        return image
    x, y, width, height = spec
    if x + width > image.width or y + height > image.height:
        raise ValueError(f"crop {spec} is outside image {image.size}")
    return image.crop((x, y, x + width, y + height))


def luminance(rgb: np.ndarray) -> np.ndarray:
    return 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]


def global_ssim(a: np.ndarray, b: np.ndarray) -> float:
    # Deterministic whole-frame SSIM-like score. It is intentionally named
    # global_ssim: this is not a replacement for windowed skimage SSIM.
    mu_a = float(a.mean())
    mu_b = float(b.mean())
    var_a = float(a.var())
    var_b = float(b.var())
    covariance = float(((a - mu_a) * (b - mu_b)).mean())
    c1 = (0.01 * 255.0) ** 2
    c2 = (0.03 * 255.0) ** 2
    denominator = (mu_a * mu_a + mu_b * mu_b + c1) * (var_a + var_b + c2)
    return ((2 * mu_a * mu_b + c1) * (2 * covariance + c2)) / denominator if denominator else 1.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--recomp", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("evidence/render-diff"))
    parser.add_argument("--original-crop", type=parse_crop)
    parser.add_argument("--recomp-crop", type=parse_crop)
    parser.add_argument(
        "--resize",
        choices=("none", "recomp-to-original", "original-to-recomp"),
        default="none",
        help="geometry changes are opt-in; default is to fail on mismatched dimensions",
    )
    parser.add_argument("--pixel-threshold", type=float, default=16.0)
    parser.add_argument("--min-global-ssim", type=float)
    parser.add_argument("--max-mae", type=float)
    parser.add_argument("--max-changed-ratio", type=float)
    args = parser.parse_args()

    if args.pixel_threshold < 0:
        parser.error("--pixel-threshold must be non-negative")

    original = crop(Image.open(args.original).convert("RGB"), args.original_crop)
    recomp = crop(Image.open(args.recomp).convert("RGB"), args.recomp_crop)
    if original.size != recomp.size:
        if args.resize == "none":
            parser.error(
                f"image geometry differs: original={original.size}, recomp={recomp.size}; "
                "use explicit crops or --resize"
            )
        if args.resize == "recomp-to-original":
            recomp = recomp.resize(original.size, Image.Resampling.LANCZOS)
        else:
            original = original.resize(recomp.size, Image.Resampling.LANCZOS)

    a = np.asarray(original, dtype=np.float32)
    b = np.asarray(recomp, dtype=np.float32)
    absolute = np.abs(a - b)
    per_pixel = absolute.mean(axis=2)
    squared = (a - b) ** 2
    metrics = {
        "width": original.width,
        "height": original.height,
        "mae": float(absolute.mean()),
        "rmse": float(math.sqrt(float(squared.mean()))),
        "p95_abs_error": float(np.percentile(absolute, 95)),
        "max_abs_error": float(absolute.max()),
        "changed_ratio": float((per_pixel > args.pixel_threshold).mean()),
        "pixel_threshold": args.pixel_threshold,
        "global_ssim": float(global_ssim(luminance(a), luminance(b))),
        "resize_mode": args.resize,
        "original_crop": args.original_crop,
        "recomp_crop": args.recomp_crop,
    }

    failures: list[str] = []
    if args.min_global_ssim is not None and metrics["global_ssim"] < args.min_global_ssim:
        failures.append(f"global_ssim {metrics['global_ssim']:.6f} < {args.min_global_ssim:.6f}")
    if args.max_mae is not None and metrics["mae"] > args.max_mae:
        failures.append(f"mae {metrics['mae']:.6f} > {args.max_mae:.6f}")
    if args.max_changed_ratio is not None and metrics["changed_ratio"] > args.max_changed_ratio:
        failures.append(
            f"changed_ratio {metrics['changed_ratio']:.6f} > {args.max_changed_ratio:.6f}"
        )
    metrics["status"] = "fail" if failures else "pass"
    metrics["failures"] = failures

    args.out.mkdir(parents=True, exist_ok=True)
    diff_rgb = np.clip(absolute, 0, 255).astype(np.uint8)
    Image.fromarray(diff_rgb, mode="RGB").save(args.out / "diff.png")
    heat = np.clip(per_pixel * 4.0, 0, 255).astype(np.uint8)
    Image.fromarray(heat, mode="L").save(args.out / "heatmap.png")
    (args.out / "metrics.json").write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    report = [
        "# Render diff",
        "",
        f"- status: **{metrics['status'].upper()}**",
        f"- geometry: `{original.width}x{original.height}`",
        f"- global SSIM: `{metrics['global_ssim']:.6f}`",
        f"- MAE: `{metrics['mae']:.6f}`",
        f"- RMSE: `{metrics['rmse']:.6f}`",
        f"- p95 absolute error: `{metrics['p95_abs_error']:.6f}`",
        f"- changed pixels (> {args.pixel_threshold:g} mean RGB error): `{metrics['changed_ratio']:.4%}`",
        f"- resize mode: `{args.resize}`",
        "- artifacts: `diff.png`, `heatmap.png`, `metrics.json`",
    ]
    if failures:
        report.extend(["", "## Failed gates", ""] + [f"- {failure}" for failure in failures])
    (args.out / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print(
        f"global_ssim={metrics['global_ssim']:.6f} mae={metrics['mae']:.6f} "
        f"changed_ratio={metrics['changed_ratio']:.6f} status={metrics['status']}"
    )
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
