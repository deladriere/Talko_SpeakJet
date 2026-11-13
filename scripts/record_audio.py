import ctypes as c, ctypes.util, sys, time, wave
pa=c.CDLL(c.util.find_library('portaudio') or 'libportaudio.dylib')
class Info(c.Structure):
    _fields_=[('version',c.c_int),('name',c.c_char_p),('host',c.c_int),('inputs',c.c_int),('outputs',c.c_int),('lowIn',c.c_double),('lowOut',c.c_double),('highIn',c.c_double),('highOut',c.c_double),('rate',c.c_double)]
class Params(c.Structure):
    _fields_=[('device',c.c_int),('channels',c.c_int),('format',c.c_ulong),('latency',c.c_double),('hostInfo',c.c_void_p)]
pa.Pa_GetDeviceInfo.restype=c.POINTER(Info)
pa.Pa_GetErrorText.restype=c.c_char_p
pa.Pa_OpenStream.argtypes=[c.POINTER(c.c_void_p),c.POINTER(Params),c.c_void_p,c.c_double,c.c_ulong,c.c_ulong,c.c_void_p,c.c_void_p]
pa.Pa_ReadStream.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
pa.Pa_StartStream.argtypes=[c.c_void_p]
pa.Pa_StopStream.argtypes=[c.c_void_p]
pa.Pa_CloseStream.argtypes=[c.c_void_p]
def check(code):
    if code<0: raise RuntimeError(pa.Pa_GetErrorText(code).decode())
check(pa.Pa_Initialize())
matches=[]
for n in range(pa.Pa_GetDeviceCount()):
    d=pa.Pa_GetDeviceInfo(n).contents
    if d.name.decode()=='MG-XU' and d.inputs>0: matches.append((n,d))
assert len(matches)==1,'Expected MG-XU input'
n,d=matches[0]
rate=int(d.rate); channels=min(d.inputs,2)
params=Params(n,channels,8,d.highIn,None) # paInt16, CoreAudio converts hardware format.
stream=c.c_void_p()
check(pa.Pa_OpenStream(c.byref(stream),c.byref(params),None,rate,512,0,None,None))
check(pa.Pa_StartStream(stream))
print('Recording MG-XU',rate,'Hz',channels,'channels',flush=True)
buffer=(c.c_int16*(512*channels))()
overflows=0
with wave.open(sys.argv[1],'wb') as f:
    f.setnchannels(channels);f.setsampwidth(2);f.setframerate(rate)
    for _ in range(int(float(sys.argv[2])*rate/512)):
        err=pa.Pa_ReadStream(stream,buffer,512)
        if err==-9981: overflows+=1
        else: check(err)
        f.writeframesraw(bytes(buffer))
check(pa.Pa_StopStream(stream));check(pa.Pa_CloseStream(stream));check(pa.Pa_Terminate())
print('Capture input overflow events',overflows,flush=True)
