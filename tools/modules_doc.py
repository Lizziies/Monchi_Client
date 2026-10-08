#!/usr/bin/env python3
"""Writes the implementation table of docs/MODULES.md from a module dump.

usage:
  MONCHI_DUMP_MODULES='Z:\\tmp\\modules.json' wine64 testhost.exe dll/Monchi.dll 4
  python3 tools/modules_doc.py /tmp/modules.json
"""
import json
import pathlib
import sys

names = ["HUD", "Visuell", "PvP", "Komfort", "Performance", "Server", "Spaß", "Client"]
tiers = {1: "Kern", 2: "Erwartet", 3: "Weitere", 4: "Extras"}

mods = json.load(open(sys.argv[1]))
doc = pathlib.Path(__file__).resolve().parent.parent / "docs" / "MODULES.md"
text = doc.read_text()
start = text.index("## Umsetzungsstand (Session B)")
head = text[:start]

out = ["## Umsetzungsstand (Session B)", ""]
out.append(
    f"Alle {len(mods)} Module sind in `dll/src/modules/Manager.cpp` registriert, bauen mit MinGW und laufen unter Wine mit den Demo-Daten "
    "(Modul \"Game Support\") ohne Absturz. Im echten Spiel getestet ist noch keins. \"Braucht\" nennt die Signaturen, ohne die das Modul grau bleibt; "
    "bei \"einer von\" reicht eine. Namen mit `fx.` sind Effekt-Kanäle, andere sind Daten-Signaturen, siehe `docs/SDK.md`. "
    "\"Stufe\" ist die Einordnung in `Tiers.cpp`, \"Einst.\" die Zahl der sichtbaren Einstellungen. Die Tabelle entsteht mit `tools/modules_doc.py` aus einem Modul-Dump."
)
out.append("")
for cat, label in enumerate(names):
    rows = sorted((m for m in mods if m["category"] == cat), key=lambda m: (m["sub"], m["name"]))
    if not rows:
        continue
    out.append(f"### {label} ({len(rows)})")
    out.append("")
    out.append("| Modul | Unterkategorie | Stufe | Einst. | Braucht | Hinweis |")
    out.append("|---|---|---|---|---|---|")
    for m in rows:
        sigs = ", ".join(f"`{s}`" for s in m["sigs"]) or "nichts"
        if m["anySig"] and len(m["sigs"]) > 1:
            sigs = "einer von " + sigs
        notes = []
        if m["risky"]:
            notes.append("Warnhinweis")
        for tag in ("info-others", "input", "timing", "chat"):
            if tag in m["tags"]:
                notes.append(tag)
        out.append(f"| {m['name']} | {m['sub'] or '–'} | {tiers.get(m['tier'], m['tier'])} | {m['settings']} | {sigs} | {', '.join(notes) or '–'} |")
    out.append("")

out += [
    "### Bewusst nicht gebaut",
    "",
    "- Reach, Killaura, Aim Assist, Autoclicker, Velocity, Scaffold, ESP, Fake Lag, Paket-Manipulation, FPS- und Ping-Spoof.",
    "- Eigene 2D-Hitboxen und eigene Boxen über den Bildschirm: sie würden durch Wände zeigen. Hitbox nutzt deshalb nur den Zeichenweg des Spiels.",
    "- Item-Texturen in Zählern und Replay-Clip: brauchen Spieldateien beziehungsweise Render-Zugriff, den der Client nicht hat.",
    "- DSCP/QoS-Markierung und Bandbreiten-Hinweis pro Programm im Netzwerk-Modul: brauchen Admin-Rechte beziehungsweise ETW, nur nach ausdrücklicher Zustimmung, später.",
    "",
]
doc.write_text(head + "\n".join(out))
print(f"{len(mods)} Module geschrieben")
