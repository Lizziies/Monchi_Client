# Security

Monchi is beta software that injects native code into Minecraft. This review reduces identified risks; it is not a guarantee that the client is free of vulnerabilities or crashes.

Report suspected vulnerabilities through this repository's private vulnerability reporting feature when it is enabled. Do not include credentials, account names, Windows user paths, crash dumps or player data in a public issue. If private reporting is unavailable, open an issue asking for a private reporting channel without disclosing the vulnerability.

## Trust boundaries

- The launcher loads executable code. Only use builds and updates from a publisher you trust. Download checksums detect corruption; a checksum distributed alongside a binary is not an independent publisher signature.
- Online install secrets and session tokens are local runtime data and must never be committed. The client requires HTTPS outside loopback development and does not follow Online API redirects. Admin credentials belong in deployment secrets, not source code or client builds.
- Monchi Online identifies an installation with a random secret. It does not cryptographically verify Microsoft/Xbox ownership of a gamertag. A first claim can impersonate an unclaimed name. Cosmetic badges must not be used as an authentication or moderation decision.
- Online rate limits are local to a worker/store instance. They are not a global abuse-prevention system; a public deployment also needs edge rate limiting and operational monitoring. The standalone server trusts forwarded IP headers only with `TRUST_PROXY=1`; enable that only behind a proxy that replaces those headers.
- The local development server listens only on loopback. Do not expose it as a production service.
- Runtime hooks, offsets and custom cosmetic data still need testing against each supported Minecraft version. Missing signatures disable affected modules.

## Source publication review, 2026-10-06

The public repository starts from a fresh source snapshot. Private development history, local settings, credentials, logs, dumps and screenshots are excluded. The two existing Boy/Girl preview textures are included at the project owner's explicit request. First-party PNG text, EXIF and timestamp metadata is removed without changing the compressed pixel chunks. Third-party license notices and attribution are retained.

Fixed during this review: streamed request bodies now stop at 16 KiB before JSON parsing, byte limits include UTF-8 data, arrays and malformed UTF-8 are rejected, standalone request collectors reject oversize bodies, and Online credentials cannot be sent to a remote cleartext endpoint or redirected endpoint. Network responses also use fixed-size read buffers and reject incomplete or oversized API responses. Regression tests cover the new API limits and transport policy.

`tools/check_publish.py` checks tracked content and commit metadata. A local private-word list extends it with the owner's names and identifiers; that list itself is never exported. Generic checks are not a substitute for reviewing images, licenses or deployment settings.

An additional Gitleaks scan checks the fresh Git snapshot for known credential patterns. `.gitleaks.toml` retains its default rules and excludes only the exact ImGui keyboard-range comparison that produces a generic-key false positive in two vendored files.


## Signed updates from 0.1.1

The updater requires an ECDSA P-256 signature over the version-bound checksum manifest before accepting client or launcher downloads. The verification key ships in source and the launcher; the signing private key stays outside the repository and CI. Missing, altered or replayed manifests are rejected. GitHub workflows produce unsigned build artifacts for review, not automatic executable releases. Existing launchers cannot gain this verification before their first upgrade: that bootstrap still uses their original HTTPS and checksum checks. Authenticode signing and antivirus clearance are separate from these release signatures.

LeviLauncher is pinned to v0.3.13 with the SHA-256 verified against the actual official download. Unverified installed or downloaded executables are not launched. Adapted Flarial telemetry no longer constructs reports or reads a machine identifier, and legacy Flarial API calls are blocked.
