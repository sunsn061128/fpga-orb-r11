"""Portable packaged vector replay. Standard-library Python; no OpenCV needed."""
import argparse,hashlib,json,pathlib,subprocess,sys
from compare import compare_file
ROOT=pathlib.Path(__file__).resolve().parents[1]
STAGES={'pyramid':'u8','fast':'u8','nms':'csv','grid':'csv','moments':'csv','gaussian':'u8','descriptors':'u8'}
def digest(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1<<20),b''):h.update(b)
    return h.hexdigest()
def check_hashes(root):
    rows=(root/'SHA256SUMS').read_text(encoding='utf-8').splitlines();count=0
    for row in rows:
        expected,name=row.split('  ',1);p=(root/name).resolve()
        if root.resolve() not in p.parents:raise ValueError('Unsafe manifest path: '+name)
        if digest(p)!=expected:raise ValueError('SHA256 mismatch: '+name)
        count+=1
    return count
def compare_result(expected,actual,config):
    results=[]
    results.append(('packet',compare_file(expected/'packet.orb',actual/'packet.orb')))
    reference=json.loads((expected/'trace/trace.json').read_text(encoding='utf-8'))
    produced=json.loads((actual/'trace/trace.json').read_text(encoding='utf-8'))
    if len(produced['levels'])!=config['levels'] or produced['angle_bins']!=32 or bool(produced['dual_threshold'])!=config['dual_threshold'] or produced['grid_top_k']!=1:
        return dict(equal=False,stage='configuration',detail='Actual trace configuration differs from vector manifest')
    stages=dict(STAGES)
    if reference.get('schema',1)>=2:
        stages.update(fast_high='u8',fast_low='u8',nms_high='csv',nms_low='csv',grid_thresholds='csv')
        for key in ['schema','threshold_high','threshold_low','fixed_fast','grid','blur']:
            if reference[key]!=produced[key]:return dict(equal=False,stage='configuration',field=key)
    for level in range(config['levels']):
        # Default archives predate the additional T2 metadata fields. Pixel,
        # point and descriptor data remain exact, not tolerance comparisons.
        for key in ['level','width','height','scale_q24','budget','candidates','selected','cell_side','cell_cols','cell_rows','empty_cells']:
            if reference['levels'][level][key]!=produced['levels'][level][key]:
                return dict(equal=False,level=level,stage='metadata',field=key,expected=reference['levels'][level][key],actual=produced['levels'][level][key])
        if config['dual_threshold'] and reference['levels'][level]['fallback_cells']!=produced['levels'][level]['fallback_cells']:
            return dict(equal=False,level=level,stage='metadata',field='fallback_cells')
        for stage,suffix in stages.items():
            name='L%d_%s.%s'%(level,stage,suffix)
            r=compare_file(expected/'trace'/name,actual/'trace'/name,reference['levels'][level]['width'])
            if not r['equal']:return dict(r,level=level,stage=stage)
    results.append(('output_order',compare_file(expected/'trace/output_order.csv',actual/'trace/output_order.csv')))
    for stage,r in results:
        if not r['equal']:return dict(r,stage=stage)
    return dict(equal=True,levels=config['levels'],stages_per_level=len(stages),packet_and_order_checked=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('replay'))
    p.add_argument('--case');p.add_argument('--profile',choices=['L4','L8','L4_T2','L8_T2']);p.add_argument('--role',choices=['normal','initialization'])
    p.add_argument('--all',action='store_true');p.add_argument('--verify-only',action='store_true');a=p.parse_args()
    manifest=json.loads((ROOT/'vectors/manifest.json').read_text(encoding='utf-8'));count=check_hashes(ROOT)
    if a.verify_only:print(json.dumps(dict(status='VERIFIED',hashed_files=count,inputs=len(manifest['inputs']),expected_cases=len(manifest['expected']))));return 0
    if not a.binary:raise ValueError('--binary required for replay')
    if not a.all and not (a.case and a.profile and a.role):raise ValueError('Use --all, or provide --case --profile --role')
    cases=[c for c in manifest['expected'] if (a.case is None or c['case']==a.case) and (a.profile is None or c['profile']==a.profile) and (a.role is None or c['role_name']==a.role)]
    if not cases:raise ValueError('No matching archived vector; see vectors/manifest.json')
    binary=a.binary.resolve()
    if not binary.is_file():raise ValueError('Missing compiled CLI: '+str(binary))
    a.output.mkdir(parents=True,exist_ok=False);results=[]
    for entry in cases:
        cfg=entry['configuration'];target=a.output/entry['case']/(entry['profile']+'_'+entry['role_name']);target.mkdir(parents=True)
        cmd=[str(binary),'--input',str(ROOT/entry['input']),'--output',str(target/'packet.orb'),'--trace',str(target/'trace'),
            '--levels',str(cfg['levels']),'--features',str(cfg['features']),'--timestamp',str(cfg['timestamp_ns']),'--role',str(cfg['role']),
            '--scale-num','6','--scale-den','5','--threshold','20','--low-threshold','7','--fixed-fast','1','--grid','1','--blur','1','--angle-bins','32',
            '--dual-threshold',str(int(cfg['dual_threshold'])),'--grid-top-k','1','--overlap-left','0','--overlap-right','1000']
        with (target/'run.log').open('w') as log:completed=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
        r=dict(case=entry['case'],profile=entry['profile'],role=entry['role_name'],exit_code=completed.returncode,command=cmd)
        if completed.returncode:r.update(equal=False,stage='producer',reason='See retained run.log')
        else:r.update(compare_result(ROOT/entry['expected'],target,cfg))
        results.append(r);print(json.dumps({k:v for k,v in r.items() if k!='command'}),flush=True)
        (a.output/'replay_results.json').write_text(json.dumps(dict(status='PASSED' if all(r['equal'] for r in results) else 'FAILED',results=results),indent=2)+'\n',encoding='utf-8')
        if not r['equal']:return 1
    print('VECTOR_REPLAY_PASS cases='+str(len(results)));return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,OSError,KeyError) as e:print(json.dumps(dict(status='INVALID_INPUT',reason=str(e))));raise SystemExit(2)
