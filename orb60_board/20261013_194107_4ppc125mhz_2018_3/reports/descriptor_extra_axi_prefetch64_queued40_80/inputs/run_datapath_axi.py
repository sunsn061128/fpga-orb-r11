"""Randomized AXI unit gate for frozen layer2/descriptor RTL, shared DDR space."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

from cocotb_tools.runner import get_results,get_runner
from gpi_runtime import configure as configure_runtime
import re
def top_name(job,kernel):
    return 'fastv2_layer4_hls' if kernel=='layer4' else 'fastv2_descriptor_hls'

def control_offsets(job,kernel,top):
    rtl=(job/f'project/solution1/syn/verilog/{top}_control_s_axi.v').read_text()
    offsets={name.removesuffix('_DATA_0'):int(value,16) for name,value in re.findall(r'ADDR_(\w+)\s*=\s*\d+\'h([0-9a-fA-F]+)',rtl) if name.endswith('_DATA_0')}
    offsets['AP_RETURN']=int(re.search(r'ADDR_AP_RETURN_0\s*=\s*\d+\'h([0-9a-fA-F]+)',rtl)[1],16)
    return offsets


def run(args):
    assert 0<=args.first_read_min<=args.first_read_max
    job=args.hls_job.resolve();vectors=args.vectors.resolve();output=args.output.resolve()
    state=json.loads((job/'status.json').read_text(encoding='utf-8-sig'))
    assert state['kernel']==args.kernel
    period=state['period_ns'];assert period in (8,10)
    if state['state']=='RUNNING':
        # The independent simulator can read the completed, immutable syn RTL
        # while HLS is running C/RTL co-simulation. Final export remains gated.
        log=(job/'hls.log').read_text(errors='replace')
        assert state['mode']=='export' and 'INFO: [COSIM 212-47]' in log and 'ERROR:' not in log
        assert (job/f'project/solution1/syn/report/{top_name(job,args.kernel)}_csynth.rpt').exists()
    else:
        assert state['state']=='SUCCEEDED' and state['mode'] in ('synth','cosim','export','package')
    for entry in json.loads((job/'source_manifest.json').read_text(encoding='utf-8-sig')):
        assert hashlib.sha256((job/entry['path']).read_bytes()).hexdigest()==entry['sha256']
    vector_info=json.loads((vectors/'vectors.json').read_text())
    for name,digest in vector_info['sha256'].items():
        assert hashlib.sha256((vectors/name).read_bytes()).hexdigest()==digest
    output.mkdir(parents=True,exist_ok=False)
    status_path=output/'status.json'
    record=dict(state='RUNNING',kernel=args.kernel,clock_period_ns=period,hls_job=str(job),hls_state_at_freeze=state['state'],started_utc=datetime.now(timezone.utc).isoformat(),
                scope='Isolated IP random AXI/reset/direct done barrier; common DDR address space, no DDR PHY or two-core contention')
    status_path.write_text(json.dumps(record,indent=2))
    try:
        top=top_name(job,args.kernel)
        scratch=Path('C:/h/orb4/axi')/output.name
        scratch.mkdir(parents=True,exist_ok=False)
        manifest=[]
        for source in (job/'project/solution1/syn/verilog').iterdir():
            if source.suffix in ('.v','.sv','.dat'):
                shutil.copyfile(source,scratch/source.name)
                digest=hashlib.sha256(source.read_bytes()).hexdigest()
                assert hashlib.sha256((scratch/source.name).read_bytes()).hexdigest()==digest
                manifest.append(dict(name=source.name,sha256=digest))
        (output/'rtl_manifest.json').write_text(json.dumps(manifest,indent=2))
        cases=[int(i) for i in args.cases.split(',')]
        result=output/'result.json'
        config=dict(kernel=args.kernel,vectors=str(vectors),offsets=control_offsets(job,args.kernel,top),
                    cases=cases,result=str(result),reset_case=args.reset_case,clock_period_ns=period,
                    read_concurrency=args.read_concurrency,first_read_latency=[args.first_read_min,args.first_read_max])
        config_path=output/'config.json';config_path.write_text(json.dumps(config,indent=2))
        modelsim=Path('C:/modeltech64_2020.4/win64')
        os.environ['MGLS_LICENSE_FILE']=str(modelsim/'LICENSE.TXT')
        os.environ['PATH']=str(modelsim)+os.pathsep+os.environ['PATH']
        source_tests=Path(__file__).resolve().parent
        here=output/'inputs'
        shutil.copytree(source_tests,here,ignore=shutil.ignore_patterns('__pycache__'))
        test_manifest=[dict(path=str(f.relative_to(output)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in here.rglob('*') if f.is_file()]
        (output/'test_manifest.json').write_text(json.dumps(test_manifest,indent=2)+'\n')
        record.update(config_sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),
                      vectors_sha256=hashlib.sha256((vectors/'vectors.json').read_bytes()).hexdigest())
        status_path.write_text(json.dumps(record,indent=2)+'\n')
        sys.path[:0]=[str(here)]
        os.environ['PYTHONPATH']=str(here)+os.pathsep+os.environ.get('PYTHONPATH','')
        os.environ['ORB60_DATAPATH_CONFIG']=str(config_path)
        configure_runtime(output/'runtime')
        print(f'ORB60_DATAPATH_AXI_BEGIN kernel={args.kernel} cases={cases}',flush=True)
        runner=get_runner('questa')
        runner.build(hdl_toplevel=top,sources=sorted(scratch.glob('*.v'))+sorted(scratch.glob('*.sv')),
                     build_dir=scratch,always=True)
        # Preserve the controlled top ports/nets only; the HLS internals may
        # optimize normally. ap_done remains visible for the direct barrier.
        test_result=runner.test(hdl_toplevel=top,test_module='test_datapath_axi',build_dir=scratch,
            test_args=['-lic_noqueue','-no_autoacc',f'-voptargs=+acc=npr0+{top}','-l',str(output/'modelsim.log')])
        total,failed=get_results(test_result)
        proof=json.loads(result.read_text()) if result.exists() else {}
        assert total>0 and failed==0 and proof.get('status')=='PASS'
        assert proof['transactions']==len(cases) and proof['reset_interruption'] and proof['direct_completion_barrier']
        runtime=json.loads((output/'runtime/python_runtime.json').read_text())
        assert Path(runtime['loaded_dll'])==Path(runtime['expected_dll'])
        record.update(state='SUCCEEDED',result=str(result),runtime=runtime)
        print(f'ORB60_DATAPATH_AXI_PASS kernel={args.kernel} cases={len(cases)}',flush=True)
    except BaseException as error:
        record.update(state='FAILED',error=str(error));raise
    finally:
        record['finished_utc']=datetime.now(timezone.utc).isoformat()
        status_path.write_text(json.dumps(record,indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--hls-job',type=Path,required=True);p.add_argument('--vectors',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--kernel',choices=('layer4','descriptor'),required=True)
    p.add_argument('--cases',required=True);p.add_argument('--reset-case',type=int,required=True)
    p.add_argument('--read-concurrency',type=int,choices=(1,2,8,16),default=1)
    p.add_argument('--first-read-min',type=int,default=0);p.add_argument('--first-read-max',type=int,default=5)
    run(p.parse_args())
