> [!IMPORTANT]
> The majority of the code for this project was generated using ChatGPT, with clear implementation instructions provided by myself.

[Lab 2 Demo](https://youtu.be/JBj5X3lEuSA)

# Guitar Tuner

ESP32 firmware and a laptop as a (somewhat) functional guitar tuner. The laptop picks up audio signal, and analyzes it using Fast-Fourier-Transform (FFT) to find the dominant frequency. Seven LED lights act as a feedback interface to help the user tune their guitar; similarly to a tubular spirit level. Upon successful tuning of the guitar, a playful melody of Schubert's "Die Forelle" (or "The Trout") plays using a passive buzzer connected to the arduino.

## Controls

The tuner is initialized by using the command line: "python tune_guitar.py", with optional arguments:
- --epsilon FLOAT (tolerance of the tuner, default 5.0)
- snr-db FLOAT (sensitivity of the tuner, default 6.0)

If the script is initialized without the arguemnt "--no-upload", it will compile the Arduino code "arduinoController/arduinoController.ino" and load it onto the ESP32 controller (autodetected).  All hardcoded pin IO# appear in this file.

Other important hardcoded variables appear in "tuner.py", which control the audio sample-rate and duration, FFT resolution, and a rolling window which corresponds to the tuner's responsiveness and smoothness.  Additionally, the target frequencies (standard guitar tuning) are hardcoded in the same file.

Since this is implemented entirely in Python, it relies on various packages. These are listen in "requirements.txt".

By calling "python tune_guitar.py" in the command line, the tuner is ready to go!

> [!WARNING]
> The automatic detection and interface with the microphone and ESP32 controller was implemented in Windows, assuming default arduino-cli install location. If using MacOS/Linux, your experience may vary!
> 
> **If this is the case, compile and upload "arduinoController.ino" manually using Arduino IDE. Close the IDE, then run: "python3 -m tune_guitar --no_upload --device {} --port {}" and input the audio capture device name or index, and ESP32 port.**
> > [!NOTE]
> > The audio capture device can be found using "python3 tune_guitar.py --list-devices". The port can be found via: "python -m serial.tools.list_ports".

## Files

| File | Responsibility |
| --- | --- |
| `arduinoController/arduinoController.ino` | Interfacing with the LEDs and buzzer. |
| `controller.py` | ESP32/Arduino detection, firmware upload, and buzzer communication. |
| `light_renderer.py` | Logic for rendering tuning offset as LED state. |
| `midi_player.py` | Parses the .midi file, communicates signal to controller. |
| `tuner.py` | Signal analysis and tuning logic. |
| `tune_guitar.py` | Wrapper which runs the guitar tuner script. |

## Default Pins

| Part | GPIO |
| --- | --- |
| Passive Buzzer | 14 |
| LEDs 1–7 | 15, 2, 4, 16, 17, 5, 18 |
