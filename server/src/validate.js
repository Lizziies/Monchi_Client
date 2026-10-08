const modes = ['solid', 'gradient', 'rainbow', 'pulse'];
const roles = ['', 'owner', 'staff'];

export const roleOk = (role) => roles.includes(role);

export const nameOk = (name) => typeof name === 'string' && /^[A-Za-z0-9 _.-]{1,32}$/.test(name) && name.trim() === name;

export const keyOf = (name) => name.toLowerCase();

// A free tag must not read like a badge that only the service hands out.
const badges = ['owner', 'staff', 'team', 'admin', 'mod', 'moderator', 'dev', 'developer', 'support', 'official'];

export function tagReserved(tag) {
  const plain = tag.toLowerCase().replace(/[^a-z]/g, '').replace(/^(monchi|mochi)/, '');
  return badges.includes(plain);
}

const hex = (value, fallback) => (typeof value === 'string' && /^#[0-9a-fA-F]{6}$/.test(value) ? value.toLowerCase() : fallback);

export function cleanStyle(raw) {
  const s = raw && typeof raw === 'object' ? raw : {};
  const mode = modes.includes(s.mode) ? s.mode : 'solid';
  const speed = Math.min(5, Math.max(0.1, Number.isFinite(s.speed) ? s.speed : 1));
  return {
    mode,
    a: hex(s.a, '#3ba7ec'),
    b: hex(s.b, '#ffffff'),
    speed: Math.round(speed * 100) / 100,
    heartColor: hex(s.heartColor, '#3ba7ec'),
    tag: typeof s.tag === 'string' ? [...s.tag].filter(c => c.codePointAt(0) >= 0x20 && c.codePointAt(0) !== 167).slice(0, 32).join('') : '',
    tagColor: hex(s.tagColor, '#3ba7ec'),
    heart: s.heart !== false,
  };
}

export function cleanWorn(raw) {
  if (!Array.isArray(raw)) return [];
  const out = [];
  for (const item of raw) {
    if (out.length >= 12) break;
    if (!item || typeof item.id !== 'string' || !/^[a-z0-9_]{1,40}$/.test(item.id)) continue;
    const tint = Array.isArray(item.tint) ? item.tint.slice(0, 8).map((c) => hex(c, '#ffffff')) : [];
    out.push({ id: item.id, tint });
  }
  return out;
}

export function cleanServer(raw) {
  if (typeof raw !== 'string') return '';
  return [...raw].filter((c) => c.codePointAt(0) >= 0x20).slice(0, 48).join('').trim();
}
