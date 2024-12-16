import numpy as np
import matplotlib.pyplot as plt

taps = np.zeros(3)
phase = np.zeros([100,11])
dphase = np.zeros([99,11])
for wid in range(11):
    for cnt in range(100):
        test = np.zeros(200)
        test[cnt:cnt+32+wid-5] = 1
        taps[0] = np.sum(test[0:32])+np.sum(test[99:131])
        taps[1] = np.sum(test[33:65])+np.sum(test[132:164])
        taps[2] = np.sum(test[66:98])+np.sum(test[165:197])
        mintap = np.argmin(taps)
        if taps[np.mod(mintap+1,3)] == 0:
            mintap = np.mod(mintap+1,3)
        C = taps[mintap]
        A = taps[np.mod(mintap+1,3)]
        B = taps[np.mod(mintap+2,3)]
        phase[cnt,wid]=((B-C)/(A+B-2*C))/3 + np.mod(mintap+1,3)/3
    dphase[:,wid] = phase[1:100,wid]-phase[0:99,wid]
plt.plot(phase)
plt.show()



