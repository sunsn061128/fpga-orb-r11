"""Read-only comparison of a physically measured trial against frozen r15."""
from pathlib import Path
import argparse, hashlib, json, re

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def counters(run,frequency):
    words = run['mailbox']
    image, descriptor = words[20:28], words[28:36]
    critical = image[0] + sum(max(descriptor[n], image[n+1]) for n in range(7)) + descriptor[7]
    per_ms=frequency*1000
    return dict(image_cycles=image, descriptor_cycles=descriptor,
                frequency_mhz=frequency,initial_image_ms=image[0]/per_ms,
                descriptor_sum_ms=sum(descriptor)/per_ms,
                overlap_model_ms=critical/per_ms,
                wall_ms=run['wall_us']/run['frames']/1000,
                residual_ms=run['wall_us']/run['frames']/1000-critical/per_ms,
                descriptor_dominates_pairs=[descriptor[n]>image[n+1] for n in range(7)])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--job', type=Path, required=True)
    parser.add_argument('--baseline',choices=('r15','r16'),default='r15')
    args = parser.parse_args()
    job = args.job.resolve()
    frozen = read(ROOT/f'evidence/baseline_{args.baseline}.json')
    baseline = Path(frozen['Path'])
    assert frozen['State']=='FROZEN_ACCEPTED'
    assert sha(baseline/'MANIFEST.json') == frozen['ManifestSHA256']
    release = baseline/'release'
    for name in ('acceptance.json', 'normal/golden.bin', 'inputs/board.c'):
        item = next(i for i in read(release/'manifest.json')['Files'] if i['Path']==name)
        assert sha(release/name)==item['SHA256']
    old_acceptance=read(release/'acceptance.json')
    old = old_acceptance['PhysicalResults']['normal_Production']
    state = read(job/'status.json')
    assert state['state']=='SUCCEEDED' and state['part']==frozen['Part'] and state['frequency_mhz'] in (100,125)
    assert (job/'vivado/accepted.stamp').is_file()
    assert sha(job/'normal/golden.bin')==sha(release/'normal/golden.bin')
    assert sha(job/'ps_inputs/board.c')==sha(release/'inputs/board.c'), 'PS schedule changed; investigate comparison scope'
    candidates = [read(p) for p in sorted(job.glob('*_normal_Production_*.json'))]
    candidates = [r for r in candidates if r['state']=='SUCCEEDED' and r['frames']>=100]
    assert candidates, 'Require actual normal Production >=100 frames'
    new = candidates[-1]
    match = re.search(r'ORB60_MAILBOX_WORDS=([^\r\n]+)', Path(new['log']).read_text())
    assert match and [int(v,0) for v in match[1].split()]==new['mailbox']
    assert new['mailbox'][4]==4 and new['mailbox'][5]==0 and new['features']==old['features']
    assert new['mailbox'][78]==old['mailbox'][78]==0x44445231
    old_fps, new_fps = old['frames']*1e6/old['wall_us'], new['frames']*1e6/new['wall_us']
    print(json.dumps(dict(baseline=args.baseline,job=str(job),baseline_fps=old_fps,
                         trial_fps=new_fps,gain_percent=100*(new_fps/old_fps-1),
                         baseline_counters=counters(old,old_acceptance['PLMHz']),trial=counters(new,state['frequency_mhz']),
                         scope='Same fixture, budgets, PS source and resident DDR; clocks reported explicitly. Counter overlap model excludes final statistics and responses; residual is not CPU-only time.'),indent=2))

if __name__=='__main__':
    main()
