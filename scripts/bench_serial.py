from pathlib import Path
import serial,serial.tools.list_ports,time,re,json,sys,argparse
parser=argparse.ArgumentParser()
parser.add_argument("--serial-id",required=True)
parser.add_argument("--output",type=Path,default=Path("validation/0.9.1"))
parser.add_argument("--quick",action="store_true")
args=parser.parse_args()
out=args.output;out.mkdir(parents=True,exist_ok=True)
ports=[p for p in serial.tools.list_ports.comports() if p.serial_number==args.serial_id]
assert len(ports)==1
assert ports[0].product=='Talko_SpeakJet 0.9.1',ports[0].product
s=serial.Serial(ports[0].device,115200,timeout=.1)
log=[];results=[]
def read_until(predicate,timeout=12):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        line=s.readline().decode(errors='replace').rstrip('\r\n')
        if not line:continue
        frames=re.findall(r'\x1eSAM:[^\x1f]*\x1f',line)
        for frame in frames: log.append('FRAME '+frame)
        clean=re.sub(r'\x1eSAM:[^\x1f]*\x1f','',line)
        if clean: log.append(clean)
        if predicate(clean): return clean
        if frames and predicate(line): return line
    raise RuntimeError('Timed out awaiting result: '+str(log[-8:]))
def run(command,timeout=12):
    s.write((command+'\n').encode());s.flush()
    line=read_until(lambda l:l.startswith('RESULT '),timeout)
    record=dict(re.findall(r'(\w+)=([^ ]+)',line));record['command']=command
    if command.startswith('PLAY '):
        _,bank,item=command.split();assert record['bank']==bank and record['item']==item
    elif command.startswith('RAW '):assert record['bank']=='10'
    results.append(record)
    return record
try:
    s.write(b'STATS ON\nSTATUS\n')
    banner=read_until(lambda l:l.startswith('Talko_SpeakJet'))
    assert 'engine=0.4.3' in banner
    if '--quick' not in sys.argv:
        counts=[6,12,54,12,18,72,55,12,10]
        for bank,count in enumerate(counts):
            for item in range(1,count+1):run(f'PLAY {bank} {item}')
            print('Completed bank',bank,'total',len(results),flush=True)
    else:
        for b,i in ((0,1),(0,6),(6,39),(6,40),(6,55),(8,1),(8,10)):
            run(f'PLAY {b} {i}')
    for seq in ('128 137','141 179 137','132 191 132','187 187','132 8 187 8 187 134',
                '21 108 141 179 137','21 118 128 147 129','132 2 132'):
        run('RAW '+seq)
    # Closely spaced live vowel triggers: repeated labels must each be emitted.
    before=len(log)
    for item in (1,10,1,10,1,10,1,10,1,10,1,10):
        s.write(f'QUEUE 1 {item}\n'.encode());s.flush();time.sleep(.04)
    q=read_until(lambda l:l.startswith('RESULT '),12)
    record=dict(re.findall(r'(\w+)=([^ ]+)',q));record['command']='12 live vowel gates at 25 Hz';results.append(record)
    queue_log=log[before:]
    labels=sum(l in ('IY','OW') for l in queue_log)
    print('Queue spoken labels',labels,flush=True)
    assert labels==12,labels
    # Cut invalidates both the playing stream and queued requests.
    s.write(b'QUEUE 0 6\n');time.sleep(.05);s.write(b'QUEUE 0 1\nCUT\n')
    cut=read_until(lambda l:l.startswith('RESULT ') and 'status=cut' in l)
    assert 'STOP' in log[-20:]
    # Request current pose at rest and speech articulation on the same framed protocol as SAM.
    s.write(b'\x1eSAM\x1f');read_until(lambda l:'SAM:-' in l)
    run('PLAY 0 1')
    assert any('SAM:' in l and 'SAM:-' not in l for l in log[-50:])
    # A closed serial host must not stall the audio core. LAST retains the result.
    s.write(b'RAW 21 60 132 141 179 137 4 187 8 187 128\n');s.flush();time.sleep(.03)
    s.close();time.sleep(20)
    s=serial.Serial(ports[0].device,115200,timeout=.1);s.write(b'LAST\n')
    closed=read_until(lambda l:l.startswith('RESULT '))
    record=dict(re.findall(r'(\w+)=([^ ]+)',closed));record['command']='serial closed during RAW';results.append(record)
finally:
    if s.is_open:s.write(b'STATS OFF\nMONITOR OFF\n')
    s.close()
    suffix='quick' if '--quick' in sys.argv else 'all-banks'
    (out/(suffix+'-serial.log')).write_text('\n'.join(log)+'\n')
    (out/(suffix+'-results.json')).write_text(json.dumps(results,indent=2)+'\n')
if results:
    peak=max(results,key=lambda r:int(r['peak_us']))
    underruns=sum(int(r['underrun_samples']) for r in results)
    late=sum(int(r['late_blocks']) for r in results)
    print('Streams',len(results),'peak',peak['peak_us'],'command',peak['command'],'late blocks',late,'underrun samples',underruns,flush=True)
    assert underruns==0,'Audio underruns occurred; see the saved serial log and results'
