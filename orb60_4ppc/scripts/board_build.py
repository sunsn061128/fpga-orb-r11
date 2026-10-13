"""Pair accepted HLS exports, then synthesize/route under a fresh short job."""
from pathlib import Path
import argparse,datetime,uuid,json,hashlib,shutil,os,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
WRAPPER=Path('C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd')
def main():
    p=argparse.ArgumentParser();p.add_argument('--layer',type=Path,required=True);p.add_argument('--descriptor',type=Path,required=True)
    p.add_argument('--axi-layer',type=Path,required=True);p.add_argument('--axi-descriptor',type=Path,required=True)
    p.add_argument('--full-layer',type=Path);p.add_argument('--full-descriptor',type=Path)
    p.add_argument('--frequency-mhz',type=int,choices=(100,125),default=100)
    a=p.parse_args()
    for kernel,source,gate in [('layer4',a.layer,a.axi_layer),('descriptor',a.descriptor,a.axi_descriptor)]:
        s=json.loads((source/'status.json').read_text());g=json.loads((gate/'status.json').read_text())
        assert s['state']=='SUCCEEDED' and s['mode'] in ('export','package') and s['kernel']==kernel
        assert g['state']=='SUCCEEDED' and g['kernel']==kernel
        assert s['period_ns']==1000/a.frequency_mhz
        assert g.get('clock_period_ns',10)==s['period_ns']
        rtl=source/'project/solution1/syn/verilog'
        for item in json.loads((gate/'rtl_manifest.json').read_text()):
            assert hashlib.sha256((rtl/item['name']).read_bytes()).hexdigest()==item['sha256']
        for item in json.loads((source/'source_manifest.json').read_text()):
            assert hashlib.sha256((source/item['path']).read_bytes()).hexdigest()==item['sha256']
    job=Path('C:/h/orb4')/(datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_board_'+uuid.uuid4().hex[:8])
    job.mkdir(parents=True,exist_ok=False)
    shutil.copytree(ROOT/'vivado',job/'inputs')
    for label,source in [('layer',a.layer),('desc',a.descriptor)]:
        shutil.copytree(source/'project/solution1/impl/ip',job/'ip'/label)
    manifest=[dict(path=str(f.relative_to(job)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for d in ('inputs','ip') for f in (job/d).rglob('*') if f.is_file()]
    (job/'input_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    env=os.environ.copy();env.update(ORB4_BOARD_JOB=str(job),ORB4_BOARD_JOBS='4',ORB4_FREQ_MHZ=str(a.frequency_mhz))
    state=dict(state='RUNNING',part='xc7z015clg485-2',top='s_wrapper',frequency_mhz=a.frequency_mhz,tool='Vivado 2018.3',layer_hls=str(a.layer.resolve()),descriptor_hls=str(a.descriptor.resolve()),axi_layer=str(a.axi_layer.resolve()),axi_descriptor=str(a.axi_descriptor.resolve()),job=str(job))
    state.update(full_layer_cosim=str((a.full_layer or a.layer).resolve()),full_descriptor_cosim=str((a.full_descriptor or a.descriptor).resolve()))
    def save():
        (job/'status.json').write_text(json.dumps(state,indent=2)+'\n')
        (ROOT/'evidence/last_board_job.json').write_text(json.dumps(state,indent=2)+'\n')
    save();print('ORB4_BOARD_BUILD '+str(job),flush=True)
    launch=job/'launch.ps1';launch.write_text("& '"+str(WRAPPER)+"' vivado -mode batch -source '"+str(job/'inputs/build_system.tcl')+"'\nexit $LASTEXITCODE\n",encoding='ascii')
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
