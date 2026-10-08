import math

import numpy as np

SS = 3


def bezier(pts, n=24):
    out = []
    for i in range(n + 1):
        t = i / n
        p = [np.array(q, float) for q in pts]
        while len(p) > 1:
            p = [a + (b - a) * t for a, b in zip(p, p[1:])]
        out.append(tuple(p[0]))
    return out


def along(path, t):
    seg = [math.dist(a, b) for a, b in zip(path, path[1:])]
    goal, acc = t * sum(seg), 0.0
    for (a, b), s in zip(zip(path, path[1:]), seg):
        if acc + s >= goal:
            k = (goal - acc) / s if s else 0
            return (a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k), math.atan2(b[1] - a[1], b[0] - a[0])
        acc += s
    a, b = path[-2], path[-1]
    return b, math.atan2(b[1] - a[1], b[0] - a[0])


class Art:
    """Grey + alpha painting in art units (x outward, y down), supersampled, premultiplied."""

    def __init__(self, w, h, texel):
        self.w, self.h, self.texel = w, h, texel
        self.pw, self.ph = round(w * texel), round(h * texel)
        xs = (np.arange(self.pw * SS) + 0.5) / (texel * SS)
        ys = (np.arange(self.ph * SS) + 0.5) / (texel * SS)
        self.X, self.Y = np.meshgrid(xs, ys)
        self.g = np.zeros_like(self.X)
        self.a = np.zeros_like(self.X)

    def put(self, mask, gray, alpha=1.0):
        m = np.clip(mask * alpha, 0, 1)
        gray = np.broadcast_to(np.asarray(gray, float), m.shape)
        self.g = gray * m + self.g * (1 - m)
        self.a = m + self.a * (1 - m)

    def inside(self, poly):
        X, Y = self.X, self.Y
        hit = np.zeros(X.shape, bool)
        n = len(poly)
        for i in range(n):
            (xi, yi), (xj, yj) = poly[i], poly[i - 1]
            cross = ((yi > Y) != (yj > Y)) & (X < (xj - xi) * (Y - yi) / (yj - yi + 1e-12) + xi)
            hit ^= cross
        return hit.astype(float)

    def dist(self, path, closed=False):
        X, Y = self.X, self.Y
        d = np.full(X.shape, 1e9)
        pts = list(path) + ([path[0]] if closed else [])
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            dx, dy = bx - ax, by - ay
            t = np.clip(((X - ax) * dx + (Y - ay) * dy) / (dx * dx + dy * dy + 1e-12), 0, 1)
            d = np.minimum(d, np.hypot(X - ax - dx * t, Y - ay - dy * t))
        return d

    def stroke(self, path, r0, r1):
        """Tapered stroke; returns (mask, normalised distance to the centre line 0..1)."""
        X, Y = self.X, self.Y
        best = np.full(X.shape, 1e9)
        seg = [math.dist(a, b) for a, b in zip(path, path[1:])]
        total, acc = sum(seg), 0.0
        for (a, b), s in zip(zip(path, path[1:]), seg):
            dx, dy = b[0] - a[0], b[1] - a[1]
            t = np.clip(((X - a[0]) * dx + (Y - a[1]) * dy) / (dx * dx + dy * dy + 1e-12), 0, 1)
            r = r0 + (r1 - r0) * (acc + t * s) / total
            best = np.minimum(best, np.hypot(X - a[0] - dx * t, Y - a[1] - dy * t) / r)
            acc += s
        return (best < 1).astype(float), np.clip(best, 0, 1)

    def disc(self, c, r):
        d = np.hypot(self.X - c[0], self.Y - c[1])
        return (d < r).astype(float), d / r

    def local(self, origin, angle):
        """Coordinates along (s) and across (n) a direction."""
        ca, sa = math.cos(angle), math.sin(angle)
        dx, dy = self.X - origin[0], self.Y - origin[1]
        return dx * ca + dy * sa, -dx * sa + dy * ca

    def pixels(self):
        g = (self.g * self.a).reshape(self.ph, SS, self.pw, SS).mean(axis=(1, 3))
        a = self.a.reshape(self.ph, SS, self.pw, SS).mean(axis=(1, 3))
        gray = np.where(a > 0, g / np.maximum(a, 1e-6), 0)
        out = np.zeros((self.ph, self.pw, 4), np.uint8)
        out[..., 0] = out[..., 1] = out[..., 2] = np.clip(gray, 0, 255).astype(np.uint8)
        out[..., 3] = np.clip(a * 255, 0, 255).astype(np.uint8)
        return out


def facet_shard(art, top, angle, length, width, lit=225, mid=180, dark=120, base_dark=0.7):
    """A three-facet gem hanging from `top` in direction `angle`. Returns (mask, u along, n across -1..1)."""
    s, n = art.local(top, angle)
    u = s / length
    half = np.where(u < 0.24, width * (0.62 + 0.38 * np.clip(u / 0.24, 0, 1)), width * np.clip((1 - u) / 0.76, 0, 1))
    mask = ((u > -0.05) & (u < 1) & (np.abs(n) < half)).astype(float)
    nn = n / np.maximum(half, 1e-6)
    gray = np.where(nn < -0.34, lit, np.where(nn < 0.34, mid, dark)) + 22 * np.clip(u, 0, 1)
    gray = gray * (base_dark + (1 - base_dark) * np.clip(u / 0.3, 0, 1))
    art.put(mask, gray)
    return mask, u, nn
