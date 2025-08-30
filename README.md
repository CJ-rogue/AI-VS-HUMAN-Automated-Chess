# AI vs Human Automated Chess

This repository contains code and resources for an automated chess project, featuring both Arduino-based hardware control and Python-based chess logic and AI.

## Project Author & Purpose

This project was created by Christian Jhames De Los Reyes and Vincent Cerillo for their Project Design Final Project for the Degree in Computer Engineering.

## Documentation


[Watch on YouTube](https://youtube.com/shorts/smxDBfMesxg?feature=share)
![d7009e39-5257-4c57-8d4f-465d057ad33a](https://github.com/user-attachments/assets/3ad40425-9df7-495c-abe3-3cdd5b7b6525)
![492567480_685433063952398_1889414771577030598_n](https://github.com/user-attachments/assets/8cbedde7-0cef-45e8-8a37-dee5d2319c18)

## Folder Structure

### Arduino/

Contains microcontroller code for controlling chess hardware, such as motors and sensors.

- **arduino_code_with_serial/**  
  Arduino code that communicates via serial.  
  - `combination_55_com.ino`: Main sketch with serial communication.  
  - `global.h`: Global variables and configuration.

- **arduino_code_without_serial/**  
  Arduino code without serial communication.  
  - `combination_55.ino`: Main sketch.  
  - `global.h`: Global variables and configuration.

- **test_motor/**  
  Test code for motor control.  
  - `motor.ino`: Motor test sketch.

### Python/

Contains chess AI, game logic, and integration with Stockfish.

- **Chess v3/**  
  Main Python chess logic and AI.  
  - `chess_test.py`: Python script for testing chess logic.  
  - `best.pt`: Pre-trained model file (PyTorch).  
  - **stockfish/**: Stockfish chess engine and documentation.  
    - `stockfish-windows-x86-64-avx2.exe`: Stockfish engine binary.  
    - `src/`: Source code for Stockfish engine.  
    - `README.md`: Documentation for Stockfish.  
    - Other supporting files.

## Getting Started

1. **Arduino**:  
   - Open the `.ino` files in the Arduino IDE.
   - Upload to your Arduino board as needed.

2. **Python**:  
   - Install required Python packages (see `chess_test.py` for dependencies).
   - Ensure Stockfish binary is present in `Chess v3/stockfish/`.
   - Run `chess_test.py` to test chess logic.

## License

See [Python/Chess v3/stockfish/Copying.txt](Python/Chess%20v3/stockfish/Copying.txt) for Stockfish licensing.

## Credits

- Stockfish engine: [Python/Chess v3/stockfish/README.md](Python/Chess%20v3/stockfish/README.md)
- Arduino hardware code:
