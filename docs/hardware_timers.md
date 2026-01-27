# Hardware Timers


### Hardware PTP timer architecture

Two timers:
* second counter (32 bits)
* subsecond counter (31 bits)

The second register increments every time the subsecond register rolls over (@ 0x7FFF_FFFF)\
The subsecond register is incremented by the value in MACSSIR.\
The value in MACSSIR determines the maximum possible precision.\
i.e.: for a theoretical precision of 10ns, the subsecond register should be incremented at 100MHz.\
In this case, the value of MACSSIR should be: 0x7FFFFFFF / 100000000 = ~21

To adjust for clock skew, the subsecond register is incremented using a high precision clock divider/multiplier implemented as another 32-bit timer.\
This timer increments the subsecond register when it rolls over. The value added to this timer at every cycle is contained in the MACTSAR register. \
The proper value for MACTSAR can be calculated with: 0xFFFFFFFF / (clock_speed / subsec_incr_freq)
i.e.: If the subsecond register is configured to increment at 100MHz and the system clock operates at 250MHz, MACTSAR should be set to 0xFFFFFFFF / (250M / 100M) = 1717986918.\
This register should be updated using the true value of the system clock as it drifts away from 250MHz.


### Configuring PTP timer snapshots

The values in both PTP timers can be captured in the same cycle using hardware snapshots, but these snapshots cannot be triggered directly. Instead, the signal must pass through an event chain: \
External Signal → GPIO → TIM3 Input Capture → TIM3 TRGO → ETH Auxiliary Snapshot.


Configure GPIO (pin D2) to trigger event on timer 3:
``` c src/apps/gps.c/gps_init()
configure_pin(GPIOD, GPIO_PIN_2, GPIO_MODE_AF_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF2_TIM3);
```

Configure timer 3 to immediately fire TRGO upon receiving an input event:\
``` c title="src/apps/gps.c/gps_init()"
// enable TIM3 peripheral clock
SET_BIT(RCC->APB1LENR, RCC_APB1LENR_TIM3EN);
(void)READ_BIT(RCC->APB1LENR, RCC_APB1LENR_TIM3EN);

// Enable one pulse mode
SET_BIT(TIM3->CR1, TIM_CR1_OPM);

// Set master mode to reset
MODIFY_REG(TIM3->CR2, TIM_CR2_MMS_Msk, 0);
// Set slave mode to reset
MODIFY_REG(TIM3->SMCR, TIM_SMCR_SMS_Msk, 0b100 << TIM_SMCR_SMS_Pos);
// Set trigger selection to external trigger input (tim_etrf)
MODIFY_REG(TIM3->SMCR, TIM_SMCR_TS_Msk, 0b111 << TIM_SMCR_TS_Pos);

// enable timer
SET_BIT(TIM3->CR1, TIM_CR1_CEN);

// enable timer interrupt
WRITE_REG(TIM3->SR, 0); // clear any interrupts
SET_BIT(TIM3->DIER, TIM_DIER_UIE); // enable TIM3 update interrupt
```

Configure Ethernet MAC to timestamp PTP timer upon receiving TRGO from timer 3:

``` c /src/hw/ethernet.c/ETH_PTP_init()
// Enable auxiliary snapshots
// ATSEN1 = TIM3 TRGO
WRITE_REG(ETH->MACACR, ETH_MACACR_ATSEN1);
```


### Experimental Verification

The MAC can output a PPS signal used to compare the synchronization between two devices. This function ETH_PPS_OUT can be assigned to pin PB5 or PG8

``` c
// configure PPS pin PG8
configure_pin(GPIOG, GPIO_PIN_8, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF11_ETH);

// Exceeding target time (0 unless set) triggers PPS output
MODIFY_REG(ETH->MACPPSCR, ETH_MACPPSCR_TRGTMODSEL0_Msk, 0x3 << ETH_MACPPSCR_TRGTMODSEL0_Pos);
// Freq of PPS
MODIFY_REG(ETH->MACPPSCR, ETH_MACPPSCR_PPSCTRL_Msk, 0x1 << ETH_MACPPSCR_PPSCTRL_Pos);
```