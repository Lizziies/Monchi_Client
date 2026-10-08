# Abyss Crystal Wings

Dark spiked frame with deep violet crystal blades and a bright core line in every blade.

Rebuilt with `tools/cosmetics_line` (`wings.py`, `abyss()`). The first version had a nearly empty 64x64 texture whose UVs overlapped, tip bones with their own pivot far from the root (so the tips floated away from the wing), and plates tilted 48° upward. It rendered as loose fragments in the Mochi preview.

- Two bones per side at the same pivot: the frame leads, the blades swing wider and slightly later. Every layer hangs from the same root, so nothing can separate.
- Idle flap 12° at 0.45 Hz, spring physics: folds back when sprinting, lifts when jumping.
- Tints: Main (blades), Accent (glow lines, tips, gems), Frame (spar and spikes).
- 8 cubes, 15 KB.
