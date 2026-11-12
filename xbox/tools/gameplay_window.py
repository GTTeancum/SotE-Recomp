"""Validate a sampled post-input interval before reporting gameplay FPS."""
def summarize(samples):
    if len(samples) < 2:
        return {'valid': False, 'reason': 'Fewer than two observation samples'}
    if any(len(s.get('scene', [])) != 6 or len(s.get('player', [])) != 8 or
           len(s.get('guest', [])) != 9 for s in samples):
        return {'valid': False, 'reason': 'Incomplete observation telemetry'}
    first, last = samples[0], samples[-1]
    identity = first['scene'][:2]
    if any(s['scene'][:2] != identity for s in samples):
        return {'valid': False, 'reason': 'Story changed or scene counters restarted'}
    if identity[0] not in (3, 4, 6, 8, 10, 14, 15, 17, 20, 25, 26, 27, 29, 30):
        return {'valid': False, 'reason': 'Not a gameplay story event'}
    if any(s['guest'][0] != 2 or s['guest'][3] or (s['guest'][8] >> 16) for s in samples):
        return {'valid': False, 'reason': 'Loading, pause or communicator observed'}
    if any(s['player'][6] != first['player'][6] for s in samples):
        return {'valid': False, 'reason': 'Life-loss event observed'}
    if any(b['scene'][4] < a['scene'][4] for a, b in zip(samples, samples[1:])):
        return {'valid': False, 'reason': 'Presentation counter decreased'}
    frames = last['scene'][4] - first['scene'][4]
    guest_ms = (last['scene'][2] - first['scene'][2]) & 0xffffffff
    host_seconds = last['host_seconds'] - first['host_seconds']
    if guest_ms < 5000 or host_seconds < 5:
        return {'valid': False, 'reason': 'Observation interval shorter than five seconds'}
    return dict(valid=True, event=identity[0], frames=frames, guest_seconds=guest_ms/1000,
                host_seconds=host_seconds, guest_fps=frames*1000/guest_ms,
                host_fps=frames/host_seconds, samples=len(samples),
                limitation='Sampled states; brief changes between samples may be missed')
