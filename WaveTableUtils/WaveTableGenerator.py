import wave,struct
import os
from math import log2, pow
import numpy as np

def noteToFreq(note):
    a = 440 #frequency of A (coomon value is 440Hz)
    return (a / 32) * (2 ** ((note - 9) / 12))
 
def readWaveSamples(file_path):
    sampleArray=[]
    f = wave.open(file_path,"rb")
    params = f.getparams()
    nchannels, sampwidth, framerate, nframes = params[:4]
    if nchannels!=1:
        print("Must be mono wave file.")
        return -1
    if sampwidth!=2:
        print("Must be 16bit sample width.")
        return -1        
    if framerate!=32000:
        print("Sample rate must be 32000 samples/s")
        return -1        

    for i in range(0,nframes):
        waveData = f.readframes(1)
        data = struct.unpack("<h", waveData)
        sampleArray.append(int(data[0]))
    f.close()
    return sampleArray

def calcIncrement(baseFreq,targetFreq):
    return targetFreq/baseFreq   

def estimateSampleFreq(samples,sampleRate):
    estimateFreq=0
    chunk=len(samples)
    # use a Blackman window
    window = np.blackman(chunk)
    # unpack the data and times by the hamming window
    indata = np.array(samples)*window
    # Take the fft and square each value
    fftData=abs(np.fft.rfft(indata))**2
    # find the maximum
    which = fftData[1:].argmax() + 1
    # use quadratic interpolation around the max
    if which != len(fftData)-1:
        y0,y1,y2 = np.log(fftData[which-1:which+2:])
        x1 = (y2 - y0) * .5 / (2 * y1 - y2 - y0)
        # find the frequency and output it
        estimateFreq = (which+x1)*sampleRate/chunk
    else:
        estimateFreq = which*sampleRate/chunk
    return estimateFreq

def exportToSourceFile(sampleName,attackSamples,loopSamples,freq,fileDir,sampleWidth):
    waveTableIdentifer="WaveTable_%s"%sampleName
    attackLen=len(attackSamples)
    loopLen=len(loopSamples)
    totalLen=attackLen+loopLen
    if sampleWidth==1:
        sampleType="int8_t"
        attackSamples=[int(sample/256) for sample in attackSamples]
        loopSamples=[int(sample/256) for sample in loopSamples]
    elif sampleWidth==2:
        sampleType="int16_t"

    hFilePath=os.path.join(fileDir,"%s.h"%waveTableIdentifer)
    cFilePath=os.path.join(fileDir,"%s.c"%waveTableIdentifer)
    
    with open(hFilePath,'w') as hFile:
        hFile.write("#ifndef __%s__\n#define __%s__\n"%(str.upper(waveTableIdentifer),str.upper(waveTableIdentifer)))
        hFile.write("#include <stdint.h>\n")
        hFile.write("// Sample's base frequency: %f Hz\n"%freq)
        hFile.write("// Sample's sample rate: %d Hz\n"%32000)
        hFile.write("#define %s_LEN %d\n"%(str.upper(waveTableIdentifer),totalLen))
        hFile.write("#define %s_ATTACK_LEN %d\n"%(str.upper(waveTableIdentifer),attackLen))
        hFile.write("#define %s_LOOP_LEN %d\n"%(str.upper(waveTableIdentifer),loopLen))
        hFile.write("extern const %s %s[%s_LEN];\n"%(sampleType,waveTableIdentifer,str.upper(waveTableIdentifer)))
        hFile.write("extern const uint16_t %s_Increment[];\n"%waveTableIdentifer)
        hFile.write("#endif")

    with open(cFilePath,'w') as cFile:
        cFile.write("#include <stdint.h>\n#include <%s.h>\n"%waveTableIdentifer)
        cFile.write("// Sample's base frequency: %f Hz\n"%freq)
        cFile.write("// Sample's sample rate: %d Hz\n"%32000)
        cFile.write("const %s %s[%s_LEN]={\n"%(sampleType,waveTableIdentifer,str.upper(waveTableIdentifer)))
        newLineCounter=0
        cFile.write("// Attack Samples:\n")
        for sample in attackSamples:
            cFile.write("%6d,"%sample)
            if newLineCounter>8:
                newLineCounter=0
                cFile.write("\n")
            else:
                newLineCounter+=1
        cFile.write("\n// Loop Samples:\n")
        for sample in loopSamples:
            cFile.write("%6d,"%sample)
            if newLineCounter>8:
                newLineCounter=0
                cFile.write("\n")
            else:
                newLineCounter+=1        
        cFile.write("};\n\n")
        cFile.write("const uint16_t %s_Increment[]={\n"%waveTableIdentifer)
        newLineCounter=0
        for i in range(0,128):
            cFile.write("%6d,"%(calcIncrement(freq,noteToFreq(i))*255))
            if newLineCounter>8:
                newLineCounter=0
                cFile.write("\n")
            else:
                newLineCounter+=1        
        cFile.write("};\n")            



def main():
    sampleName="Celesta_C5"
    attackSamples=readWaveSamples("./WaveTableUtils/%s_ATTACK.wav"%sampleName)	
    loopSamples=readWaveSamples("./WaveTableUtils/%s_LOOP.wav"%sampleName)
    sampleFreq=estimateSampleFreq(attackSamples,32000)
    print("Estimated base frequency:%f Hz"%sampleFreq)
    exportToSourceFile(sampleName,attackSamples,loopSamples,sampleFreq,'./WaveTableUtils/',1)
    
if __name__ == "__main__":
	main()