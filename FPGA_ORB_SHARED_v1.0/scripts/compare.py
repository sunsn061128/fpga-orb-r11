"""Field-exact RTL stage comparison. Exit 0=equal, 1=mismatch, 2=invalid input.

No image loading, tolerance, or endian conversion that could mask an RTL error.
"""
import argparse
import csv
import json
import pathlib
import struct
import sys

HEADER = [(0,8,'magic'),(8,2,'version'),(10,2,'header_bytes'),(12,2,'record_bytes'),
          (14,1,'role'),(15,1,'flags'),(16,8,'timestamp_ns'),(24,4,'width'),(28,4,'height'),
          (32,4,'point_count'),(36,4,'levels'),(40,4,'scaleFactor_f32_bits'),(44,4,'budget'),
          (48,4,'descriptor_bytes'),(52,4,'coordinate_space'),(56,4,'monoLeft'),(60,4,'reserved')]
FIELDS = ['x_f32_bits','y_f32_bits','octave','angle_f32_bits','size_f32_bits',
          'response_f32_bits','class_id','reserved']

def validate_packet(data):
    if len(data)<64 or data[:8]!=b'ORBFEAT\0':
        raise ValueError('Invalid B1 magic/length')
    if struct.unpack_from('<HHH',data,8)!=(1,64,64):
        raise ValueError('Unsupported B1 version/layout')
    count=struct.unpack_from('<I',data,32)[0]
    if len(data)!=64+64*count:
        raise ValueError('B1 length does not match point_count')
    if data[14]>1 or data[15] or data[60:64]!=b'\0'*4:
        raise ValueError('Invalid B1 role/reserved')
    if struct.unpack_from('<II',data,48)!=(32,0):
        raise ValueError('B1 descriptor/coordinate format')
    return count

def mismatch(expected,actual,**position):
    return dict(equal=False,expected=expected,actual=actual,**position)

def compare_file(expected,actual,width=None):
    expected,actual=pathlib.Path(expected),pathlib.Path(actual)
    if expected.suffix=='.csv':
        with expected.open(newline='') as e,actual.open(newline='') as a:
            er,ar=list(csv.reader(e)),list(csv.reader(a))
        if not er or not ar or er[0]!=ar[0]:
            return mismatch(er[0] if er else None,ar[0] if ar else None,field='CSV header')
        for row in range(1,max(len(er),len(ar))):
            if row>=len(er) or row>=len(ar):
                return mismatch(len(er)-1,len(ar)-1,row=row-1,field='row count')
            if len(er[row])!=len(er[0]) or len(ar[row])!=len(ar[0]):
                raise ValueError('Malformed CSV record at row '+str(row))
            for column,(ev,av) in enumerate(zip(er[row],ar[row])):
                if int(ev)!=int(av):
                    return mismatch(int(ev),int(av),row=row-1,field=er[0][column])
        return dict(equal=True,rows=len(er)-1)
    e,a=expected.read_bytes(),actual.read_bytes()
    if expected.suffix=='.orb':
        validate_packet(e);validate_packet(a)
        for offset,n,name in HEADER:
            if e[offset:offset+n]!=a[offset:offset+n]:
                return mismatch(e[offset:offset+n].hex(),a[offset:offset+n].hex(),offset=offset,field=name)
        for offset in range(64,len(e),4):
            if e[offset:offset+4]!=a[offset:offset+4]:
                record=(offset-64)//64;local=(offset-64)%64
                if local<32:
                    return mismatch(e[offset:offset+4].hex(),a[offset:offset+4].hex(),record=record,field=FIELDS[local//4],offset=offset)
                for byte in range(offset,offset+4):
                    if e[byte]!=a[byte]:
                        xor=e[byte]^a[byte];bit=(xor&-xor).bit_length()-1
                        return mismatch(e[byte],a[byte],record=record,field='descriptor',byte=local-32+byte-offset,bit=bit,offset=byte)
        return dict(equal=True,records=(len(e)-64)//64)
    if len(e)!=len(a):
        return mismatch(len(e),len(a),field='byte length',offset=min(len(e),len(a)))
    for offset,(ev,av) in enumerate(zip(e,a)):
        if ev!=av:
            pos=dict(offset=offset)
            if 'descriptors' in expected.name:
                xor=ev^av;pos.update(record=offset//32,byte=offset%32,bit=(xor&-xor).bit_length()-1)
            elif width:
                pos.update(y=offset//width,x=offset%width)
            return mismatch(ev,av,**pos)
    return dict(equal=True,bytes=len(e))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--expected',required=True,type=pathlib.Path)
    p.add_argument('--actual',required=True,type=pathlib.Path)
    p.add_argument('--level',type=int,default=0)
    p.add_argument('--stage',choices=['all','pyramid','fast','fast_high','fast_low','nms','nms_high','nms_low','grid','grid_thresholds','moments','gaussian','descriptors','packet'],default='all')
    p.add_argument('--width',type=int)
    args=p.parse_args()
    if args.expected.is_file():
        result=compare_file(args.expected,args.actual,args.width)
    else:
        meta=json.loads((args.expected/'trace.json').read_text())
        stages=['pyramid','fast','nms','grid','moments','gaussian','descriptors'] if args.stage=='all' else [args.stage]
        if args.stage=='all' and meta.get('schema',1)>=2:stages+=['fast_high','fast_low','nms_high','nms_low','grid_thresholds']
        suffixes={'pyramid':'u8','fast':'u8','nms':'csv','grid':'csv','moments':'csv','gaussian':'u8','descriptors':'u8','fast_high':'u8','fast_low':'u8','nms_high':'csv','nms_low':'csv','grid_thresholds':'csv'}
        result=dict(equal=True)
        for stage in stages:
            if stage=='packet':raise ValueError('Pass .orb file for --stage packet')
            name='L%d_%s.%s'%(args.level,stage,suffixes[stage])
            result=compare_file(args.expected/name,args.actual/name,meta['levels'][args.level]['width'])
            result.update(level=args.level,stage=stage)
            if not result['equal']:break
    print(json.dumps(result,ensure_ascii=False))
    return 0 if result['equal'] else 1

if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,KeyError,IndexError) as error:
        print(json.dumps(dict(equal=False,invalid_input=str(error))))
        sys.exit(2)
