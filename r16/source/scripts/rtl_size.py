"""Real Vivado synthesis estimates for both frozen HLS RTL sets; no bitstream."""
from pathlib import Path
import argparse,datetime,uuid,shutil,json,hashlib,os,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--layer',type=Path,required=True);p.add_argument('--descriptor',type=Path,required=True)
    p.add_argument('--only',choices=('layer4','descriptor'));a=p.parse_args()
    job=Path('C:/h/orb4')/(datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_size_'+uuid.uuid4().hex[:8]);job.mkdir(parents=True)
    manifest=[];source_states={}
    for kernel,source in [('layer4',a.layer),('descriptor',a.descriptor)]:
        if a.only and kernel!=a.only:continue
        state=json.loads((source/'status.json').read_text())
        if state['state']=='RUNNING':
            # Size the completed frozen RTL while its full co-simulation runs.
            # This result cannot accept the IP for an integrated board build.
            log=(source/'hls.log').read_text(errors='replace')
            assert state['mode']=='export' and 'INFO: [COSIM 212-47]' in log and 'ERROR:' not in log
        else:assert state['state']=='SUCCEEDED'
        for item in json.loads((source/'source_manifest.json').read_text()):
            assert hashlib.sha256((source/item['path']).read_bytes()).hexdigest()==item['sha256']
        source_states[kernel]=dict(job=str(source.resolve()),hls_state_at_freeze=state['state'])
        shutil.copytree(source/'project/solution1/syn/verilog',job/kernel)
        manifest.extend(dict(path=str(f.relative_to(job)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in (job/kernel).iterdir() if f.is_file())
    (job/'rtl_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    tcl='''set root [file normalize $::env(ORB4_SIZE_JOB)]
set_param general.maxThreads 4
foreach kernel {layer4 descriptor} {
    cd [file join $root $kernel]
    create_project -in_memory -part xc7z015clg485-2
    read_verilog [glob *.v]
    synth_design -top fastv2_${kernel}_hls -mode out_of_context -part xc7z015clg485-2
    report_utilization -file [file join $root ${kernel}_utilization.rpt]
    report_utilization -hierarchical -file [file join $root ${kernel}_hierarchy.rpt]
    write_checkpoint [file join $root ${kernel}.dcp]
    close_project
}
set f [open [file join $root success.stamp] w];puts $f ORB4_SIZE_SYNTH_PASS;close $f
exit 0
'''
    if a.only:tcl=tcl.replace('foreach kernel {layer4 descriptor}', 'foreach kernel {'+a.only+'}')
    (job/'run.tcl').write_text(tcl)
    wrapper='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd'
    (job/'launch.ps1').write_text("& '"+wrapper+"' vivado -mode batch -source '"+str(job/'run.tcl')+"'\nexit $LASTEXITCODE\n")
    env=os.environ.copy();env['ORB4_SIZE_JOB']=str(job);state=dict(state='RUNNING',job=str(job),tool='Vivado 2018.3',sources=source_states,scope='Individual IP out-of-context synthesis, no timing or integrated fit acceptance')
    def save():
        (job/'status.json').write_text(json.dumps(state,indent=2)+'\n');(ROOT/'evidence/last_size_job.json').write_text(json.dumps(state,indent=2)+'\n')
    save();print('ORB4_SIZE_JOB '+str(job),flush=True);begin=time.monotonic()
    with (job/'build.log').open('w') as f:
        proc=subprocess.Popen(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(job/'launch.ps1')],cwd=job,env=env,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        state['pid']=proc.pid;save()
        try:rc=proc.wait(timeout=5400)
        except subprocess.TimeoutExpired:subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],stdout=f,stderr=f);rc=-1
    state.update(state='SUCCEEDED' if rc==0 and (job/'success.stamp').exists() else 'FAILED',returncode=rc,seconds=round(time.monotonic()-begin,3));save();print(json.dumps(state),flush=True)
    if state['state']!='SUCCEEDED':raise SystemExit(1)
if __name__=='__main__':main()
