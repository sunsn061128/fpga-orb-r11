"""Portable delivery integrity and comparator checks, without running the model."""
import argparse,json,pathlib,shutil,subprocess,sys,tempfile,os
from replay_vectors import check_hashes,compare_result
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--package',type=pathlib.Path,default=ROOT);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args();package=a.package.resolve()
    if a.output.exists():raise ValueError('Existing verification record preserved')
    result=dict(status='FAILED',algorithm_reexecuted=False,SLAM_reexecuted=False)
    try:
        result['hashed_files']=check_hashes(package)
        manifest=json.loads((package/'vectors/manifest.json').read_text(encoding='utf-8'));result.update(inputs=len(manifest['inputs']),expected_cases=len(manifest['expected']))
        test=subprocess.run([sys.executable,str(package/'tests/test_compare.py')],capture_output=True,text=True)
        result.update(comparator_test_exit=test.returncode,comparator_test_log=test.stdout+test.stderr)
        if test.returncode:raise ValueError('Comparator rejection tests failed')
        # Exercise the complete replay comparator against retained references.
        # Only legacy metadata is adapted to current CLI fields; payload data
        # are hard-linked read-only. This is a tooling check, not a new run.
        with tempfile.TemporaryDirectory(prefix='v095-tool-check-',dir=package.parent) as tmp:
            profiles=list(dict.fromkeys(c['profile'] for c in manifest['expected']))[:2]
            for profile in profiles:
                entry=next(c for c in manifest['expected'] if c['case']=='random_seed_73' and c['profile']==profile and c['role_name']=='normal')
                expected=package/entry['expected'];actual=pathlib.Path(tmp)/profile
                for source in expected.rglob('*'):
                    if source.is_file():
                        dest=actual/source.relative_to(expected);dest.parent.mkdir(parents=True,exist_ok=True)
                        if source.name=='trace.json':
                            meta=json.loads(source.read_text(encoding='utf-8'));meta['dual_threshold']=int(entry['configuration']['dual_threshold']);meta['grid_top_k']=1
                            dest.write_text(json.dumps(meta),encoding='utf-8')
                        else:os.link(source,dest)
                checked=compare_result(expected,actual,entry['configuration'])
                if not checked['equal']:raise ValueError('Full-layer replay comparator check failed: '+json.dumps(checked))
        result.update(status='PASSED',replay_tool_reference_checks=2,model_replay='tooling-only check; actual model replay is recorded separately')
    except Exception as error:result.update(reason=str(error))
    with a.output.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2);f.write('\n')
    print(json.dumps({k:v for k,v in result.items() if k!='comparator_test_log'},ensure_ascii=False))
    return 0 if result['status']=='PASSED' else 1
if __name__=='__main__':raise SystemExit(main())
