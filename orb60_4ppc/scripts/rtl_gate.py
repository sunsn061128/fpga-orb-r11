"""Validate the complete independent layer C/reference-to-RTL proof."""
import hashlib,json
from pathlib import Path
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify_layer21(proof,hls,period):
    proof=Path(proof);hls=Path(hls);state=read(proof/'status.json');config=read(proof/'config.json');result=read(proof/'result.json')
    assert state['state']=='SUCCEEDED' and state['kernel']=='layer4' and state['clock_period_ns']==period
    assert sha(proof/'config.json')==state['config_sha256']
    vectors=Path(config['vectors']);assert sha(vectors/'vectors.json')==state['vectors_sha256']
    assert config['cases']==list(range(21)) and config['reset_case']==14
    assert result['status']=='PASS' and result['transactions']==21 and result['direct_completion_barrier'] and result['reset_interruption']
    assert [c['case'] for c in result['cases']]==list(range(21))
    v=read(vectors/'vectors.json');origin=read(vectors/'origin_vectors.json')
    assert len(v['cases'])==21 and origin['fast_python_cpp_exact']==19 and v['cases'][:19]==origin['cases']
    assert sha(vectors/'origin_vectors.json')==v['full_layer_gate']['original_vectors_sha256']
    assert [c['level'] for c in v['cases'][19:]]==[-1,8] and all(c['status']==1 and c['args']==[c['level'],217,20,7,1,1000] for c in v['cases'][19:])
    for name,digest in origin['sha256'].items():assert v['sha256'][name]==digest
    for name,digest in v['sha256'].items():assert sha(vectors/name)==digest
    for item in read(proof/'test_manifest.json'):assert sha(proof/item['path'])==item['sha256']
    for item in read(proof/'rtl_manifest.json'):assert sha(hls/'project/solution1/syn/verilog'/item['name'])==item['sha256']
    return dict(type='Independent ModelSim 2020.4 full C/reference-to-RTL with random AXI',cases=21,proof=str(proof.resolve()),hls=str(hls.resolve()),period_ns=period,result_sha256=sha(proof/'result.json'),config_sha256=sha(proof/'config.json'),vectors_sha256=sha(vectors/'vectors.json'))
