"""Read-only integrity and performance-provenance check for the version branch."""
from pathlib import Path
import hashlib, json

ROOT=Path(__file__).resolve().parents[1]

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def verify(directory,items,path_key='Path',hash_key='SHA256'):
    count=0
    for item in items:
        relative=Path(item[path_key])
        assert not relative.is_absolute() and '..' not in relative.parts,item[path_key]
        path=directory/relative
        assert path.is_file(),f'Missing: {path}'
        assert sha(path)==item[hash_key].upper(),f'Hash mismatch: {path}'
        if 'Bytes' in item:assert path.stat().st_size==item['Bytes'],path
        count+=1
    return count

publication=read(ROOT/'docs/VERSION_PUBLICATION_MANIFEST.json')
count=verify(ROOT,publication['Files'])
assert count==publication['FileCount']
baselines={}
for name,expected in publication['OriginalBaselineManifestSHA256'].items():
    directory=ROOT/name
    assert sha(directory/'MANIFEST.json')==expected,name
    baselines[name]=verify(directory,read(directory/'MANIFEST.json')['Files'])
archive=ROOT/'archive/2024_2'
archived=verify(archive,read(archive/'PACKAGE_CONTENTS.json')['files'],'path','sha256')
summary=read(ROOT/'docs/VERSION_RESULTS.json')
for version in summary['MainPhysicalSeries']:
    acceptance=read(ROOT/version['Evidence'])
    if 'PhysicalResults' in acceptance:
        prod=acceptance['PhysicalResults']['normal_Production']
        measured=prod['frames']*1e6/prod['wall_us']
        assert version['PLMHz']==acceptance['PLMHz']
    else:
        measured=acceptance['Results']['production']['LoopFPS']
    assert abs(version['FPS']-measured)<1e-10,version['Name']
assert summary['CurrentBaseline']=='u1'
assert summary['MainPhysicalSeries'][-1]['FPS']==read(ROOT/'u1/BASELINE.json')['FPS']
print(json.dumps(dict(State='ALL_VERSIONS_HASH_PASS',CopiedFiles=count,Baselines=baselines,
    Archived2024Files=archived,CurrentBaseline='u1',NoFreshBuildOrBoardTest=True),ensure_ascii=False))
