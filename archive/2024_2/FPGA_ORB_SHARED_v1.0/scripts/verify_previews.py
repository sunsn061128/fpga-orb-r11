"""Optional Pillow check: every PNG preview decodes to the original PGM pixels."""
import argparse,json,pathlib
import PIL
from PIL import Image
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--package',type=pathlib.Path,default=ROOT);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
    if a.output.exists():raise ValueError('Existing check record preserved')
    manifest=json.loads((a.package/'vectors/manifest.json').read_text(encoding='utf-8'));checked=[]
    for entry in manifest['inputs']:
        source=a.package/entry['input'];raw=source.read_bytes().split(b'\n',3)[3]
        with Image.open(source.with_suffix('.png')) as im:
            im.load()
            if im.mode!='L' or im.size!=(entry['width'],entry['height']) or im.tobytes()!=raw:raise ValueError('Preview differs: '+entry['case'])
        checked.append(entry['case'])
    record=dict(status='PASSED',independent_decoder='Pillow '+PIL.__version__,preview_count=len(checked),pixel_exact_with_PGM=True,cases=checked)
    with a.output.open('x',encoding='utf-8') as f:json.dump(record,f,indent=2);f.write('\n')
    print('PREVIEW_DECODE_PASS count='+str(len(checked)))
if __name__=='__main__':main()
