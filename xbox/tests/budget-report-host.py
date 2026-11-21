"""Deterministic coverage/units tests. All telemetry here is synthetic."""
import copy
import json
import sys
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import console_budget_report as b
import console_mode_comparison as pair


def write(p,data):
    p.write_text(json.dumps(data))


def row(j,event,samples,ok=True):
    p=30*1024**2
    return dict(seconds=samples, sote_xbox_status=[0x534f5445,1,5,0,0x3929aae9,0xa2f391ae],
                sote_budget_status=[1,samples,0,64*b.MIB,p,p,64*b.MIB-p,9,196560,2,b.TARGET,0,0,0,0,0,0,0,1],
                sote_budget_heap=[8*b.MIB,8*b.MIB], sote_rom_io_status=[1,2,3,4,5,6,0,7,8],
                sote_perf_scene=[event,0,0,0,5 if ok else 0,0], sote_story_status=[event,0,1],
                sote_scene_fixture_status=[1,j,event,100*j])


with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/"disc").mkdir()
    cfg="budget_probe=1\nscene_jump=100:25\nscene_jump=200:2\n"
    (p/"disc"/"xbox-test.cfg").write_text(cfg)
    a,c=row(1,25,123),row(2,2,456)
    write(p/"telemetry.json",[a,c]); r=b.report(p,partial=True)
    assert r["all_scheduled_jumps_presented"] and b.fixture_failures(r)==["no saved XBE identity"]
    write(p/"identity.json",dict(xbe_sha256="a"*64,config=cfg))
    assert not b.fixture_failures(b.report(p))
    write(p/"telemetry.json",[a,row(2,2,456,ok=False)])
    r=b.report(p);assert r["build_identity_written"] and not r["all_scheduled_jumps_presented"]
    assert r["unseen_scripted_checkpoints"]==[(2,2)] and len(b.fixture_failures(r))==1
    fault=copy.deepcopy(c); fault["sote_budget_status"][2]+=1;fault["sote_rom_io_status"][6]+=1
    write(p/"telemetry.json",[a,fault]);assert len(b.fixture_failures(b.report(p)))==2
    for bad in (row(2,2,100),dict(c,sote_budget_status=[1]),dict(c,sote_xbox_status=[0x534f5445,1,8,0])):
        write(p/"telemetry.json",[a,bad])
        try:b.report(p)
        except ValueError:pass
        else:raise AssertionError("counter reset, bad ABI, or unsupported guest passed")
assert b.surfaces(640,480)==6144000 and b.surfaces(1280,720)==18432000
assert b.scenario(30924800,True,True,int(9.25*b.MIB),0,6144000,b.MIB)["conditional_total"]==65019904
with tempfile.TemporaryDirectory() as d:
    dirs=[]
    for mode in (0,1):
        p=Path(d)/("on" if mode else "off");disc=p/"disc";pack=disc/"hdtextures"
        pack.mkdir(parents=True);dirs.append(p)
        for name in ("index.bin","hash_index.bin","0001.bin"):(pack/name).write_bytes(name.encode())
        (disc/"default.xbe").write_bytes(b"synthetic build");(disc/"xbox-input.txt").write_bytes(b"synthetic input")
        (disc/"xbox-test.cfg").write_text(cfg);write(p/"hdd.backing.json",dict(synthetic=True))
        (p/"xemu.toml").write_text(str(p.resolve())+"/sote.iso")
        if mode:(disc/"hd-textures.on").touch()
        write(p/"identity.json",dict(fixture=True,xbe_sha256=pair.digest(disc/"default.xbe"),
                                     input_sha256=pair.digest(disc/"xbox-input.txt"),config=cfg))
        rows=[row(1,25,10),row(2,2,20)]
        for rr in rows:
            rr["sote_hd_status"]=[0]*16
            if mode:
                rr["sote_hd_status"][0]=rr["sote_hd_status"][5]=1
                rr["sote_hd_status"][7]=8*b.MIB
                for idx,sign in ((4,1),(5,1),(6,-1)):rr["sote_budget_status"][idx]+=sign*8*b.MIB
        write(p/"telemetry.json",rows)
    r=pair.compare(*dirs)
    assert r["highest_observed_delta_bytes"]==8*b.MIB and r["measured_enhancement_residency"]
    (dirs[1]/"disc"/"hdtextures"/"0001.bin").write_bytes(b"same path, different file")
    try:pair.compare(*dirs)
    except ValueError as e:assert "Pack file differs" in str(e)
    else:raise AssertionError("mismatched staged pack passed")
print("Numeric model; identity-vs-presentation, reboot, status ABI, missing frames, I/O/query failures: PASS")
print("Controlled OFF/ON fixture identity, matched pack contents, resident delta and tampered pair rejection: PASS")
