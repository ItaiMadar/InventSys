> [!IMPORTANT]
> The majority of the code for this project was generated using ChatGPT, with clear implementation instructions provided by myself.

# Five-LED Coupled Oscillator

ESP32/Arduino firmware for a Kuramoto-model demonstration. Three capacitive
electrodes recognize directional swipes, a fourth capacitive button selects the
controlled parameter—flash frequency or coupling strength—and five PWM LEDs
visualize the oscillator phases.

## Controls

| Mode | Up swipe (`3 → 2 → 1`) | Down swipe (`1 → 2 → 3`) |
| --- | --- | --- |
| Frequency | Increase frequency | Decrease frequency |
| Coupling strength \(K\) | Increase coupling | Decrease coupling |

A faster swipe produces a larger parameter change:

$$
\Delta = \frac{C}{1+\sqrt{T}},
$$

where \(T\) is the swipe duration and \(C\) is a scaling parameter. Separate
values of \(C\) may be used for frequency and coupling strength.

The mode button toggles between frequency and coupling control. Each touch
electrode uses a derivative-threshold detector.

## Files

| File | Responsibility |
| --- | --- |
| `CoupledOscillator.ino` | Direct swipe timing, mode selection, and diagnostics |
| `Config.h` | Hardware assignments and tuning parameters |
| `TouchButton.h` | Header-only capacitive-press detector |
| `ParameterController.h` | Header-only mode and parameter-adjustment logic |
| `KuramotoLedSystem.h` and `.cpp` | Physics integration and LED rendering |

## Default Pins

| Part | GPIO |
| --- | --- |
| Swipe electrode 1 | 27 |
| Swipe electrode 2 | 33 |
| Swipe electrode 3 | 32 |
| Mode button | 13 |
| LEDs 1–5 | 15, 2, 4, 16, 17 |

## Kuramoto Model

The Kuramoto model is one of the most widely used models for describing the
dynamics of synchronization. Its simplest form assumes that every pair of
oscillators is coupled with coupling strength $K$. The phase dynamics are

$$\frac{d\theta_i}{dt} = \omega_i + \frac{K}{N}\sum_{j\neq i}^{N}\sin\left(\theta_j-\theta_i\right).$$

Here, $N$ is the number of oscillators, and $\theta_i$, $\omega_i$ are the phase and natural frequency of the
$i$-th oscillator, respectively.

The Kuramoto order parameter is

$$r = \frac{1}{N}\left|\sum_{j=1}^{N}e^{i\theta_j}\right|.$$

The order parameter satisfies

$$0 \leq r \leq 1.$$

For randomly distributed phases, $r$ approaches zero as
$N\rightarrow\infty$. When all oscillators are perfectly phase-aligned,
$r=1$.

For an intuitive video: https://www.youtube.com/watch?v=T58lGKREubo
