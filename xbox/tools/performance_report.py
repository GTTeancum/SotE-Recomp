"""Summarize Xbox guest timings; overlapping categories are not additive."""
import json
import sys
from pathlib import Path
from gameplay_window import summarize

result = json.loads(Path(sys.argv[1]).read_text())
window=summarize(result.get('gameplay_samples',[]))
if window['valid']:
    print(f"Post-input gameplay: {window['frames']} frames / {window['guest_seconds']:.2f} guest seconds = {window['guest_fps']:.2f} FPS")
    print(f"Host wall-time rate: {window['host_fps']:.2f} FPS; {window['samples']} state samples. {window['limitation']}.")
else:
    print('Post-input gameplay FPS withheld: '+window['reason'])
state = result['subsystems']
scene = state['sote_perf_scene']
samples = state['sote_perf_samples']
names = ['texture_wait', 'texture_decode', 'shader_submission', 'display_list_total',
         'task_end_wait', 'presentation_wait', 'command_buffer_wait',
         'vertex_load_transform_lighting', 'triangle_total', 'clipping',
         'gpu_triangle_total', 'texture_lookup_total']
observations=result.get('gameplay_samples',[])
if window['valid'] and all(len(s.get('timings',[]))==36 for s in observations):
    first,last=observations[0]['timings'],observations[-1]['timings']
    print('Post-input timings (overlapping categories, sampled asynchronously):')
    for index,name in enumerate(names):
        total=(last[index*3+1]-first[index*3+1])&0xffffffff
        print(f'  {name}: {total/max(1,window["frames"])/1000:.3f} ms/frame')
elapsed_ms = (scene[2] - scene[1]) & 0xffffffff
frames = scene[4] if len(scene) >= 6 else samples[5 * 3]
detailed = bool(scene[5]) if len(scene) >= 6 else True
print(f'Story {scene[0]}: {frames} presentations over ~{elapsed_ms / 1000:.2f} guest seconds')
if elapsed_ms:
    print(f'Approximate scene presentation rate: {frames * 1000 / elapsed_ms:.2f} FPS')
if not detailed:
    print("Fine-grained timers disabled; only frame/time counters active.")
for index, name in enumerate(names[:len(samples)//3] if detailed else []):
    calls, total, maximum = samples[index * 3:index * 3 + 3]
    print(f'{name}: {calls} calls, {total / 1000:.2f} ms total, '
          f'{total / max(1, frames) / 1000:.3f} ms/presentation, max {maximum / 1000:.3f} ms')
print('Guest clock and whole-scene average; includes transitions and any scripted pause.')
print('Display-list total includes texture work, shader submission and command-buffer waits.')
