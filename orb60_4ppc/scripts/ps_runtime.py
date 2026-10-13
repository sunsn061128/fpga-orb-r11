"""Generate a fresh 2018.3 standalone runtime from candidate PS hardware."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--job',type=Path,required=True);a=p.parse_args()
    job=a.job.resolve();state=json.loads((job/'status.json').read_text())
    assert state['part']=='xc7z015clg485-2' and state['frequency_mhz']==125
    assert not state.get('checkpoint_parent'), 'Generate runtime from the full candidate project'
    for item in json.loads((job/'input_manifest.json').read_text()):
        assert hashlib.sha256((job/item['path']).read_bytes()).hexdigest()==item['sha256']
    out=job/'ps_runtime';out.mkdir(exist_ok=False);(out/'inputs').mkdir()
    shutil.copy2(job/'vivado/ps_runtime.hdf',out/'inputs/ps_runtime.hdf')
    shutil.copy2(ROOT/'ps_board/runtime.tcl',out/'inputs/runtime.tcl')
    shutil.copy2(job/'vivado/p.srcs/sources_1/bd/s/ip/s_ps7_0/ps7_init.tcl',out/'inputs/generated_ps7_init.tcl')
    frozen=[dict(path=str(f.relative_to(out)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in (out/'inputs').iterdir()]
    record=dict(state='RUNNING',job=str(job),frequency_mhz=125,inputs=frozen,started=datetime.datetime.now().isoformat())
    def save():(out/'status.json').write_text(json.dumps(record,indent=2)+'\n')
    save();env=os.environ.copy();env['ORB4_PS_RUNTIME']=str(out)
    wrapper='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd'
    launch=out/'launch.ps1';launch.write_text("& '"+wrapper+"' xsct '"+str(out/'inputs/runtime.tcl')+"'\nexit $LASTEXITCODE\n")
    begin=time.monotonic()
    with (out/'build.log').open('w') as log:
        proc=subprocess.Popen(['C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(launch)],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        record['pid']=proc.pid;save()
        try:rc=proc.wait(timeout=1800)
        except subprocess.TimeoutExpired:
            subprocess.run(['C:/Windows/System32/taskkill.exe','/PID',str(proc.pid),'/T','/F'],stdout=log,stderr=log);rc=-1
    record.update(state='SUCCEEDED' if rc==0 and (out/'accepted.stamp').exists() else 'FAILED',returncode=rc,seconds=round(time.monotonic()-begin,3))
    if record['state']=='SUCCEEDED':
        try:
            assert (out/'sdk/next_hw/ps7_init.tcl').read_bytes()==(out/'inputs/generated_ps7_init.tcl').read_bytes()
            record['files']=[dict(path=str(f.relative_to(out)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in (out/'sdk').rglob('*') if f.is_file() and (f.suffix in ('.h','.a','.tcl','.ld','.spec') or f.name=='Xilinx.spec')]
        except BaseException as error:
            record.update(state='FAILED',error=str(error));save();raise
    save();print(json.dumps(record if record['state']!='SUCCEEDED' else {k:v for k,v in record.items() if k not in ('files','inputs')}),flush=True)
    if record['state']!='SUCCEEDED':raise SystemExit(1)
if __name__=='__main__':main()
