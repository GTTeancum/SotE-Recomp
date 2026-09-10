# Modern blaster aiming

Modern on-foot blaster shots converge from the native muzzle toward the first
collision on the camera-center ray. Empty space uses a finite distant point.
This adjusts launch velocity, preserving its speed and the original muzzle
position. Native projectile collision still blocks shots on nearby cover.
A surface behind the muzzle does not turn a shot backwards.

The camera, body pitch, enemy fire, Classic controls, seeker missiles and thermal
detonators are unchanged by this hook. This initial implementation targets the
standard blaster laser. It does not add target
leading, ballistic-drop compensation or homing after launch.

## Hot-reloaded CONTROLS_MODERN.INI

| Key | Default | Meaning |
| --- | --- | --- |
| `on_foot_convergence` | `1` | Enable camera-center convergence; 0 also disables magnetism. |
| `on_foot_convergence_range` | `1500` | Empty-space aim distance, 50-5000 game units. |
| `on_foot_magnetism_strength` | `0` | 0=off, 1=full correction toward an eligible visible target; intermediate values blend the directions. |
| `on_foot_magnetism_cone_degrees` | `2` | Target-search half-angle, 0.1-8 degrees. |
| `on_foot_magnetism_range` | `150` | Maximum target distance from camera, 1-500 game units. |

For an initial magnetism experiment, try strength `0.2` and keep the default
cone/range. These are starting values for playtesting, not validated feel presets.
Magnetism uses the native targetable/alive-object pool and requires visibility
from both camera and muzzle. It never moves the camera or changes stick input.
Native exclusions for crates and the special boss object class are retained.

## Integration and checks

The hook runs in the dedicated laser pool allocator `80031FE0` at `800321FC`,
after muzzle placement and before velocity scaling, only for the active Modern
player. The active camera at `800D08A4`
provides its world position and forward basis. Collision uses native `800654A0`
with copied registers and separate guest stack scratch; the player's collision
object is excluded as in the original firing code.

`modern_aim_harness` exercises the real hook with a native-trace test double:
near/far convergence, empty space, preserved speed/origin/registers, backwards
shot prevention, partial/full magnetism, occlusion and cone rejection, INI off,
other owner exclusions, disconnect and Classic isolation.

Set `SOTE_TRACE_MODERN_AIM=1` for spawn/aim telemetry. The centered marker and
hands-on close/far validation remain in the to-do list; this work establishes
the launch-direction behavior that the marker will represent.

Validation recorded September 9, 2026: all six CTest harnesses pass. The focused
Echo Base run completed at VI 3600 and recorded 14 corrected laser launches
while turning, at camera-ray hit distances 9.547-94.910 game units. Launch vectors
matched muzzle-to-target directions within the rounded trace precision (maximum
vector difference 0.000261). Magnetism remained off. Evidence and native captures
are in `build/diagnostics/convergence/final/`; `shot_audit.json` records each shot.
Magnetism selection/occlusion is validated in the harness, with hands-on tuning
still pending. The playable executable matches the verified Release build.
