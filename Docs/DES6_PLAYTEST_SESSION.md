# DES-6 Playtest Session — "Did They Laugh? Did They Talk?"

**Purpose:** The Phase 7.6 design gate. A 2-player session must demonstrate triage tension
competing holdover clocks, attributed mistakes, dispatcher reactions — and the two hard gates:
did anyone laugh unprompted; did players talk to coordinate.

**Status:** Ready to run. Do not begin the Phase 8 art pass until this gate is met.

---

## 1. Session setup

### Build requirements
- [ ] `HoldoverEditor` Win64 Development build compiles clean
- [ ] `L_RampGreybox` is the startup map
- [ ] 2 aircraft placed with different `ArrivalDelay` values (e.g. 0s and 60s)
- [ ] 2 refill stations placed (one `StationFluidType=TypeI`, one `StationFluidType=TypeIV`)
- [ ] At least 1 ice volume and 1 jet blast volume on the ramp
- [ ] Shift profile: Night 1 (default) — start easy, escalate if the session goes well

### Launch
1. Open `Holdover.uproject` in UE 5.8.
2. Press **Play** (Alt+P). Two game windows open (listen-server host + client).
3. **Players:** one person per window. Use voice chat (Discord/Steam) — the game has no
   built-in voice. The facilitator observes and takes notes.

### Roles
| Role | Who | Does |
|------|-----|------|
| Player 1 (Host) | Human | Plays in window 1 |
| Player 2 (Client) | Human | Plays in window 2 |
| Facilitator | Observer | Watches both screens, takes notes, runs the interview |

**If only one tester is available:** run single-player and note that the coordination gate
cannot be assessed. Record it as "not tested — needs 2 players."

---

## 2. Pre-session brief (2 minutes)

Read this to the players before they start:

> "You're ground crew at a freezing airport. Your job is to deice aircraft and get them
> in the air before their holdover timers expire. Use Type I to clean surfaces, Type IV
> to protect them. You each carry one tank — you'll need to work together. Spray with
> left mouse, interact with E, refill at the stations. There are two planes and a shift
> timer. Questions?"

**Do not explain:**
- The one-tank relay (let them discover it)
- The dispatcher line bank (let them react to it)
- Weather squalls (let them feel it)
- Per-aircraft deadlines (let them discover lateness)

---

## 3. Session script — what to observe

Play through **2–3 full shifts**. The facilitator watches for the moments below and
marks each with a timestamp.

### Shift 1 — Discovery (Night 1, gentle weather)

| # | Watch for | Timestamp | Notes |
|---|-----------|-----------|-------|
| 1 | Players split up without prompting (one takes Type I, one takes Type IV?) | | |
| 2 | Players communicate about which plane to treat first | | |
| 3 | First holdover starts — do players understand the clock? | | |
| 4 | Second aircraft arrives — do they react? ("The other plane!") | | |
| 5 | Dispatcher line draws a reaction (smile, comment, laughter) | | |
| 6 | Ice stumble surprises someone | | |
| 7 | Jet blast penalty surprises someone | | |
| 8 | Refill station confusion (how does this work?) | | |
| 9 | Shift end — do they look at the results screen? | | |
| 10 | Do they want to play another shift? | | |

### Shift 2 — Coordination (Night 2 or 3)

| # | Watch for | Timestamp | Notes |
|---|-----------|-----------|-------|
| 11 | Players negotiate roles ("I'll clean, you protect") | | |
| 12 | Race between competing holdover clocks — who do they save? | | |
| 13 | A plane expires — how do they react? (frustration? laughter?) | | |
| 14 | Dispatcher blames a player — do they laugh? | | |
| 15 | Net pay matters — do they notice the ledger? | | |
| 16 | Late departure fires per-aircraft — surprise? | | |
| 17 | Weather squall forces a panic moment | | |
| 18 | Results screen attribution ("Dan +$100 -$25") | | |

### Shift 3 (optional) — Triage (Night 3+)

| # | Watch for | Timestamp | Notes |
|---|-----------|-----------|-------|
| 19 | 3+ competing clocks — visible panic/communication | | |
| 20 | Players assign sub-roles ("you do the wings, I'll do the tail") | | |
| 21 | Someone laughs at a dispatcher line or a failure | | |
| 22 | Post-shift: "again!" or strategic discussion | | |

---

## 4. DES-6 gate questions

**These two questions decide whether Phase 7.6 passes.**

### Gate A: Did anyone laugh unprompted?

"Unprompted" means a genuine reaction to the game — not the facilitator making a joke.

| Response | Pass? |
|----------|-------|
| Yes — spontaneous laughter at a dispatcher line, a stumble, or a failure | **PASS** |
| Yes — but only after the facilitator pointed it out | PARTIAL |
| No — players were focused/frowning the whole time | **FAIL** |

Record: *What triggered the laugh? (dispatcher line / ice stumble / jet blast / failure / results screen)*

### Gate B: Did players talk to coordinate?

Look for unprompted communication about strategy, timing, or division of labor.

| Response | Pass? |
|----------|-------|
| Yes — players negotiated roles, called out timing, or coordinated saves | **PASS** |
| Partially — some communication but mostly independent play | PARTIAL |
| No — both players played in silence | **FAIL** |

Record: *What did they say? (direct quotes are gold)*

### Overall triage tension

| Question | Yes / No | Evidence |
|----------|----------|----------|
| Did players make visible triage decisions between competing clocks? | | |
| Were mistakes attributed to specific players on the results screen? | | |
| Did dispatcher lines feel reactive (not static)? | | |
| Did the shift profile control difficulty (not code edits)? | | |
| Did players want to play another shift? | | |

---

## 5. Post-session interview (5 minutes)

Ask each player separately:

1. **"What was the most fun moment?"** (Listen for: triage tension, comedy, coordination.)
2. **"What was the most frustrating moment?"** (Listen for: unclear rules, unfair deaths, UI confusion.)
3. **"Were you aware of the other player? Did you need them?"** (Tests the relay mechanic.)
4. **"What did the radio voice sound like to you? Funny? Annoying? Background noise?"** (Tests the dispatcher bank.)
5. **"If you played again right now, what would you do differently?"** (Tests strategic depth.)
6. **"One word to describe the game."** (Record verbatim.)

---

## 6. Results template

Copy this after the session:

```
## DES-6 Playtest Results — [DATE]

**Players:** [names/handles]
**Shifts played:** [N]
**Shift profiles used:** [Night 1 / 2 / 3...]

### Gate A — Laughter
- [ ] PASS / [ ] PARTIAL / [ ] FAIL
- Trigger: [what caused the laugh]
- Timestamp(s): [when]

### Gate B — Coordination
- [ ] PASS / [ ] PARTIAL / [ ] FAIL
- Key quotes:
  - Player 1: "[quote]"
  - Player 2: "[quote]"

### Triage tension
- Visible triage decisions: [Yes/No — describe]
- Attributed mistakes: [Yes/No]
- Reactive dispatcher: [Yes/No]
- Profile-controlled difficulty: [Yes/No]
- Wanted another shift: [Yes/No]

### Fun moment
[Description]

### Frustration moment
[Description]

### Top 3 fixes
1. [ ] [fix]
2. [ ] [fix]
3. [ ] [fix]

### Verdict
- [ ] **PHASE 7.6 GATE PASSED** — proceed to Phase 8 art pass
- [ ] **ITERATE** — run another playtest after fixes
```

---

## 7. Evidence capture

Save to `Docs/evidence/des6_playtest/`:

| Artifact | When |
|----------|------|
| `session_<date>_notes.md` — filled results template | After session |
| `dispatcher_funny_moment.txt` — the exact line that got a laugh | During session |
| `coordination_quote.txt` — best coordination quote | During session |
| `screenshot_triage.png` — HUD showing 2 competing holdover clocks | During shift 2+ |
| `screenshot_results.png` — results screen with per-player attribution | At shift end |
