# Arabic interface test for LG webOS

Based on the TV-tested subtitle branch at `310c9da10271914d462d949b4f0e68b3b868be43` (upstream 2.0.0).

## Scope

Adds Arabic as language ID 30, preserving all existing persisted IDs. The catalog contains all 4762 master keys, manually reviewed in Arabic, with the original printf placeholders, line breaks and markup. Arabic regional locales and `ara` are recognized. Menu positions, remote navigation, and video controls retain the existing layout. Arabic metadata selection is deferred; TMDB's follow-interface option uses en-US for this new UI language. Existing explicit metadata choices are unchanged.

UI strings containing Arabic use Noto Sans Arabic with the already-linked FreeType, HarfBuzz and FriBidi libraries. Text stays in logical Unicode order; layout resolves embedding levels, shapes logical runs, and draws them in visual order. Measurement and rasterization share the same layout, including ink bounds. Inter Display supplies missing symbols such as navigation arrows/checkmarks. Font objects are lazy, textures and widths use the existing caches. No new linked dependency is added.

Letter-spaced headings draw as a complete Arabic label to preserve joining. Arabic tour/help lines flatten inline bold spans into regular text to preserve paragraph order; other languages retain their existing rich text. Long texture cache keys include a hash of the complete text to prevent collisions between strings with a shared UTF-8 prefix. Subtitle styles are explicitly excluded from the UI renderer, and existing subtitle files/routing are unchanged.

## Fonts

Noto Sans Arabic version 2.012, SIL Open Font License 1.1 (bundled as `NotoSansArabic-OFL.txt`). Source: Google Fonts, `ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf`:
https://github.com/google/fonts/tree/main/ofl/notosansarabic

Static instances created with fontTools `instantiateVariableFont`, width 100, weights 400 and 700. Full character coverage is retained (Arabic, Latin, digits and punctuation), rather than an Arabic-only subset. No glyph outlines were edited.

SHA256:
- Regular: `48a4653fd32d2d5c5d8e7268b8abe6fb125d158c766077e7135bf8d193e8b6b0`
- Bold: `e702ebb741ef6440def0633480c6d480fd0a56ff171ec8dd3d41697a1b022d90`

## Validation

- `python3 tests/idioma_ar.py`: all keys, formats, whitespace, line breaks, markup, placeholder residue, and coverage of every catalog character in both weights.
- `bash tests/idiomaauto.sh`: language/locale selection and priority.
- `bash tests/uiarabic.sh`: mixed scripts, joining, punctuation, marks, directional controls, symbols, sizes and weights; measures match raster widths. Repeat with `ASAN_OPTIONS=detect_leaks=0 SANITIZE=1`.
- Existing bidi, primary/secondary subtitle and plainass tests remain in the workflow.
- ARM package build verifies static linking, preserved P2P engine, no packaged user state, and staged Inter font coverage.

The custom package is version 2.0.2 and uses the existing app ID. It is a prerelease, not an upstream version. LG C3 UI testing remains necessary: select Arabic, browse Home/Settings/help and long dialogs, change UI scale, reopen the app, switch back to English, and verify that the existing Arabic/English subtitle behavior is unchanged. The new shaping path is enabled only in builds with NV_ASS_LIBASS; this test targets LG webOS, not every platform supported by upstream.
