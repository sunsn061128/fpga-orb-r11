"""Bounded native functional gates; no finite-FIFO timing/performance claims."""
from pathlib import Path
import os, subprocess, json, hashlib, time
ROOT=Path(__file__).resolve().parents[1]
NEXT=ROOT/'upstream/FPGA_ORB_HLS/fastv2/next'
REF=ROOT/'upstream/FPGA_ORB_SHARED_v1.0'
COMPILER=Path('C:/Users/dmsun/mingw/mingw64/bin/g++.exe')
BUILD=ROOT/'build/host'
BUILD.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
env['PATH']=str(COMPILER.parent)+';'+env.get('SystemRoot','C:/Windows')+'/System32;'+env.get('SystemRoot','C:/Windows')
records=[]
def run(name,args,timeout=300):
    start=time.monotonic()
    with (BUILD/(name+'.log')).open('w',encoding='utf-8') as f:
        p=subprocess.run([str(a) for a in args],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=timeout,cwd=ROOT)
    records.append({'name':name,'returncode':p.returncode,'seconds':round(time.monotonic()-start,3)})
    print(name, 'PASS' if p.returncode==0 else 'FAIL',flush=True)
    if p.returncode: raise RuntimeError(str(BUILD/(name+'.log')))
def compile(name,sources):
    exe=BUILD/(name+'.exe')
    args=[COMPILER,'-std=c++17','-O2','-Wall','-Wextra','-Wno-unknown-pragmas','-Wno-unused-label']
    for include in [NEXT/'../include',NEXT/'include',NEXT/'hls',BUILD,REF]: args+=['-I',include]
    run(name+'_compile',args+sources+['-o',exe]);return exe
def main():
    if '--descriptor-bench-only' in os.sys.argv:
        previous=json.loads((BUILD/'status.json').read_text())
        if previous['state']!='HOST_EXACT_PASS':raise RuntimeError('Require previous completed gate')
        for item in previous['sources']:
            if item['path'].replace('\\','/').startswith(('scripts/','tests/')):continue
            if hashlib.sha256((ROOT/item['path']).read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Native prerequisite source drift')
        records.extend(previous['tests'])
        bench=compile('descriptor2018_bench',[NEXT/'hls/tb_descriptor2018.cpp',BUILD/'descriptor2018.o'])
        run('descriptor2018_bench30',[bench,ROOT.parent/'orb60_board/20261012_172441_100mhz'])
        save_success();return
    if '--frames-only' in os.sys.argv:
        previous=json.loads((BUILD/'status.json').read_text())
        records.extend(r for r in previous['tests'] if r['returncode']==0 and not r['name'].startswith('frame4'))
        required=['layer4_exact21','gaussian_exact14','resize_exact13','quad_boundaries','descriptor_angle','descriptor_changing']
        if any(not any(r['name']==n for r in records) for n in required): raise RuntimeError('Incomplete prerequisite gates')
        frame=compile('frame4',[ROOT/'tests/test_frame4.cpp',BUILD/'layer4.o',BUILD/'descriptor2018.o'])
        for label in ['normal','noise','zero']:
            golden=ROOT.parent/'orb60_board/20261012_172441_100mhz'/label/'golden.bin'
            run('frame4_'+label,[frame,golden])
        save_success();return
    (BUILD/'status.json').write_text(json.dumps({'state':'RUNNING','tests':[]},indent=2)+'\n')
    run('quad_rom',[Path(os.sys.executable),ROOT/'scripts/generate_quad_rom.py'])
    run('image_migration',[Path(os.sys.executable),ROOT/'scripts/migrate_image.py'])
    run('compiler_version',[COMPILER,'--version'])
    generator=compile('layer_vectors',[NEXT/'host/generate_layer2_vectors.cpp',REF/'golden.cpp'])
    run('independent_cpu_vectors',[generator,BUILD/'layer2_vectors.inc'])
    model=BUILD/'layer4.o'
    args=[COMPILER,'-std=c++17','-O2','-Wall','-Wextra','-Wno-unknown-pragmas','-Wno-unused-label']
    for inc in [NEXT/'../include',NEXT/'include']: args+=['-I',inc]
    run('layer4_model_compile',args+['-c',NEXT/'src/layer4_hls.cpp','-o',model])
    stream=compile('layer4_stream',[NEXT/'hls/tb_layer4_stream.cpp',model]);run('layer4_exact21',[stream])
    images=compile('layer4_images',[NEXT/'host/test_layer4_images.cpp',model])
    run('gaussian_exact14',[images,'0']);run('resize_exact13',[images,'1'])
    boundaries=compile('quad_boundaries',[ROOT/'tests/test_quad_boundaries.cpp',model]);run('quad_boundaries',[boundaries])
    run('descriptor_migration',[Path(os.sys.executable),ROOT/'scripts/migrate_descriptor.py'])
    descriptor=BUILD/'descriptor2018.o'
    run('descriptor_model_compile',args+['-c',NEXT/'src/descriptor2018_hls.cpp','-o',descriptor])
    for name,source in [('descriptor_angle', 'test_backend_angle.cpp'),('descriptor_changing','test_backend_changing.cpp')]:
        exe=compile(name,[NEXT/'host'/source,REF/'golden.cpp',descriptor,'-DNEXT_TEST_PL_DESCRIPTOR=1'])
        run(name,[exe])
    bench=compile('descriptor2018_bench',[NEXT/'hls/tb_descriptor2018.cpp',descriptor])
    run('descriptor2018_bench30',[bench,ROOT.parent/'orb60_board/20261012_172441_100mhz'])
    frame=compile('frame4',[ROOT/'tests/test_frame4.cpp',model,descriptor])
    for label in ['normal','noise','zero']:
        golden=ROOT.parent/'orb60_board/20261012_172441_100mhz'/label/'golden.bin'
        run('frame4_'+label,[frame,golden])
    save_success()
def save_success():
    sources=[p for p in NEXT.rglob('*') if p.is_file()]+list((ROOT/'scripts').glob('*.py'))+list((ROOT/'tests').glob('*.cpp'))
    manifest=[{'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources]
    (BUILD/'status.json').write_text(json.dumps({'state':'HOST_EXACT_PASS','compiler':str(COMPILER),'tests':records,'sources':manifest},indent=2)+'\n')
    print('ORB4_HOST_EXACT_PASS; hardware timing/finite FIFO gates not run',flush=True)
if __name__=='__main__':
    try:main()
    except Exception as e:
        (BUILD/'status.json').write_text(json.dumps({'state':'FAILED','tests':records,'error':str(e)},indent=2)+'\n')
        raise
