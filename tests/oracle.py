"""Independent float64 NumPy FFT implementation of DFTTest2's public semantics.

No project shader or DFTTest2 implementation is imported. NumPy's normalized
inverse FFT is explicitly converted to the unnormalized inverse used upstream.
This oracle is for small validation images, not performance measurements.
"""
from __future__ import annotations

import math
import numpy as np


def window_value(x, size, mode, beta):
    a = math.pi * x / size
    coefficients = {
        0: (0.5, -0.5),
        1: (0.53836, -0.46164),
        2: (0.42, -0.5, 0.08),
        3: (0.35875, -0.48829, 0.14128, -0.01168),
        5: (0.27105140069342415, -0.433297939234486060,
            0.218122999543110620, -0.065925446388030898,
            0.010811742098372268, -7.7658482522509342e-4,
            1.3887217350903198e-5),
        6: (0.2810639, -0.5208972, 0.1980399),
        10: (0.355768, -0.487396, 0.144232, -0.012604),
        11: (0.3635819, -0.4891775, 0.1365995, -0.0106411),
    }
    if mode in coefficients:
        return sum(c * math.cos(2 * i * a) for i, c in enumerate(coefficients[mode]))
    if mode == 4:
        # Upstream evaluates a finite modified-Bessel series, so use that
        # definition rather than np.i0's slightly different convergence.
        def i0(p):
            p /= 2
            n = d = total = 1.0
            for k in range(1, 15):
                n *= p
                d *= k
                v = n / d
                total += v * v
                if v <= 1e-8:
                    break
            return total
        v = 2 * x / size - 1
        return i0(math.pi * beta * math.sqrt(max(1 - v * v, 0))) / i0(math.pi * beta)
    if mode == 7:
        return 1.0
    if mode == 8:
        return 1 - 2 * abs(x - size / 2) / size
    if mode == 9:
        # Preserve the actual upstream formula, including its asymmetry.
        return 0.62 - 0.48 * (x / size - 0.5) - 0.38 * math.cos(2 * a)
    raise ValueError(f"Unknown window {mode}")


def constants(params):
    bs = params.get("sbsize", 16)
    ts = params.get("tbsize", 3)
    step = bs - params.get("sosize", 12)
    sw = np.array([window_value(i + 0.5, bs, params.get("swin", 0),
                                params.get("sbeta", 2.5)) for i in range(bs)])
    # Every sample with the same residue modulo the block step contributes.
    sw /= np.sqrt(np.array([sum(sw[j] ** 2 for j in range(i % step, bs, step))
                            for i in range(bs)]))
    tw = np.array([window_value(i + 0.5, ts, params.get("twin", 7),
                                params.get("tbeta", 2.5)) for i in range(ts)])
    window = tw[:, None, None] * sw[None, :, None] * sw[None, None, :]
    window /= math.sqrt(ts) * bs
    wscale = np.sum(window * window)
    shape = (ts, bs, bs // 2 + 1)
    sigma = np.full(shape, params.get("sigma", 8.0), dtype=np.float64)

    slocation = params.get("slocation")
    axes = [params.get("ssx"), params.get("ssy"), params.get("sst")]
    if slocation is not None or any(a is not None for a in axes):
        ssystem = params.get("ssystem", 0)
        ndim = (1 if ts > 1 else 0) + (2 if bs > 1 else 0)
        def norm(value):
            if slocation is not None and ssystem == 1:
                return value
            # A scalar 1x1x1 block has only DC. Its defined curve behavior is
            # direct evaluation of the temporal/common curve at zero.
            return value ** (1 / max(ndim, 1))
        def function(points):
            if points is None:
                return lambda _: norm(params.get("sigma", 8.0))
            p = np.array(points, dtype=np.float64).reshape(-1, 2)
            p = p[np.argsort(p[:, 0])]
            if len(p) < 2 or p[0, 0] != 0 or p[-1, 0] != 1:
                raise ValueError("Oracle profiles must span frequencies 0..1")
            return lambda x: np.interp(x, p[:, 0], np.array([norm(v) for v in p[:, 1]]))
        fx, fy, ft = ([function(slocation)] * 3 if slocation is not None
                      else [function(a) for a in axes])
        def location(i, n):
            return 0.0 if n == 1 else min(i, n - i) / (n // 2)
        for t in range(ts):
            lt = location(t, ts)
            for y in range(bs):
                ly = location(y, bs)
                for x in range(bs // 2 + 1):
                    lx = location(x, bs)
                    if ndim == 0:
                        sigma[t, y, x] = ft(0)
                    elif ssystem == 0:
                        sigma[t, y, x] = (fx(lx) * fy(ly) if bs > 1 else 1) * (ft(lt) if ts > 1 else 1)
                    else:
                        sigma[t, y, x] = ft(math.sqrt((lt * lt + ly * ly + lx * lx)
                                                                    / ndim))
    if params.get("ftype", 0) < 2:
        sigma *= wscale
    return window, sigma, params.get("sigma2", 8.0) * (wscale if params.get("ftype", 0) < 2 else 1), params.get("pmin", 0.0) * wscale, params.get("pmax", 500.0) * wscale


def filter_frequency(freq, sigma, sigma2, pmin, pmax, params):
    ftype = params.get("ftype", 0)
    psd = freq.real ** 2 + freq.imag ** 2
    if ftype == 0:
        beta = params.get("f0beta", 1.0)
        # DFTTest2 selects specialization in these public tolerance bands.
        beta = 1 if abs(beta - 1) < 0.00005 else 0.5 if abs(beta - 0.5) < 0.0005 else beta
        multiplier = np.maximum((psd - sigma) / (psd + 1e-15), 0) ** beta
    elif ftype == 1:
        multiplier = psd >= sigma
    elif ftype == 2:
        multiplier = sigma
    elif ftype == 3:
        multiplier = np.where((psd >= pmin) & (psd <= pmax), sigma, sigma2)
    elif ftype == 4:
        multiplier = sigma * np.sqrt(psd * pmax / ((psd + pmin) * (psd + pmax) + 1e-15))
    else:
        raise ValueError(f"Unknown ftype {ftype}")
    return freq * multiplier


def plane(frames, n, params, bits, floating=False):
    """Filter one plane; frames is a [frame][height,width] sequence."""
    bs = params.get("sbsize", 16)
    ts = params.get("tbsize", 3)
    if ts < 1 or ts % 2 != 1:
        raise ValueError("Oracle supports odd tbsize")
    step = bs - params.get("sosize", 12)
    if not 1 <= step <= bs:
        raise ValueError("Invalid overlap")
    h, w = frames[n].shape
    def pad_size(s):
        return ((s + bs - 1) // bs) * bs + 2 * max(bs - step, step)
    ph, pw = pad_size(h), pad_size(w)
    oy, ox = (ph - h) // 2, (pw - w) // 2
    scale = 255.0 if floating else 1.0 / (1 << (bits - 8))
    radius = ts // 2
    padded = np.stack([np.pad(np.asarray(frames[min(max(n + t, 0), len(frames) - 1)],
                                           dtype=np.float64) * scale,
                              ((oy, ph - h - oy), (ox, pw - w - ox)), mode="reflect")
                       for t in range(-radius, radius + 1)])
    window, sigma, sigma2, pmin, pmax = constants(params)
    window_freq = np.fft.rfftn(window * 255)
    acc = np.zeros((ph, pw), dtype=np.float64)
    for y in range(0, ph - bs + 1, step):
        for x in range(0, pw - bs + 1, step):
            freq = np.fft.rfftn(padded[:, y:y + bs, x:x + bs] * window)
            if params.get("zmean", True):
                mean = freq.flat[0].real / window_freq.flat[0].real
                freq -= mean * window_freq
            freq = filter_frequency(freq, sigma, sigma2, pmin, pmax, params)
            if params.get("zmean", True):
                freq += mean * window_freq
            inverse = np.fft.irfftn(freq, s=(ts, bs, bs)) * (ts * bs * bs)
            acc[y:y + bs, x:x + bs] += inverse[radius] * window[radius]
    result = acc[oy:oy + h, ox:ox + w] / scale
    if floating:
        return result.astype(frames[n].dtype)
    return np.clip(np.floor(result + 0.5), 0, (1 << bits) - 1).astype(frames[n].dtype)
