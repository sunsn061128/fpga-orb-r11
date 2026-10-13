"""Explicitly authorized volatile board test using an accepted bit/ELF pairing."""
from pathlib import Path
import argparse,json,hashlib,subprocess,os,datetime,struct,time,re
from rtl_gate import verify_layer21
def main():
    p=argparse.ArgumentParser();p.add_argument('--job',type=Path,required=True);p.add_argument('--frame',choices=('normal','noise','zero'),default='normal')
    p.add_argument('--frames',type=int,default=100);p.add_argument('--mode',choices=('Verified','Production'),default='Verified')
    p.add_argument('--full-layer-axi',type=Path,help='Complete independent 21-case layer RTL proof, when vendor co-simulation times out')
    p.add_argument('--rerun',action='store_true',help='Normal only: reuse the immediately preceding accepted hardware session');a=p.parse_args()
    job=a.job.resolve();assert 1<=a.frames<=1000
    state=json.loads((job/'status.json').read_text(encoding='utf-8-sig'))
    pairing=json.loads((job/'pairing.json').read_text(encoding='utf-8-sig'))
    assert state['state']=='SUCCEEDED' and (job/'vivado/accepted.stamp').exists() and pairing['State']=='PASS'
    assert state['frequency_mhz']==pairing['FrequencyMHz']
    metrics=dict(line.split('\t') for line in (job/'vivado/post_route_metrics.tsv').read_text().splitlines())
    assert float(metrics['frequency_mhz'])==state['frequency_mhz'] and float(metrics['wns_ns'])>=0 and float(metrics['whs_ns'])>=0
    for kernel,key,axi in [('layer4','full_layer_cosim','axi_layer'),('descriptor','full_descriptor_cosim','axi_descriptor')]:
        full=Path(state[key]);rpt_path=full/f'project/solution1/sim/report/fastv2_{kernel}_hls_cosim.rpt'
        if kernel=='layer4' and a.full_layer_axi:
            full_layer_proof=verify_layer21(a.full_layer_axi,full,1000/state['frequency_mhz'])
        else:
            assert rpt_path.exists() and re.search(r'\|\s*Verilog\s*\|\s*Pass\s*\|',rpt_path.read_text()), 'Require the complete C/RTL gate before programming'
        for item in json.loads((Path(state[axi])/'rtl_manifest.json').read_text()):
            assert hashlib.sha256((full/'project/solution1/syn/verilog'/item['name']).read_bytes()).hexdigest()==item['sha256']
    for item in pairing['Files']:
        assert hashlib.sha256(Path(item['Path']).read_bytes()).hexdigest().upper()==item['SHA256']
    if a.mode=='Production':
        verified=[json.loads(f.read_text()) for f in job.glob('*_Verified_*.json')]
        assert any(s['state']=='SUCCEEDED' and s['frame']==a.frame and s['frames']>=100 for s in verified)
    name=datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_'+a.frame+'_'+a.mode+'_'+str(a.frames)
    log=job/(name+'.log');status=job/(name+'.json')
    s=dict(state='RUNNING',frame=a.frame,frames=a.frames,mode=a.mode,job=str(job),frequency_mhz=state['frequency_mhz'],volatile_sram_only=True)
    if a.full_layer_axi:s['full_layer_rtl_proof']=full_layer_proof
    previous=None
    if a.rerun:
        assert a.frame=='normal'
        runs=[f for f in job.glob('*.json') if '_Verified_' in f.name or '_Production_' in f.name]
        assert runs, 'No preceding board session'
        previous_file=max(runs,key=lambda f:f.name);previous=json.loads(previous_file.read_text())
        assert previous['state']=='SUCCEEDED' and previous['frame']=='normal' and previous['job']==str(job)
        prior_log=Path(previous['log']).read_text(errors='replace')
        prior_match=re.search(r'ORB60_MAILBOX_WORDS=([^\r\n]+)',prior_log)
        assert prior_match and [int(v,0) for v in prior_match[1].split()]==previous['mailbox']
        prior_words=job/(name+'_previous_mailbox.txt')
        prior_words.write_text(' '.join(str(v) for v in previous['mailbox'])+'\n')
        s.update(reuses_configuration=True,previous_session=str(previous_file))
    status.write_text(json.dumps(s,indent=2)+'\n')
    wrapper='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/xilinx-2018.3.cmd'
    launch=job/(name+'.ps1')
    if a.rerun:
        arguments=[job/'inputs/rerun.tcl',job,a.frames,a.mode,prior_words]
    else:
        arguments=[job/'inputs/run.tcl',job,a.frame,a.frames,a.mode]
    launch.write_text("& '"+wrapper+"' xsct "+' '.join("'"+str(v)+"'" for v in arguments)+"\nexit $LASTEXITCODE\n")
    begin=time.monotonic()
    try:
        system32=Path(os.environ['SystemRoot'])/'System32'
        powershell=system32/'WindowsPowerShell/v1.0/powershell.exe'
        assert powershell.is_file(), 'Windows PowerShell executable unavailable'
        with log.open('w') as f:
            proc=subprocess.Popen([str(powershell),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(launch)],stdout=f,stderr=subprocess.STDOUT,cwd=job,creationflags=subprocess.CREATE_NO_WINDOW)
            try:rc=proc.wait(timeout=180+a.frames*2)
            except subprocess.TimeoutExpired:
                subprocess.run([str(system32/'taskkill.exe'),'/PID',str(proc.pid),'/T','/F'],stdout=f,stderr=f);raise RuntimeError('Owned board process timeout')
        text=log.read_text(errors='replace')
        clock=re.search(r'ORB4_FCLK0_READBACK actual=(0x[0-9A-F]+) mask=(0x[0-9A-F]+) expected=(0x[0-9A-F]+)',text)
        if state['frequency_mhz']==125:
            assert clock, 'Require candidate FCLK0 hardware readback'
        if clock:
            actual,mask,expected=(int(v,16) for v in clock.groups());assert actual&mask==expected&mask
            s['fclk0_readback']=dict(actual=actual,mask=mask,expected=expected)
        match=re.search(r'ORB60_MAILBOX_WORDS=([^\r\n]+)',text);assert match, 'No actual terminal mailbox'
        words=[int(v,0) for v in match[1].split()];assert len(words)==128
        wanted=2 if a.mode=='Verified' else 4
        assert rc==0 and words[2]==0x60c00002 and words[3]==1 and words[4]==wanted and words[5]==0 and words[6]==a.frames
        assert 'ORB60_BOARD_RESULT mode='+a.mode in text and 'FAILED' not in text and 'ERROR:' not in text
        total=struct.unpack_from('<I',(job/a.frame/'golden.bin').read_bytes(),12)[0]
        assert words[16]==total
        s.update(state='SUCCEEDED',result='EXACT_VERIFIED' if a.mode=='Verified' else 'COMPLETE_UNVERIFIED',features=words[16],processing_avg_us=words[10],processing_min_us=words[13],processing_max_us=words[14],wall_us=words[11],verification_us=words[12],fps_milli=words[15],mailbox=words,log=str(log))
        s.update(input_contract='direct_immutable_ddr' if words[78]==0x44445231 else 'per_frame_cpu_copy',
                 whole_loop_fps=a.frames*1e6/words[11] if words[11] else 0)
    except BaseException as error:s.update(state='FAILED',error=str(error));raise
    finally:
        s['seconds']=round(time.monotonic()-begin,3);status.write_text(json.dumps(s,indent=2)+'\n')
        print(json.dumps({k:v for k,v in s.items() if k!='mailbox'}),flush=True)
if __name__=='__main__':main()
