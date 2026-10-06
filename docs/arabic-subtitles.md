# Arabic subtitles: second TV test candidate

Based on upstream **v1.7.4**, commit `1eee19b64329768891f6776c47bb87a714eb8b3b`.
Target for final validation: LG C3, firmware 03.43.21. This is an experimental
candidate; passing host tests and ARM compilation does not establish TV success.

## Cause and approach

Downloaded SRT/VTT is parsed in `legenda.c`, wrapped in `player.c`, and drawn by
`text.c` with `TTF_RenderUTF8_Blended`. The webOS SDK's SDL_ttf build disables
HarfBuzz and FriBidi; the app supplies no alternate shaping or Unicode BiDi
layout on that path. A different font alone cannot supply that layout.

Authored ASS/SSA already uses `assrender.c`: libass with complex shaping,
statically linked HarfBuzz, FriBidi and FreeType, a rendering worker, and cached
GLES images. The candidate reuses this renderer only for parsed plain tracks
containing Arabic text. Pure LTR tracks retain the existing overlay. Authored ASS
retains its existing document, style and compatibility behavior.

`plainass.c` creates timed ASS events from logical Unicode text, preserving
join/direction controls and escaping literal ASS syntax. It does not reverse
strings. Generated styles use **Encoding -1** (automatic paragraph direction),
and the production renderer enables **whole-text layout** and **BiDi brackets**
for generated documents only. This matters: default ASS compatibility layout
can lay out separately styled runs left-to-right even when Arabic is joined.

A bundled Noto Naskh Arabic face supplies Arabic glyphs explicitly. Latin runs
use the selected user family. The SDK disables system font providers, so
relying on automatic fallback from unnamed in-memory fonts is insufficient.
The font is from `notofonts/noto-fonts`, `hinted/ttf/NotoNaskhArabic/`:
SHA256 `2f4b88e6ee50fa82c617e2d1d4ba18281cb1c6cd71c3af3ec64970c23995db4b`.
Its SIL OFL license is bundled beside it.

User size, color, bold, outline/shadow, background, opacity, delay and position
remain editable for generated subtitles. Style changes run on the existing
worker and invalidate cached paused frames. Generated tracks use screen layout
and the same control/next-episode positioning as the old plain overlay.
Packaged fonts are loaded once; system fonts are requested by the existing
authored ASS/attachment path. No new runtime libraries are required on the TV.

## Investigation of contributor work

Upstream issues [245](https://github.com/iqui27/nuvio-native-legacy/issues/245)
and [247](https://github.com/iqui27/nuvio-native-legacy/issues/247) describe Arabic
rendering; #247 does not establish a distinct encoding cause. BasimLFC's PR
was located in [Better-Nuvio #3](https://github.com/alenkpedro/Better-Nuvio/pull/3),
not this repository. It bundles Noto fonts but supplies no shaping/BiDi path.
The four upstream commits after 1.7.4 inspected on 2026-10-05 changed docs or
funding rather than this code. No complete reusable Arabic implementation was
found in the checked PRs or recent public fork branch heads.

Reusing libass avoids maintaining a second glyph layout engine. Rebuilding
SDL_ttf with HarfBuzz would still need correct Unicode BiDi paragraph/run layout
and platform packaging; a custom HarfBuzz/FriBidi glyph pipeline would duplicate
existing libass work. Neither is the smallest robust first candidate here.

## Build and install without a computer

Pushes to `fix/arabic-subtitles` run `.github/workflows/webos-arabic.yml`.
The workflow tests subtitles, builds with the pinned webOS ARM SDK, uploads an
artifact, and publishes a **prerelease** in this fork. It includes the IPK,
checksums, build information, test SRT, and `apps.json`/`manifest.json` for
Homebrew Channel. In Homebrew Channel Settings, add the release's **apps.json
download URL** as a repository, then select **Nuvio Legacy Arabic Test**.

The app ID is unchanged because playback permissions and data paths use it.
The package version is **1.7.7** so Homebrew can offer an upgrade; the internal
candidate label is **1.7.4-arabic.3**. This is not an official upstream 1.7.7.
It replaces the existing installation. For rollback, install upstream's
official [1.7.4 IPK](https://github.com/iqui27/nuvio-native-legacy/releases/download/v1.7.4/space.nuvio.native.legacy_1.7.4_arm.ipk).

`tools/ci-webos.sh` verifies SDK/package SHA256 values, uses clean assets from
the pinned public 1.7.4 package, compiles the modified sources, and packages
with `@webos-tools/cli` 3.2.6. `ci-public-config.py` recovers allowlisted compiled
application settings from that same official binary. It reads no TV/user data;
the temporary header is not logged, committed or uploaded. No personal API key
or account token is required for the build. Static subtitle libraries are
built with the repository's existing dependency recipe.

## Validation and limits

Host checks include Arabic lam-alef joining versus explicit ZWNJ, join-control
preservation, a colored Latin-run placement test for mixed RTL order, escaping,
production parser/worker routing, paused style updates, stale-generation
rejection, authored ASS isolation, and fallback to the English overlay.
Address/undefined-behavior sanitizers run with leak detection disabled (the
renderer's global worker lifetime is process-wide). Existing parser tests pass.
A standalone existing `ass_libass.sh` test needs additional CJK/karaoke font
fixtures in the host environment and did not pass there; it does not compile
this changed renderer. It is not counted as a passing check.

TV tests must reproduce the original external Arabic subtitles, mixed
Arabic/English/numbers/punctuation, multiple lines, settings, seeks, track
switches, English-only subtitles and authored ASS. Measure responsiveness and
watch for flashes, missing glyphs, crashes, or delay. Report photographs of the
same scenes before/after.

Windows-1256 encoding detection is not added by this candidate. Recognizable
disconnected/reordered UTF-8 glyphs point to layout, but an independently
misdecoded subtitle file can still fail. Existing parser limits (768 bytes per
cue, cue count cap, centisecond ASS timing) remain. Tracks containing other scripts alone retain the original renderer. Arabic
mixed with additional scripts and rare controls need broader language coverage.

## LG C3 feedback and font size follow-up

The user tested candidate 1 on 2026-10-06: Arabic was joined/readable and English
subtitles worked. Photographs showed Arabic noticeably smaller at 100%, while
Latin digits retained their normal size. Raising the shared size to 200% enlarged
English/digits as well. Other behaviors remain unconfirmed on the TV.

Candidate 2 scales only the Arabic face runs by 1.5 on both axes; Latin runs
explicitly reset to 1.0. The shared size slider and pure-English renderer retain
their existing behavior. The scale compensates for Noto Naskh's unusually tall
ascender/descender metrics, rather than changing the font file or scaling every
subtitle. With the bundled Inter face, host rendering at slider 100% measured
alef / Latin 5 / Latin H at 24 / 24 / 24 pixels; at 200% they measured 47 / 49 /
48 pixels. These ink measurements verify relative sizing, not exact equality
for every letter or font family. Arabic shaping and mixed-text ordering tests
still pass. After installing, start with the shared size at 100%.

## Candidate 3: dialogue markers

LG C3 feedback confirmed candidate 2 Arabic shaping, size and Latin digit rendering.
Speaker dashes appeared at the visual left in the reported dialogue. The exact
source SRT has not been captured, so its invisible controls remain unverified.
Host reproduction showed that a leading LRM (U+200E) causes the same placement;
a logical leading dash without it already renders correctly.

For a line with a leading speaker dash whose first substantive strong letter
is Arabic, the converter now prefixes RLM (U+200F). FriBidi, already linked
for libass, classifies strong letters. Text order, existing controls and
embedded Latin/numbers are preserved. Trailing dashes, attached numeric minus
signs, English-led dialogue and authored ASS are unchanged. This is applied
per explicit source line, before ASS escaping; no per-frame work or new library.

Color-separated renderer tests assert the dash is at the right of the text on
both dialogue lines, including LRM-prefixed and mixed Arabic/Latin/numeric
input. Real-TV confirmation of the affected dialogue is still pending.
