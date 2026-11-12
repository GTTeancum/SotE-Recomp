"""Boot only this project's XBE; inspect guest telemetry, never send desktop input."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import time
import sys
sys.dont_write_bytecode=True
import xemu_native_screenshot as native_capture
from disk_overlay import ensure_overlay

ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--xemu-root',type=Path,default=Path('C:/Games/Emulators/Xemu'))
    parser.add_argument('--nxdk-root',type=Path,default=Path('C:/nxdk'))
    parser.add_argument('--port',type=int,default=9259)
    parser.add_argument('--duration',type=int,default=180,choices=range(10,181))
    parser.add_argument('--input-script',type=Path)
    parser.add_argument('--unlock-levels',action='store_true',help='Isolated level-picker fixture; never a progression pass')
    parser.add_argument('--profile-renderer',action='store_true',help='Enable fine-grained Xbox renderer timers; off by default')
    parser.add_argument('--final-only',action='store_true',help='Capture only the final image to keep visual evidence bounded')
    parser.add_argument('--no-capture',action='store_true',help='Telemetry-only run; cannot establish visual correctness')
    parser.add_argument('--capture-step',type=int,action='append',default=[],help='With --final-only, also capture at this applied input step (at most two)')
    parser.add_argument('--after-input-seconds',type=int,choices=range(5,61),help='Stop an entry check after all scripted inputs plus this observation period; not a validation pass')
    parser.add_argument('--disable-dl-cull',action='store_true',help='Diagnostic only: retain display lists rejected by bounding-box clipping')
    parser.add_argument('--disable-depth',action='store_true',help='Diagnostic only: disable GPU depth comparison and writes')
    parser.add_argument('--flat-lit',action='store_true',help='Diagnostic only: draw lit models in solid magenta')
    parser.add_argument('--dump-rdram',action='store_true',help='Retain one 8 MB guest-memory snapshot for display-list analysis')
    parser.add_argument('--late-lit',action='store_true',help='Diagnostic only: replay flat lit triangles after scene drawing')
    args=parser.parse_args()
    if args.no_capture:
        if args.capture_step:parser.error('--no-capture cannot request step images')
        args.final_only=True
    if args.capture_step and (not args.final_only or len(args.capture_step)>2 or
            len(set(args.capture_step))!=len(args.capture_step) or min(args.capture_step)<1):
        parser.error('--capture-step requires --final-only and one or two distinct positive step numbers')
    if args.late_lit: args.flat_lit=True
    build=ROOT/'build/xbox'; run=build/'xemu'; run.mkdir(parents=True,exist_ok=True)
    # Never attach to an unrelated emulator already using this monitor port.
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1',args.port))
    (run/'result.json').unlink(missing_ok=True)
    (run/'current-screen.png').unlink(missing_ok=True)
    (run/'first-graphics-rdram.bin').unlink(missing_ok=True)
    env=os.environ.copy()
    for key in ('TEMP','TMP','APPDATA','LOCALAPPDATA','XDG_CACHE_HOME'):
        folder=run/key; folder.mkdir(exist_ok=True); env[key]=str(folder)
    rom=build/'disc/sote.us.v1.2.z64'
    (build/'disc/xbox-test.cfg').write_bytes(b'virtual_controller=1\n'+(b'unlock_levels=1\n' if args.unlock_levels else b'')+(b'disable_dl_cull=1\n' if args.disable_dl_cull else b'')+(b'disable_depth=1\n' if args.disable_depth else b''))
    if args.profile_renderer:
        with (build/'disc/xbox-test.cfg').open('ab') as config:config.write(b'profile_renderer=1\n')
    input_data=b''
    if args.flat_lit:
        with (build/'disc/xbox-test.cfg').open('ab') as config: config.write(b'flat_lit=1\n')
    if args.late_lit:
        with (build/'disc/xbox-test.cfg').open('ab') as config: config.write(b'late_lit=1\n')
    if args.input_script:
        source=args.input_script.resolve()
        if not source.is_relative_to(ROOT):raise ValueError('Input script must stay inside this repository')
        input_data=source.read_bytes()
        if len(input_data)>16384:raise ValueError('Input script is too large')
    (build/'disc/xbox-input.txt').write_bytes(input_data)
    if not rom.exists():os.link(ROOT/'SotE_Recompiled/sote.us.v1.2.z64',rom)
    iso=build/'sote-current.iso'
    with (run/'iso.log').open('w') as log:
        subprocess.run([str(args.nxdk_root/'tools/extract-xiso/build/extract-xiso.exe'),'-c',str(build/'disc'),str(iso)],
            cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
    shutil.copy2(args.xemu_root/'eeprom.bin',run/'eeprom.bin')
    hdd=ensure_overlay(args.xemu_root/'HDD/xbox_hdd.qcow2',run/'persistent-hdd.qcow2')
    if args.unlock_levels:
        hdd=ensure_overlay(hdd,run/'level-fixture.qcow2')
    files=dict(bootrom_path=args.xemu_root/'MCPX/mcpx_1.0.bin',
        flashrom_path=args.xemu_root/'BIOS/xbox-4627_debug.bin',eeprom_path=run/'eeprom.bin',
        hdd_path=hdd,dvd_path=iso)
    assert all(p.is_file() for p in files.values())
    capture_dir=run/'native';capture_dir.mkdir(exist_ok=True)
    native_capture.xemu_find_screenshot_flag_pointer_rva(str(args.xemu_root/'xemu.exe'))
    for i in range(5):
        (run/f'scene-screen-{i}.png').unlink(missing_ok=True)
        (run/f'scene-capture-{i}.json').unlink(missing_ok=True)
    for name in ('startup-screen.png','current-screen.png','startup-capture.json',*(f'startup-screen-{i}.png' for i in range(3)),*(f'startup-capture-{i}.json' for i in range(3))):(run/name).unlink(missing_ok=True)
    config=run/'xemu.toml'
    config.write_text('[general]\nshow_welcome=false\nskip_boot_anim=true\n'+
        'screenshot_dir='+json.dumps(str(capture_dir))+'\n[sys]\nmem_limit="64"\n'+
        '[display.quality]\nsurface_scale=1\n[display.window]\nstartup_size="640x480"\n'+
        '[sys.files]\n'+''.join(k+'='+json.dumps(p.as_posix())+'\n' for k,p in files.items()))
    map_text=(build/'sote.map').read_text()
    match=re.search(r'_sote_xbox_status\s+([0-9a-fA-F]{8,16})\s', map_text)
    assert match, 'Telemetry symbol missing from fresh linker map'
    address=int(match[1],16)
    input_match=re.search(r'_sote_input_status\s+([0-9a-fA-F]{8,16})\s',map_text)
    assert not (args.capture_step or args.after_input_seconds) or input_match, 'Input telemetry missing'
    step_captures={}
    inputs_finished_at=None
    last_input_poll=None
    last_input_progress=time.monotonic()
    last_reported_step=-1
    gameplay_samples=[]
    for step in args.capture_step:
        (run/f'input-step-{step}.png').unlink(missing_ok=True)
        (run/f'input-step-{step}.json').unlink(missing_ok=True)
    si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
    startup_captured=0;startup_done=False;scene_captured=0;status=None;history=[]; sock=None;start=time.monotonic();last_liveness=None;last_alive=start
    identity=json.loads((build/'source-manifest.json').read_text())
    with (run/'stdout.log').open('w') as stdout,(run/'stderr.log').open('w') as stderr:
        process=subprocess.Popen([str(args.xemu_root/'xemu.exe'),'-config_path',str(config),
            '-monitor',f'tcp:127.0.0.1:{args.port},server,nowait'],cwd=run,env=env,
            startupinfo=si,stdout=stdout,stderr=stderr)
        def response():
            data=b''
            while not data.endswith(b'(qemu) '):
                part=sock.recv(65536)
                if not part: raise RuntimeError('XEMU monitor closed')
                data+=part
            return data.decode(errors='replace')
        def read_words(pointer,count):
            sock.sendall(f'x /{count}wx 0x{pointer:x}\n'.encode())
            values=[]
            for line in response().splitlines():
                if re.match(r'^\s*[0-9a-fA-F]+:',line):
                    values += [int(v,16) for v in re.findall(r'0x([0-9a-fA-F]+)',line.split(':',1)[1])]
            return values
        def gameplay_sample():
            record=dict(host_seconds=time.monotonic()-start)
            for field,symbol,count in (('scene','sote_perf_scene',6),('player','sote_player_status',8)):
                found=re.search(r'_'+symbol+r'\s+([0-9a-fA-F]{8,16})\s',map_text)
                record[field]=read_words(int(found[1],16),count) if found else []
            # RDRAM words are host-word ordered. The communicator halfword at
            # DD2D0 occupies the upper half of that word after N64 byte swapping.
            record['guest']=read_words(status[16]+0xDD2B0,9)
            if args.profile_renderer:
                found=re.search(r'_sote_perf_samples\s+([0-9a-fA-F]{8,16})\s',map_text)
                record['timings']=read_words(int(found[1],16),36) if found else []
            return record
        try:
            while time.monotonic()-start<args.duration:
                if process.poll() is not None:raise RuntimeError(f'XEMU exited: {process.returncode}')
                if sock is None:
                    try:
                        sock=socket.create_connection(('127.0.0.1',args.port),timeout=2)
                        sock.settimeout(30);response()
                        sock.sendall(b'help wavcapture\n')
                        (run/'native-audio-capture-help.txt').write_text(response())
                    except OSError:
                        if sock:sock.close()
                        sock=None;time.sleep(.5);continue
                sock.sendall(f'x /22wx 0x{address:x}\n'.encode())
                words=[]
                for line in response().splitlines():
                    if re.match(r'^\s*[0-9a-fA-F]+:',line):
                        words += [int(v,16) for v in re.findall(r'0x([0-9a-fA-F]+)',line.split(':',1)[1])]
                if len(words)==22 and words[0]==0x534F5445:
                    status=words
                    liveness=(words[2],words[11],words[14])
                    if liveness!=last_liveness:last_liveness=liveness;last_alive=time.monotonic()
                    if words[2]>=5 and time.monotonic()-last_alive>15:
                        sock.sendall(b'info registers\n')
                        (run/'stalled-registers.txt').write_text(response())
                        raise RuntimeError(f'Xbox heartbeat and vblank stalled; last game function {words[18]:08X}')
                    if not history or history[-1]['stage']!=words[2]:
                        history.append(dict(seconds=round(time.monotonic()-start,2),stage=words[2],error=words[3]))
                        print(json.dumps(history[-1]),flush=True)
                    if words[2]>=5 and (len(step_captures)<len(args.capture_step) or args.after_input_seconds):
                        sock.sendall(f'x /8wx 0x{int(input_match[1],16):x}\n'.encode())
                        input_state=[]
                        for line in response().splitlines():
                            if re.match(r'^\s*[0-9a-fA-F]+:',line):
                                input_state += [int(v,16) for v in re.findall(r'0x([0-9a-fA-F]+)',line.split(':',1)[1])]
                        if len(input_state)==8:
                            if input_state[0]!=last_input_poll:
                                last_input_poll=input_state[0];last_input_progress=time.monotonic()
                            elif time.monotonic()-last_input_progress>30:
                                sock.sendall(b'info registers\n')
                                (run/'stalled-registers.txt').write_text(response())
                                raise RuntimeError('Game input polling stalled for30 seconds while emulator remained live')
                            if input_state[5]!=last_reported_step and (input_state[5]<10 or input_state[5]>=last_reported_step+10 or input_state[5]==input_state[4]):
                                last_reported_step=input_state[5]
                                print(json.dumps(dict(seconds=round(time.monotonic()-start,2),input_step=input_state[5],total_steps=input_state[4])),flush=True)
                            if args.after_input_seconds and input_state[4]>0 and input_state[5]==input_state[4]:
                                if inputs_finished_at is None:inputs_finished_at=time.monotonic()
                                gameplay_samples.append(gameplay_sample())
                                if time.monotonic()-inputs_finished_at>=args.after_input_seconds:
                                    raise TimeoutError('Input sequence and observation period finished; visual/behavior validation still required')
                            for step in args.capture_step:
                                if step in step_captures or input_state[5]<step:continue
                                record=dict(input_state=input_state,seconds=time.monotonic()-start,
                                            captured=False,reason='Requested input step was passed before observation')
                                if input_state[5]==step:
                                    ok,detail,path=native_capture.trigger_native_screenshot(process.pid,str(args.xemu_root/'xemu.exe'),str(capture_dir),8)
                                    record.update(captured=ok,reason=detail)
                                    if ok:shutil.move(path,run/f'input-step-{step}.png')
                                step_captures[step]=record
                                (run/f'input-step-{step}.json').write_text(json.dumps(record,indent=2))
                    if not args.final_only and words[2]==5 and startup_captured<3 and not startup_done:
                        symbols={}
                        for name,count in (('sote_video_status',10),('sote_video_capture',4),('sote_boot_pixels',256),('sote_boot_ownership',6),('sote_scanout_waits',1),('sote_present_sync',4),('sote_video_trace',512)):
                            match=re.search(r'_'+name+r'\s+([0-9a-fA-F]{8,16})\s',map_text)
                            if match:
                                sock.sendall(f'x /{count}wx 0x{int(match[1],16):x}\n'.encode())
                                values=[]
                                for line in response().splitlines():
                                    if re.match(r'^\s*[0-9a-fA-F]+:',line):values += [int(v,16) for v in re.findall(r'0x([0-9a-fA-F]+)',line.split(':',1)[1])]
                                symbols[name]=values
                        video=symbols.get('sote_video_status',[]);frame=symbols.get('sote_video_capture',[])
                        if len(video)==10 and video[0]>=124:startup_done=True
                        if len(video)==10 and (20,60,100)[startup_captured]<=video[0]<124 and len(frame)==4 and frame[0]:
                            sock.sendall(b'stop\n');response()
                            ok,detail,path=native_capture.trigger_native_screenshot(process.pid,str(args.xemu_root/'xemu.exe'),str(capture_dir),8)
                            if ok:
                                shutil.move(path,run/f'startup-screen-{startup_captured}.png')
                                (run/f'startup-capture-{startup_captured}.json').write_text(json.dumps(symbols,indent=2))
                                startup_captured+=1
                            else:raise RuntimeError('Native startup capture: '+detail)
                            sock.sendall(b'cont\n');response()
                    if not args.final_only and words[2]==5 and scene_captured<5 and time.monotonic()-start>=(40,60,90,120,150)[scene_captured]:
                        ok,detail,path=native_capture.trigger_native_screenshot(process.pid,str(args.xemu_root/'xemu.exe'),str(capture_dir),8)
                        if not ok:raise RuntimeError('Native scene capture: '+detail)
                        shutil.move(path,run/f'scene-screen-{scene_captured}.png')
                        (run/f'scene-capture-{scene_captured}.json').write_text(json.dumps(dict(seconds=time.monotonic()-start,status=words),indent=2))
                        scene_captured+=1
                    if words[3]:raise RuntimeError(f'Guest bootstrap error {words[3]}: {[hex(w) for w in words]}')
                    if words[2]==8:
                        imports=json.loads((build/'unsupported.json').read_text())
                        named_codes={0xE4000010:'NV2A graphics-task renderer',0xE4000003:'RSP audio microcode exit'}
                        fallback=(f'graphics opcode {words[13]&255:02X}' if words[13]&0xFFFF0000==0xE5010000 else f'entrypoint {words[13]:08X}')
                        name=imports.get(str(words[13]),named_codes.get(words[13],fallback))
                        raise RuntimeError(f'Unimplemented Xbox service: {name}')
                time.sleep(.5)
            raise TimeoutError(f'{args.duration}-second Xbox run watchdog expired; no completed visual/behavior validation checkpoint')
        except Exception as error:
            subsystems={}
            if sock:
                if args.dump_rdram:
                    sock.sendall(b'stop\n');response()
                for symbol,count in (('sote_save_status',7),('sote_audio_status',6),('sote_dma_status',8),('sote_input_status',8),('sote_sp_status',4),('sote_sp_task',16),('sote_video_status',10),('sote_video_capture',4),('sote_boot_pixels',256),('sote_boot_ownership',6),('sote_scanout_waits',1),('sote_present_sync',4),('sote_graphics_tasks',12),('sote_texture_checks',1),('sote_texture_key_checks',1),('sote_texture_status',5),('xb_tiles',120),('sote_video_trace',512),('sote_gpu_status',2),('sote_gpu_flushes',1),('sote_gpu_batches',1),('sote_perf_samples',36),('sote_perf_scene',6),('sote_texture_cache_status',4),('sote_vertex_status',4),('sote_vertex_checks',1),('sote_clip_checks',1),('sote_dl_cull_status',3),('sote_move_mem_counts',256),('sote_move_word_counts',256),('sote_lit_probe',12),('sote_material_census',352),('sote_material_status',8),('sote_overlay_status',16),('sote_triangle_status',3),('sote_menu_status',6),('sote_menu_trace',384),('sote_profile_trace',512),('sote_pause_trace',512),('sote_level_fixture_status',2),('sote_controls_status',6),('sote_controls_masks',48),('sote_vehicle_status',10),('sote_vehicle_trace',1024),('sote_outrider_status',3),('sote_hoth_status',4),('sote_player_status',8),('sote_player_checks',1),('sote_bike_status',8),('sote_bike_checks',1),('sote_audio_exponent_status',2),('sote_motion_guard_status',5),('sote_safeguard_checks',1),('sote_diagnostic_status',8),('sote_camera_status',12),('sote_player_trace',640),('sote_communicator_trace',640),('sote_story_status',3),('sote_text_status',5)):
                    symbol_match=re.search(r'_'+symbol+r'\s+([0-9a-fA-F]{8,16})\s',map_text)
                    if not symbol_match:continue
                    try:
                        sock.sendall(f'x /{count}wx 0x{int(symbol_match[1],16):x}\n'.encode())
                        values=[]
                        for line in response().splitlines():
                            if re.match(r'^\s*[0-9a-fA-F]+:',line):
                                values += [int(v,16) for v in re.findall(r'0x([0-9a-fA-F]+)',line.split(':',1)[1])]
                        subsystems[symbol]=values
                    except OSError:break
                if status and (status[2]==8 or args.dump_rdram):
                    capture=(run/'first-graphics-rdram.bin').as_posix()
                    sock.sendall(f'memsave 0x{status[16]:x} 0x800000 {json.dumps(capture)}\n'.encode())
                    capture_reply=response()
                    subsystems['rdram_capture_bytes']=Path(capture).stat().st_size if Path(capture).exists() else 0
            if not args.no_capture and sock and status and status[2]>=5:
                ok,detail,path=native_capture.trigger_native_screenshot(process.pid,str(args.xemu_root/'xemu.exe'),str(capture_dir),8)
                subsystems['capture_detail']=detail
                if ok:
                    shutil.move(path,run/'current-screen.png')
                    subsystems['native_capture']=str(run/'current-screen.png')
            (run/'result.json').write_text(json.dumps(dict(passed=False,error=str(error),status=status,history=history,
                subsystems=subsystems,
                xbe_sha256=hashlib.sha256((build/'disc/default.xbe').read_bytes()).hexdigest(),
                virtual_controller=True, profile_renderer=args.profile_renderer, disable_dl_cull=args.disable_dl_cull, disable_depth=args.disable_depth,
                requested_capture_steps=args.capture_step, step_captures=step_captures,
                capture_disabled=args.no_capture,
                after_input_seconds=args.after_input_seconds,
                gameplay_samples=gameplay_samples,
                flat_lit=args.flat_lit, late_lit=args.late_lit,
                level_unlock_fixture=args.unlock_levels,
                input_script_sha256=hashlib.sha256(input_data).hexdigest(),
                foundation_passed=bool(status and len(status)==22 and status[10]==128 and status[12]==35 and status[21]==48
                    and status[17]==12 and status[20]==11 and status[4]==int(identity['rom_crc'],16)
                    and status[5]==int(identity['image_crc'],16)
                    and subsystems.get('sote_texture_checks')==[8197]
                    and subsystems.get('sote_texture_key_checks')==[4097]
                    and subsystems.get('sote_vertex_checks')==[44]
                    and subsystems.get('sote_clip_checks')==[560]
                    and subsystems.get('sote_player_checks')==[12]
                    and subsystems.get('sote_bike_checks')==[9]
                    and subsystems.get('sote_safeguard_checks')==[8]
                    and subsystems.get('sote_save_status',[0]*7)[6]==527)),indent=2))
            raise
        finally:
            if sock:
                try:sock.sendall(b'quit\n')
                except OSError:pass
                sock.close()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
            if args.unlock_levels:
                hdd.unlink(missing_ok=True)
                hdd.with_suffix('.backing.json').unlink(missing_ok=True)
            # XEMU leaves small snapshot overlay files behind even after quit.
            # Remove only these disposable files in our own temporary folder.
            for temporary in (run/'TEMP').glob('vl.*'):
                if temporary.is_file():temporary.unlink()
if __name__=='__main__':main()
