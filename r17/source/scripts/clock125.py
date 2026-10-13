"""Drive isolated 8 ns verification/builds; board programming stays explicit."""
import argparse,json,os,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BIN=Path('C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin')
PS='C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def main():
    p=argparse.ArgumentParser();p.add_argument('--layer',type=Path,required=True);p.add_argument('--descriptor',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--axi-layer',type=Path);p.add_argument('--axi-descriptor',type=Path)
    p.add_argument('--full-layer',type=Path);p.add_argument('--full-descriptor',type=Path);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    for job in (a.layer,a.descriptor):assert read(job/'status.json')['period_ns']==8
    record=dict(state='RUNNING',phase='HLS_8NS',layer=str(a.layer),descriptor=str(a.descriptor),started=time.time())
    def save():
        (out/'status.json').write_text(json.dumps(record,indent=2)+'\n')
        print(json.dumps(record),flush=True)
    def launch(name,arguments,profile='fpga-python.cmd'):
        command=[str(BIN/profile)]+([str(BIN/'fpga-python.cmd')] if profile.startswith('modelsim') else [])+[str(x) for x in arguments]
        script=out/(name+'.ps1');script.write_text('& '+ ' '.join("'"+v.replace("'","''")+"'" for v in command)+'\nexit $LASTEXITCODE\n')
        log=(out/(name+'.log')).open('w');proc=subprocess.Popen([PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(script)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        return proc
    save();axi={};procs={};board=None;route=None;runtime=None;full_job=None
    if a.axi_layer or a.axi_descriptor:
        for k,proof in [('layer4',a.axi_layer),('descriptor',a.axi_descriptor)]:
            if not proof:continue
            status=read(proof/'status.json');assert status['state']=='SUCCEEDED' and status['kernel']==k and status['clock_period_ns']==8
            axi[k]=proof.resolve()
        record['axi']={k:str(v) for k,v in axi.items()};save()
    deadline=time.monotonic()+4*3600
    try:
        while time.monotonic()<deadline:
            hls={k:read(j/'status.json') for k,j in [('layer4',a.layer),('descriptor',a.descriptor)]}
            for k,j in [('layer4',a.layer),('descriptor',a.descriptor)]:
                assert hls[k]['state']!='FAILED', f'{k} HLS failed'
                log=(j/'hls.log').read_text(errors='replace')
                if k not in axi and (hls[k]['state']=='SUCCEEDED' or 'INFO: [COSIM 212-47]' in log):
                    axi[k]=out/(out.name+'_axi_'+k+'_8ns')
                    args=[ROOT/'verification/run_datapath_axi.py','--hls-job',j,'--vectors',ROOT/'build'/('protocol_vectors' if k=='layer4' else 'descriptor_vectors'),'--output',axi[k],'--kernel',k,'--cases','7,1,14' if k=='layer4' else '35,0,30,24,25,36,37','--reset-case',14 if k=='layer4' else 7]
                    procs[k]=launch('axi_'+k,args,'modelsim-cocotb-2020.4.cmd')
                    record.update(phase='HLS_COSIM_AND_AXI',axi={x:str(y) for x,y in axi.items()});save()
            for k,proc in procs.items():
                if proc.poll() is not None:assert proc.returncode==0 and read(axi[k]/'status.json')['state']=='SUCCEEDED',f'{k} AXI failed'
            if board is None and all(s['state']=='SUCCEEDED' for s in hls.values()) and len(axi)==2 and all(proc.poll()==0 for proc in procs.values()):
                args=[ROOT/'scripts/board_build.py','--frequency-mhz',125,'--layer',a.layer,'--descriptor',a.descriptor,'--axi-layer',axi['layer4'],'--axi-descriptor',axi['descriptor']]
                if a.full_layer:args+=['--full-layer',a.full_layer]
                if a.full_descriptor:args+=['--full-descriptor',a.full_descriptor]
                board=launch('board',args)
                record.update(phase='INTEGRATED_125MHZ');save()
            if board is not None and full_job is None:
                match=re.search(r'ORB4_BOARD_BUILD ([^\r\n]+)',(out/'board.log').read_text(errors='replace'))
                if match:full_job=Path(match[1]);record.update(full_job=str(full_job));save()
                elif board.poll() is not None:raise RuntimeError('Integrated build did not create a job')
            if full_job:
                if runtime is None and (full_job/'vivado/ps_runtime.hdf').exists():
                    runtime=launch('ps_runtime',[ROOT/'scripts/ps_runtime.py','--job',full_job]);record['runtime_started']=True;save()
                full=read(full_job/'status.json')
                if route is None and full['state']!='SUCCEEDED' and (full_job/'vivado/p.runs/impl_1/.init_design.end.rst').exists():
                    args=[ROOT/'scripts/board_route.py','--from-job',full_job,'--carry-remap',2,'--no-fanout-opt']
                    if full['state']=='RUNNING':args+=['--from-running-implementation']
                    route=launch('route',args);record.update(phase='ROUTE_125MHZ',retry_started=True);save()
                if route is not None and 'route_job' not in record:
                    match=re.search(r'ORB4_BOARD_ROUTE ([^\r\n]+)',(out/'route.log').read_text(errors='replace'))
                    if match:record['route_job']=match[1];save()
                selected=full_job if full['state']=='SUCCEEDED' else None
                if record.get('route_job') and read(Path(record['route_job'])/'status.json')['state']=='SUCCEEDED':selected=Path(record['route_job'])
                if selected:
                    record.update(phase='PAIRED_PS_BUILD',accepted_job=str(selected));save()
                    assert runtime is not None and runtime.wait(timeout=1800)==0,'Candidate SDK runtime failed'
                    ps_log=(out/'ps_build.log').open('w')
                    rc=subprocess.run([PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'ps_board/build.ps1'),'-JobRoot',str(selected)],cwd=ROOT,stdout=ps_log,stderr=subprocess.STDOUT,timeout=600).returncode
                    assert rc==0 and read(selected/'pairing.json')['State']=='PASS','Candidate PS pairing failed'
                    record.update(state='PREPARED_FOR_BOARD',phase='ACCEPTED_125MHZ_BIT_ELF');save();return
                if board.poll() is not None and route is not None and route.poll() is not None:raise RuntimeError('Both integrated physical strategies failed')
            time.sleep(30)
        raise TimeoutError('Clock trial controller timeout')
    except BaseException as error:
        record.update(state='FAILED',error=str(error));save();raise
if __name__=='__main__':main()
