# GPS Synchronization

### Tuning PI(D) controller
The proportional component should be as large as possible without causing oscillations. This is fairly easy to tune by disabling all the other components and plotting the error over time. However, I found that I was able to increase the proportional gain after including the integral component.

The integral part should have a range large enough to correct for the steady-state frequency-offset of the fine clock divider. The maximum value shouldn't be set much higher than this in order to minimize wind-up. A good way to find the steady-state offset required is to run the time sync with only the proportional component enabled. While in this mode, the error will converge to some nonzero value. The correction value applied when the error becomes stable is the value the integral component should assume at steady state.

To tune the integral gain, set it to ~1/100 the proportional gain and increase until oscillations occur, then back off by a value of 10 or so.