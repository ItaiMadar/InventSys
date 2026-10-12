> [!IMPORTANT]
> The majority of the code for this project was generated using ChatGPT, with clear implementation instructions provided by myself.

[Lab 3 Demo](https://youtu.be/)

# Reactive Vibration Chess Companion

An ESP32 controller is connected to a computer which allows the user to play chess and experience real-time haptic feedback.  The player plays chess against the Stockfish chess engine, while the same engine analyzes the state of the board to determine the optimal next possible moves of the player.

The player experiences haptic feedback from the vibration motor attached to the controller; a slow\weak vibration corresponds to a sup-optimal piece or move, while a fast\strong vibration corresponds to a better one.
The user recieves this feedback by hovering over their available pieces, or once a pieces is selected, by hovering over an available move. A piece is scored according to the value of its best possible move.

For demonstration purposes, a series of LED lights acts a meter filled according to the vibration strength, which also flashes relative to the vibration frequency.

## Setup Instructions
Install the Python packages listed in requirements.txt, as well as the latest Stockfish release from the [official Stockfish website](https://stockfishchess.org/download/).  Then, compile and upload "VibrationController.ino" to the ESP32 controller using Arduino IDE.  Finally, close Arduino IDE and run the command line: "~\Lab3_Haptics> python .\gui.py".

> [!WARNING]
> The port detection and interface with the Stockfish distribution were implemented in Windows.  If using MacOS/Linux, your experience may vary.

## Controls
After launching the GUI, select the port to which the ESP32 controller is connected and press "Connect".  Adjust the game settings as desired, then press "New game / Apply".  The haptic feedback settings are also adjustable, however it is recommended to stay with the preconfigured values; if adjusted, press "New game / Apply" to enact the changes.

You are now ready to play!

## Files

| File | Responsibility |
| --- | --- |
| `VibrationController/VibrationController.ino` | Programs the vibration motor and LEDs. |
| `engine.py` | Finds Stockfish, validates positions, evaluates legal moves, and chooses opponent moves. |
| `haptics.py` | Converts engine evaluations into piece and move vibration signals. |
| `gui.py` | Provides the playable chessboard, settings, hover interaction, and game loop. |
| `serial_transport.py` | Detects serial ports and sends vibration commands to the ESP32. |

## Default Pins

| Function | GPIO |
| --- | ---: |
| Motor enable / PWM | 13 |
| Direction 1 | 12 |
| Direction 2 | 14 |
| LED meter 20 - 100% | 17, 16, 4, 2, 15 |