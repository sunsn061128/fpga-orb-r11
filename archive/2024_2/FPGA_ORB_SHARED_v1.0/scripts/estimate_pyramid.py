"""Portable v1.0 integer pyramid capacity estimates; not measured FPGA resources."""
import argparse,csv,json,pathlib


def even(n,d):
    q,r=divmod(n,d)
    return q+int(r>d-r or (r==d-r and q%2))


def estimate():
    rows=[];totals=[]
    for w0,h0 in [(960,720),(752,480)]:
        layers=[];scale=1<<24
        for level in range(8):
            w=even(w0*(1<<24),scale);h=even(h0*(1<<24),scale)
            layers.append(dict(input_width=w0,input_height=h0,level=level,width=w,height=h,pixels=w*h,
                window_line_bytes_14w=14*w,separable_line_bytes_20w=20*w,
                resize_source_line_bytes=0 if not level else 2*layers[-1]['width']))
            scale=even(scale*6,5)
        rows+=layers
        for count in (4,8):
            subset=layers[:count];pixels=sum(x['pixels'] for x in subset)
            totals.append(dict(input_width=w0,input_height=h0,nLevels=count,total_pixels=pixels,raw_bytes=pixels,
                raw_plus_gaussian_bytes=2*pixels,
                parallel_window_line_bytes=sum(x['window_line_bytes_14w']+x['resize_source_line_bytes'] for x in subset),
                parallel_separable_line_bytes=sum(x['separable_line_bytes_20w']+x['resize_source_line_bytes'] for x in subset),
                scope='capacity estimate only; not measured BRAM, bandwidth or performance'))
    return dict(layers=rows,totals=totals)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True,type=pathlib.Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);data=estimate()
    (a.output/'storage_estimates.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
    with (a.output/'pyramid_storage.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,list(data['layers'][0]),lineterminator='\n');w.writeheader();w.writerows(data['layers'])
    print(json.dumps(data['totals']))


if __name__=='__main__':main()
