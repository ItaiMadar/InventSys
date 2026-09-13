# Five-LED Coupled Oscillator

ESP32/Arduino firmware for a Kuramoto-model demonstration. Three capacitive
electrodes recognize directional swipes, a fourth capacitive button chooses the
controlled parameter (flash frequency or coupling strength),
and five PWM LEDs visualize the oscillator phases.

## Controls

| Mode | Up swipe (`3 -> 2 -> 1`) | Down swipe (`1 -> 2 -> 3`) |
| --- | --- | --- |
| Frequency | Increase frequency | Decrease frequency |
| K | Increase coupling | Decrease coupling |

A faster swipe corresponds to a larger change:
$$
\\Delta = C / (1 + sqrt(swipeDuration),
$$
where C is a scaling parameter which could be unique to frequency or K.

The mode button toggles between frequency and K. Each touch electrode uses a derivative-
threshold detector.

## Files

| File | Responsibility |
| --- | --- |
| `CoupledOscillator.ino` | Direct swipe timing, mode toggle, and diagnostics |
| `Config.h` | All hardware and tuning values |
| `TouchButton.h` | Header-only capacitive press detector |
| `ParameterController.h` | Header-only mode and parameter adjustment logic |
| `KuramotoLedSystem.*` | Physics integration and rendering |

## Default pins

| Part | GPIO |
| --- | --- |
| Swipe electrode 1 | 27 |
| Swipe electrode 2 | 33 |
| Swipe electrode 3 | 32 |
| Mode button | 13 |
| LEDs 1-5 | 15, 2, 4, 16, 17 |

## Kuramoto Model

The Kuramoto model is perhaps the most widely used model which describes the dynamics of synchronization.
The simplest form of the model assumes each two oscillators are coupled with coupling constant $K$, and the dynamics are given by:
$$
\frac{d \theta_i}{dt} = \omega_i + \frac{K}{N} \sum_{j \neq i}^N \sin{(\theta_j - \theta_i)} \ ,
$$
where $N$ is the number of oscillators, and $\theta_i, \omega_i$ are the angle and the natural frequency of the $i$-th oscillator, respectively.

The order parameter is:
$$
r = \frac{1}{N} \left| \sum_j^N e^{i \theta_j} \right| \ ,
$$
which vanishes when every oscillator has a random phase ($N \to \infty$), and equals 1 when all oscillators are phase-locked.