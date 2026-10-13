"""Freeze one HLS 2018.3 job; explicit mode and bounded owned process."""
from pathlib import Path
import argparse, os, json, shutil, subprocess, datetime, hashlib, uuid, time
ROOT=Path(__file__).resolve().parents[1]
WRAPPER=Path('C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd')
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--kernel',choices=['layer4','descriptor'],default='layer4')
    ap.add_argument('--mode',choices=['csim','synth','cosim','export','package'],default='synth')
    ap.add_argument('--timeout-minutes',type=int,default=90)
    ap.add_argument('--from-job',type=Path)
    ap.add_argument('--axi-proof',type=Path)
    args=ap.parse_args()
    host=json.loads((ROOT/'build/host/status.json').read_text())
    if host['state']!='HOST_EXACT_PASS': raise RuntimeError('Require fresh exact host gate')
    for item in host['sources']:
        if item['path'].replace('\\','/').startswith(('scripts/hls','scripts/board','scripts/rtl_size')):continue
        p=ROOT/item['path']
        if hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:
            raise RuntimeError('Host source drift: '+str(p))
    name=datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_'+args.kernel+'_'+uuid.uuid4().hex[:8]
    job=Path('C:/h/orb4')/name
    job.mkdir(parents=True,exist_ok=False)
    if args.from_job:
        parent=args.from_job.resolve();parent_state=json.loads((parent/'status.json').read_text())
        assert parent_state['kernel']==args.kernel
        assert args.mode in ('cosim','export','package')
        if args.mode=='package':
            assert args.axi_proof
            proof=json.loads((args.axi_proof/'status.json').read_text())
            assert proof['state']=='SUCCEEDED' and proof['kernel']==args.kernel and Path(proof['hls_job']).resolve()==parent
            for item in json.loads((args.axi_proof/'rtl_manifest.json').read_text()):
                assert hashlib.sha256((parent/'project/solution1/syn/verilog'/item['name']).read_bytes()).hexdigest()==item['sha256']
            log=(parent/'hls.log').read_text(errors='replace')
            assert 'INFO: [COSIM 212-47]' in log and 'ERROR:' not in log
        else:assert parent_state['state']=='SUCCEEDED'
        for item in json.loads((parent/'source_manifest.json').read_text()):
            assert hashlib.sha256((parent/item['path']).read_bytes()).hexdigest()==item['sha256']
        for item in host['sources']:
            rel=item['path'].replace('\\','/')
            if rel.startswith('upstream/'):
                assert hashlib.sha256((parent/'inputs'/rel[len('upstream/'):]).read_bytes()).hexdigest()==item['sha256']
        for directory in ('inputs','generated','project'):
            shutil.copytree(parent/directory,job/directory,ignore=shutil.ignore_patterns('sim','csim') if args.mode=='package' else None)
    else:
        shutil.copytree(ROOT/'upstream',job/'inputs')
        shutil.copytree(ROOT/'build/host',job/'generated',ignore=shutil.ignore_patterns('*.exe','*.o','*.log'))
    shutil.copy2(ROOT/'scripts/hls2018.tcl',job/'run.tcl')
    if args.kernel=='descriptor':
        vectors=job/'descriptor_vectors';vectors.mkdir()
        for label in ['normal','noise','zero']:
            source=ROOT.parent/'orb60_board/20261012_172441_100mhz'/label
            shutil.copytree(source,vectors/label)
    manifest=[{'path':str(p.relative_to(job)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in (job/'inputs').rglob('*') if p.is_file()]
    (job/'source_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    env=os.environ.copy();env.update(ORB4_JOB=str(job),ORB4_KERNEL=args.kernel,ORB4_MODE=args.mode)
    if args.from_job:env['ORB4_REUSE_SYNTH']='1'
    if args.from_job and args.mode!='package' and parent_state['mode'] in ('cosim','export'):
        report=(parent/f'project/solution1/sim/report/fastv2_{args.kernel}_hls_cosim.rpt').read_text()
        import re
        assert re.search(r'\|\s*Verilog\s*\|\s*Pass\s*\|',report)
        env['ORB4_REUSE_COSIM']='1'
    status={'state':'RUNNING','job':str(job),'kernel':args.kernel,'mode':args.mode,'tool':'Vivado HLS 2018.3','part':'xc7z015clg485-2','period_ns':10}
    if args.from_job:status['synthesis_parent']=str(parent)
    if args.axi_proof:status['axi_proof']=str(args.axi_proof.resolve())
    def save():
        (job/'status.json').write_text(json.dumps(status,indent=2)+'\n')
        (ROOT/'evidence/last_hls_job.json').write_text(json.dumps(status,indent=2)+'\n')
        (ROOT/('evidence/last_hls_'+args.kernel+'.json')).write_text(json.dumps(status,indent=2)+'\n')
    save();print('ORB4_HLS_JOB '+str(job),flush=True)
    launch=job/'launch.ps1'
    launch.write_text("& '"+str(WRAPPER)+"' vivado_hls -f '"+str(job/'run.tcl')+"'\nexit $LASTEXITCODE\n",encoding='ascii')
    begin=time.monotonic()
    with (job/'hls.log').open('w',encoding='utf-8') as log:
        process=subprocess.Popen(['C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(launch)],stdout=log,stderr=subprocess.STDOUT,env=env,cwd=job,creationflags=subprocess.CREATE_NO_WINDOW)
        status['pid']=process.pid;save()
        try: rc=process.wait(timeout=args.timeout_minutes*60)
        except subprocess.TimeoutExpired:
            subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],stdout=log,stderr=log)
            rc=-1;status['reason']='owned-process timeout'
    status.update(state='SUCCEEDED' if rc==0 and (job/'success.stamp').exists() else 'FAILED',returncode=rc,seconds=round(time.monotonic()-begin,3));save()
    print(json.dumps(status),flush=True)
    if status['state']!='SUCCEEDED':raise SystemExit(1)
if __name__=='__main__':main()
