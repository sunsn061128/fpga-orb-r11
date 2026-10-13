"""Fresh PS-only trial paired with an unchanged, previously accepted routed bit."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import shutil
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--from-job', type=Path, required=True)
    args = parser.parse_args()
    parent = args.from_job.resolve()
    state = json.loads((parent / 'status.json').read_text(encoding='utf-8-sig'))
    pairing = json.loads((parent / 'pairing.json').read_text(encoding='utf-8-sig'))
    assert state['state'] == 'SUCCEEDED' and pairing['State'] == 'PASS'
    assert state['part'] == 'xc7z015clg485-2' and state['frequency_mhz'] == 100
    assert (parent / 'vivado/accepted.stamp').exists()
    for item in pairing['Files']:
        assert hashlib.sha256(Path(item['Path']).read_bytes()).hexdigest().upper() == item['SHA256']
    bit = Path(state.get('bit_path', parent / 'vivado/p.runs/impl_1/s_wrapper.bit')).resolve()
    assert bit.is_file() and any(Path(item['Path']).resolve() == bit or
                               (Path(item['Path']).suffix == '.bit' and item['SHA256'] == hashlib.sha256(bit.read_bytes()).hexdigest().upper())
                               for item in pairing['Files'])
    job = Path('C:/h/orb4') / (datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '_ps_direct_' + uuid.uuid4().hex[:8])
    job.mkdir(parents=True, exist_ok=False)
    shutil.copytree(parent / 'inputs', job / 'inputs')
    (job / 'vivado').mkdir()
    files = [parent / 'vivado/accepted.stamp'] + list((parent / 'vivado').glob('post_route*'))
    evidence = []
    for source in files:
        destination = job / 'vivado' / source.name
        shutil.copy2(source, destination)
        evidence.append(dict(source=str(source), copy=str(destination), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    state.update(job=str(job), bit_path=str(bit), reused_routed_from=str(parent),
                 scope='PS-only trial; unchanged accepted routed bit. No new synthesis/route claimed.')
    # build.ps1 must validate the actual PS initialization associated with this bit.
    state['checkpoint_parent'] = state.get('checkpoint_parent', str(parent))
    state.pop('pid', None)
    (job / 'status.json').write_text(json.dumps(state, indent=2) + '\n')
    (job / 'reused_route_evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    (ROOT / 'evidence/last_ps_trial.json').write_text(json.dumps(state, indent=2) + '\n')
    print('ORB4_PS_TRIAL ' + str(job), flush=True)


if __name__ == '__main__':
    main()
