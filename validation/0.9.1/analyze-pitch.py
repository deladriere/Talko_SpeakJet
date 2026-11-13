from pathlib import Path
import json, wave
import numpy as np
root=Path(__file__).resolve().parent
metrics={}
for name,(minimum,maximum) in {'low':(15,35),'noon':(65,110),'high':(180,300)}.items():
    with wave.open(str(root/f'pitch-{name}.wav'),'rb') as w:
        rate=w.getframerate();channels=w.getnchannels()
        assert w.getsampwidth()==2
        data=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').reshape(-1,channels).astype(float)/32768
    # Select the stronger MG-XU input channel and a sustained window away from onset.
    signal=data[:,np.argmax(np.mean(data*data,axis=0))]
    energy=np.convolve(signal*signal,np.ones(1024)/1024,mode='same')
    active=np.flatnonzero(energy>energy.max()*.25)
    start=int(active[0]+.2*rate)
    x=signal[start:min(start+int(.7*rate),active[-1])];x=x-x.mean()
    assert len(x)>rate*.3
    length=1<<(2*len(x)-1).bit_length()
    spectrum=np.fft.rfft(x,length);corr=np.fft.irfft(spectrum*np.conj(spectrum),length)[:len(x)]
    corr/=np.arange(len(x),0,-1)
    lo=int(rate/maximum);hi=int(rate/minimum)
    lag=lo+np.argmax(corr[lo:hi])
    metrics[name]={'fundamental_hz':rate/float(lag),'peak_dbfs':float(20*np.log10(np.abs(data).max())),
                   'rate':rate,'lag_samples':int(lag),'method':'autocorrelation of sustained IY; expected octave range constrained'}
(root/'pitch-analysis.json').write_text(json.dumps(metrics,indent=2)+'\n')
print(json.dumps(metrics,indent=2))
