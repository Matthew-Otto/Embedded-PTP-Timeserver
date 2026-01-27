# Clocks

The MB1404 board comes equipped with an external 25MHz crystal (labeled X3) that can act as a much more stable frequency source than the 8 MHz generated clock produced by STLINK-V3EC.

To use this crystal, solder bridges must be added to SB3 and SB4 and the solder bridge at SB49 must be removed.
The HSE input must then be enabled to drive PLL1 from the external crystal.

### Configuring System Clock

To configure the system clock with a frequency of 250MHz assuming a 25 MHz source signal (HSE), configure the PPL parameters like so:

```
M = 1   (pre-div1)
N = 20  (mult)
P = 2   (post-div)
Q = 2   (post-div)
R = 2   (post-div)
```

Complete initialization code can be found in `src/hw/clocks.c/init_sysclk()`