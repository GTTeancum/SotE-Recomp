# PC radio and pilot voices

`src/pc_voices.cpp` supplies Hoth and Skyhook voices through the existing HD-audio setting and Sdata discovery. Missing WAVs leave native audio intact. Leebo's separate 31-entry mapping is covered in `PC_VOICE_AUDIT.md`.

## Current mappings

| Clips | Trigger |
| --- | --- |
| ILU01/04/05/07 | Visible Hoth Stage One/Two/Three/Four instructions |
| ILU22 | Visible Return to Battle warning |
| ILU31 | Visible You lost the tow cable message |
| IR108 | Visible Fire tow cable prompt; fixed pilot bank 1 |
| IR101/201/301 | Visible Don't shoot Rebel forces warning |
| IR103/203/303 | Visible I'm on your side / We're on the same side warnings |
| ILU13 | Visible Empire attack communication at Skyhook |
| ILU16 | Visible instruction to destroy station arm turrets |
| ILU17 | Visible instruction to destroy the power core |
| ILU19 | Visible Let's get out of here communication |
| ILU20 | Visible full Where's Dash communication |
| ILU29 | Native successful Hoth tow-cable attachment |
| ILU32 | Native harpoon attempt without a target, after ammo/cooldown checks |
| ILU33 | Native successful walker Trip followed by cable release |

Visible-text mappings run at `func_80008778` before `0x80008C28`, not at message lookup. Normalization removes formatting, folds whitespace and case, and preserves wording. Hoth is restricted to events 2/3; Skyhook to 28-30. Continuous redraw does not replay speech. More than 60 game updates without that message rearms it. Event changes reset message state.

Friendly-fire warnings rotate anonymous pilot banks 1, 2, 3 independently of text, following the installed PC dispatcher. IR108 does not participate in that rotation. Some pairings match meaning rather than exact wording; see the detailed audit.

The native harpoon hooks are `0x80086FCC` (no target), `0x800870C8` (attached), and `0x800866CC` (successful Trip). They share a 180-update cooldown. Cable loss now follows its displayed message. Generic cable clears, launch validation, death and reset do not independently play loss speech.

## Evidence and remaining work

`pc_voices_harness` verifies mappings, event gates, redraw suppression, rearming, pilot rotation, fixed readiness voice, harpoon target checks and excluded cable clears. It uses synthetic guest memory and an audio stub. Native captures and mixed-PCM checks for both cutscene modes are recorded in `PC_VOICE_AUDIT.md`, together with each fixture's limits.

`COMMUNICATOR_VOICE_INVENTORY.tsv` inventories all 97 installed ILB/ILU/IR clips, their staged hashes, mapping status and evidence. All currently match the PC install. Unmatched alternative recordings and combat chatter are not attached to unrelated messages. Named pilot identity, remaining actor-only responses and natural trigger routes must not be inferred from file prefixes or presentation-only fixtures.
