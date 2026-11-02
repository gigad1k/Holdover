# READY FOR ALI

1. All AI-implementable work is DONE: Phase 9 (MP audit, world-based MP tests, hardening G-1..G-5) and Phase 10 (packaged Win64 build, demo checklist, field manual D1-D3, config hygiene).
2. Suite 26/26 green (`Tools/run_tests.ps1`), including MP, field-manual and legal-DRAFT tests.
3. Packaged build PASS: `Builds\Windows\Holdover.exe` (Win64 Development) smoke-tested — loads `L_RampGreybox`, 0 fatals, audio cooked, field manual built.
4. Field manual (F1): 8 first-shift tips + credits + legal + feedback. Legal is labelled "DRAFT - not legal advice, needs Ali/lawyer review" and must not ship as final until reviewed.
5. Six field-manual values (studio/credits/copyright/EULA/feedback route) are `[[TODO:ALI]]` placeholders — no names, emails or URLs were invented.
6. `Docs/AI/NEEDS_ALI.md` is now ONE prioritised list: 9 items with exact steps, minutes and follow-ups (~5 h total; items 1-5 are the demo-critical path).
7. Start there with item 1: play one full shift in `Builds\Windows\Holdover.exe` (~40 min) and report bugs + whether F1/auto-open work.
8. Then item 2 (DES-6 2-player gate, ~90 min) and item 3 (3-4 player MP, ~45 min) — these unblock the art pass and Phase 9 sign-off.
9. Still unapproved/blocked: atmosphere look, art assets, snow (`NS_SnowFlurry` needs authoring), Steamworks — all on your side per NEEDS_ALI 6-9.
10. Branch `gigad1k/holdover`, tree clean, every item committed with CHANGELOG ticks. Note: HANDOFF records a "push as you go" policy, but this session's orders said no push — nothing was pushed; confirm which policy stands.
