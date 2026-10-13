"""Retry physical implementation of the identical linked netlist in a fresh job."""
from pathlib import Path
import argparse,datetime,uuid,json,hashlib,shutil,os,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
WRAPPER=Path('C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd')

def main():
    p=argparse.ArgumentParser();p.add_argument('--from-job',type=Path,required=True)
    p.add_argument('--remap-ce-max-fanout',type=int,choices=(0,4,8),default=0)
    p.add_argument('--carry-remap',type=int,choices=(0,1,2),default=0)
    p.add_argument('--from-running-implementation',action='store_true',help='Freeze the completed linked synthesis checkpoint while an independent implementation is running')
    p.add_argument('--no-fanout-opt',action='store_true');a=p.parse_args()
    parent=a.from_job.resolve();state=json.loads((parent/'status.json').read_text())
    allowed_states=('RUNNING','FAILED') if a.from_running_implementation else ('FAILED',)
    assert state['state'] in allowed_states and state['part']=='xc7z015clg485-2' and state['frequency_mhz'] in (100,125)
    parent_state_at_freeze=state['state']
    assert (parent/'vivado/p.runs/synth_1/.vivado.end.rst').exists()
    checkpoint=parent/'vivado/p.runs/impl_1/s_wrapper.dcp'
    assert checkpoint.exists(), 'Require the fully linked initial implementation checkpoint'
    assert (parent/'vivado/p.runs/impl_1/.init_design.end.rst').exists(), 'Require completed linked checkpoint write'
    for item in json.loads((parent/'input_manifest.json').read_text()):
        assert hashlib.sha256((parent/item['path']).read_bytes()).hexdigest()==item['sha256']
    job=Path('C:/h/orb4')/(datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_route_'+uuid.uuid4().hex[:8]);job.mkdir()
    shutil.copytree(parent/'inputs',job/'inputs')
    for name in ('retry_route.tcl','routed_checks.tcl','timing_checks.tcl'):
        shutil.copy2(ROOT/'vivado'/name,job/'inputs'/name)
    (job/'vivado').mkdir();shutil.copy2(checkpoint,job/'linked_synth.dcp')
    manifest=[dict(path=str(f.relative_to(job)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in (job/'inputs').rglob('*') if f.is_file()]
    manifest.append(dict(path='linked_synth.dcp',sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest()))
    (job/'input_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    state.update(state='RUNNING',job=str(job),checkpoint_parent=str(parent),checkpoint_sha256=manifest[-1]['sha256'],implementation='opt ExploreWithRemap; control_set_merge; place Default; physopt Explore; route Explore',bit_path=str(job/'vivado/s_wrapper.bit'),hardware_definition='Inherited source PS configuration; no new HDF claimed')
    state['remap_ce_max_fanout']=a.remap_ce_max_fanout
    state['parent_state_at_freeze']=parent_state_at_freeze
    state.update(carry_remap=a.carry_remap,no_fanout_opt=a.no_fanout_opt)
    state.pop('returncode',None);state.pop('seconds',None)
    def save():
        (job/'status.json').write_text(json.dumps(state,indent=2)+'\n');(ROOT/'evidence/last_board_job.json').write_text(json.dumps(state,indent=2)+'\n')
    save();print('ORB4_BOARD_ROUTE '+str(job),flush=True)
    env=os.environ.copy();env.update(ORB4_BOARD_JOB=str(job),ORB4_FREQ_MHZ=str(state['frequency_mhz']),ORB4_REMAP_CE=str(a.remap_ce_max_fanout),ORB4_CARRY_REMAP=str(a.carry_remap),ORB4_NO_FANOUT_OPT=str(int(a.no_fanout_opt)))
    launch=job/'launch.ps1';launch.write_text("& '"+str(WRAPPER)+"' vivado -mode batch -source '"+str(job/'inputs/retry_route.tcl')+"'\nexit $LASTEXITCODE\n",encoding='ascii')
    begin=time.monotonic()
    with (job/'build.log').open('w',encoding='utf-8') as f:
        proc=subprocess.Popen(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(launch)],cwd=job,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        state['pid']=proc.pid;save()
        try:rc=proc.wait(timeout=10800)
        except subprocess.TimeoutExpired:
            subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],stdout=f,stderr=f);rc=-1
    state.update(state='SUCCEEDED' if rc==0 and (job/'vivado/accepted.stamp').exists() else 'FAILED',returncode=rc,seconds=round(time.monotonic()-begin,3));save()
    print(json.dumps(state),flush=True)
    if state['state']!='SUCCEEDED':raise SystemExit(1)
if __name__=='__main__':main()
