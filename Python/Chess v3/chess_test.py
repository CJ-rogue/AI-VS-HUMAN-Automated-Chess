import cv2
import chess
import chess.engine
import torch
import random
import serial
import time
from ultralytics import YOLO

# Connect to Arduino Mega (Update COM port for Windows)
try:
    ser = serial.Serial('COM5', 9600, timeout=1)
    time.sleep(2)
    if ser.is_open:
        print("✅ Serial connection established.")
except serial.SerialException:
    print("❌ ERROR: Unable to connect to Arduino. Check COM port.")
    ser = None

# Load YOLO Model
model = YOLO("best.pt")  # Ensure best.pt is in the same directory

# Initialize Stockfish Engine
STOCKFISH_PATH = "stockfish/stockfish-windows-x86-64-avx2.exe"

# Open webcam
cap = cv2.VideoCapture(1)  # Change index if using external camera
cap.set(3, 1280)  # Set width
cap.set(4, 720)   # Set height

# Define chessboard grid (Adjust based on camera calibration)
BOARD_X_MIN, BOARD_X_MAX = 340, 940
BOARD_Y_MIN, BOARD_Y_MAX = 60, 660
SQUARE_SIZE_X = (BOARD_X_MAX - BOARD_X_MIN) / 8
SQUARE_SIZE_Y = (BOARD_Y_MAX - BOARD_Y_MIN) / 8


# Piece notation mappings
piece_short_names = {
    "black-pawn": "p", "black-knight": "n", "black-bishop": "b", "black-rook": "r",
    "black-queen": "q", "black-king": "k",
    "white-pawn": "P", "white-knight": "N", "white-bishop": "B", "white-rook": "R",
    "white-queen": "Q", "white-king": "K"
}


# Read data from Arduino and return it as a string
def read_arduino():
    if ser and ser.is_open:  # Check if Serial connection exists
        if ser.in_waiting > 0:
            return ser.readline().decode().strip()
    time.sleep(0.01)
    return None


# Send AI move to Arduino
def send_ai_move(move):
    if ser and ser.is_open:
        print(f"📡 Sending AI Move: {move}")
        ser.write(f"{move}\n".encode())
    else:
        print("❌ ERROR: Serial connection is closed. Cannot send AI move.")


# Convert bounding box center to chess notation
def get_chess_square(x, y):
    if x < BOARD_X_MIN or x > BOARD_X_MAX or y < BOARD_Y_MIN or y > BOARD_Y_MAX:
        return None  # Outside board

    # Flip column (X-axis)
    col = 7 - int((x - BOARD_X_MIN) / SQUARE_SIZE_X)
    # Flip row (Y-axis)
    row = int((y - BOARD_Y_MIN) / SQUARE_SIZE_Y) + 1

    if 0 <= col < 8 and 1 <= row <= 8:
        return f"{chr(97 + col)}{row}"  # 'a1' to 'h8'
    return None


# Draw chessboard grid
def draw_chess_grid(frame):
    for i in range(9):  # 8 squares + 1 for edges
        x = int(BOARD_X_MIN + i * SQUARE_SIZE_X)
        y = int(BOARD_Y_MIN + i * SQUARE_SIZE_Y)
        
        # Draw vertical lines (columns)
        cv2.line(frame, (x, BOARD_Y_MIN), (x, BOARD_Y_MAX), (0, 255, 255), 1)
        
        # Draw horizontal lines (rows)
        cv2.line(frame, (BOARD_X_MIN, y), (BOARD_X_MAX, y), (0, 255, 255), 1)

    return frame


########################################################################################    


# Game Status (Check, Checkmate, Stalemate, Draw Conditions)
def process_game_status(board, player1, player2):
    global game_started, previous_fen
    game_over = False
    message = ""

    # Checkmate
    if board.is_checkmate():
        message = f"CHECKMATE: {player1} Wins"
        print(f" 🎉 Game Status: Checkmate")
        print(f" 🏆 {player1} Wins by Checkmate!")
        game_over = True

    # Stalemate
    elif board.is_stalemate():
        message = "STALEMATE"
        print(f" 🤝 Game Status: Stalemate")
        print(f" 😐 The game is drawn due to Stalemate.")
        game_over = True

    # Draw by Insufficient Material
    elif board.is_insufficient_material():
        message = "DRAW INSUFFICIENT MATERIAL"
        print(f" 🤝 Game Status: Draw by Insufficient Material")
        print(f" 😐 Neither player has enough material to checkmate.")
        game_over = True

    # Draw by Threefold Repetition
    elif board.is_repetition(3):
        message = "DRAW REPETITION"
        print(f" 🤝 Game Status: Draw by Threefold Repetition")
        print(f" 🔄 The same position has repeated three times.")
        game_over = True

    # Draw by 50-Move Rule
    elif board.halfmove_clock >= 100:  # 50-move rule (100 half-moves = 50 full moves)
        message = "DRAW 50 MOVE RULE"
        print(f" 🤝 Game Status: Draw by 50-Move Rule")
        print(f" ⏳ No captures or pawn moves in 50 moves.")
        game_over = True

    # Check (Not a game-ending event)
    elif board.is_check():
        message = f"{player2} is CHECK"
        print(f" ⚠️ {player2} is in Check!")

    # If No Special Status, Send a Default Message
    if not message:
        message = "GAME_CONTINUES"

    # Send status to Arduino
    if ser and ser.is_open:
        ser.write(f"{message}\n".encode())
        print(f"📡 Sent to Arduino: {message}")
    else:
        print("❌ ERROR: Serial connection is closed. Cannot send status.")

    # Reset game if it's over but keep detection running
    if game_over:
        game_started = False  # Allow restarting
        previous_fen = initial_fen  # Reset position
        board.reset()  # Reset board
        print("🔄 Game over. Press Start Button for a new game.")
        return False  # Game ended

    return True  # Game continues


########################################################################################    


# Convert detected board state to FEN notation
def board_state_to_fen(board_state, board):
    """Update the board's FEN while preserving game state."""
    
    new_board = board.copy()  # Keep castling, en passant, turn tracking
    new_board.clear_board()  # Remove all pieces first

    for square, piece in board_state.items():
        rank = int(square[1]) - 1
        file = ord(square[0]) - ord('a')
        new_board.set_piece_at(rank * 8 + file, chess.Piece.from_symbol(piece))

    return new_board.fen()  # Stockfish handles castling & en passant correctly


########################################################################################    


def validate_move_with_stockfish(fen, move_uci):
    """Send FEN and move to Stockfish to check legality."""
    with chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH) as engine:
        board = chess.Board(fen)  # Load board state
        legal_moves = [m.uci() for m in board.legal_moves]  # Get legal moves
        
        print(f"🧾 Stockfish Legal Moves: {legal_moves}")  # Debug all legal moves

        return move_uci in legal_moves  # Return True if move is legal


########################################################################################    


def detect_human_move(old_board, new_board):
    """Detects the human move by finding one White piece that moved and optionally one Black piece that was removed."""

    moved_from, moved_to = None, None
    moved_from_2, moved_to_2 = None, None # for castling validation
    removed_black = None  # Track removed Black piece
    white_moved_count = 0  # Count moved White pieces
    black_removed_count = 0  # Count removed Black pieces
    black_moved_count = 0  # Count Black pieces that changed squares

    for square in chess.SQUARES:
        old_piece = old_board.piece_at(square)
        new_piece = new_board.piece_at(square)

        # ✅ Find White piece that moved (previously here but now gone)
        if old_piece and old_piece.color == chess.WHITE and not new_piece:
            white_moved_count += 1
            if moved_from is None:
                moved_from = square
            elif moved_from_2 is None:
               moved_from_2 = square
            else:
                print("❌ Multiple White pieces moved! Invalid turn.")
                return None

        # ✅ Detect where a White piece moved to (Capture Move)
        if new_piece and new_piece.color == chess.WHITE and old_piece and old_piece.color == chess.BLACK:
            if moved_to is None:
                moved_to = square  # ✅ Assign move destination (Capture Move)
                removed_black = square  # ✅ Also assign removed Black piece
                black_removed_count += 1
            else:
                print("❌ Multiple White pieces detected in new locations! Invalid turn.")
                return None

        # ✅ Detect where a White piece moved to (Normal Move)
        if new_piece and new_piece.color == chess.WHITE and not old_piece:
            if moved_to is None:
                moved_to = square  # ✅ Assign move destination (Normal Move)
            elif moved_to_2 is None:
                moved_to_2 = square  
            else:
                print("❌ Multiple White pieces detected in new locations! Invalid turn.")
                return None

        # ❌ If a Black piece moved (not just removed), it's invalid
        if new_piece and new_piece.color == chess.BLACK and not old_piece:
            black_moved_count += 1
            print("❌ A Black piece appeared in a new square! Invalid turn.")
            return None

    print(f"🔹 Move From: {chess.square_name(moved_from) if moved_from else 'None'}")
    print(f"🔹 Move To: {chess.square_name(moved_to) if moved_to else 'None'}")
    print(f"🔹 Removed Black Piece: {chess.square_name(removed_black) if removed_black else 'None'}")

    # ✅ Separate Castling Check (Runs after normal move validation)
    if white_moved_count == 2 and black_removed_count == 0:
        # ✅ Check if this is a castling move (White's king & rook moved correctly)
        castling_moves = {
            "O-O": (chess.E1, chess.G1, chess.H1, chess.F1),  # Kingside castling
            "O-O-O": (chess.E1, chess.C1, chess.A1, chess.D1)  # Queenside castling
        }

        for castling_type, (king_from, king_to, rook_from, rook_to) in castling_moves.items():
            if {moved_from, moved_from_2} == {king_from, rook_from} and {moved_to, moved_to_2} == {king_to, rook_to}:
                castling_move = chess.Move(king_from, king_to)
                print(f"✅ Castling Move Detected: {castling_type}")

                # ✅ Validate move with Stockfish
                if validate_move_with_stockfish(old_board.fen(), castling_move.uci()):
                    return castling_move
                else:
                    print(f"❌ Castling Move {castling_move.uci()} is not legal!")
                    return None

    # ❌ If more than one White piece moved or any Black piece moved
    if white_moved_count > 1 or black_moved_count > 0:
        print("❌ Invalid move: Multiple White pieces moved or Black moved.")
        return None

    # ❌ Handle edge case where moved_from exists but moved_to is still None
    if moved_from and not moved_to:
        print("❌ Move destination not detected! Invalid move.")
        return None

    # ❌ Invalid Case: No White moved, but a Black piece was removed
    if white_moved_count == 0 and black_removed_count == 1:
        print("❌ A Black piece was removed but no White piece moved! Invalid turn.")
        return None  

    # ✅ Normal Move: If one White piece moved and no Black piece was removed
    if white_moved_count == 1 and black_removed_count == 0:
        # ✅ Check if this is a Pawn Promotion move
        if old_board.piece_at(moved_from) and old_board.piece_at(moved_from).piece_type == chess.PAWN:
            if chess.square_rank(moved_to) == 7:  # White Pawn reaching 8th rank
                print("♛ Pawn Promotion Detected! Promoting to Queen.")
                move = chess.Move(moved_from, moved_to, promotion=chess.QUEEN)  # Auto-promote to Queen
            else:
                move = chess.Move(moved_from, moved_to)  # Regular pawn move
        else:
            move = chess.Move(moved_from, moved_to)  # Non-pawn moves

        move_uci = move.uci()
        print(f"✅ Normal Move Detected: {move_uci}")

        # ✅ Validate move with Stockfish
        if validate_move_with_stockfish(old_board.fen(), move_uci):
            return move
        else:
            print(f"❌ Move {move_uci} is not legal according to Stockfish!")
            return None


    # ✅ Capture Move: If one White piece moved and exactly one Black piece was removed
    if white_moved_count == 1 and black_removed_count == 1:
        # ✅ Check if this is a Pawn Promotion move
        if old_board.piece_at(moved_from) and old_board.piece_at(moved_from).piece_type == chess.PAWN:
            if chess.square_rank(moved_to) == 7:  # White Pawn reaching 8th rank
                print("♛ Pawn Promotion Detected! Promoting to Queen.")
                move = chess.Move(moved_from, moved_to, promotion=chess.QUEEN)  # Auto-promote to Queen
            else:
                move = chess.Move(moved_from, moved_to)  # Regular pawn move
        else:
            move = chess.Move(moved_from, moved_to)  # Non-pawn moves

        move_uci = move.uci()
        print(f"✅ Capture Move Detected: {move_uci} (Captured {chess.square_name(removed_black)})")

        # ✅ Validate move with Stockfish
        if validate_move_with_stockfish(old_board.fen(), move_uci):
            return move
        else:
            print(f"❌ Move {move_uci} is not legal according to Stockfish!")
            return None

    print("❌ No valid White move detected!")
    return None  # Invalid move


########################################################################################    


# Function to get AI move while preventing special moves
def get_ai_move(board, difficulty):
    time_limit = 0.5 if difficulty == "easy" else 1.5  
    options = {"UCI_LimitStrength": True, "UCI_Elo": 1320 if difficulty == "easy" else 2000}

    # ✅ Ensure AI does not have castling rights
    fen_parts = board.fen().split()
    fen_parts[2] = fen_parts[2].replace("k", "").replace("q", "")  # Remove kq (Black's castling rights)
    fixed_fen = " ".join(fen_parts)
    board.set_fen(fixed_fen)  # ✅ Apply updated FEN

    result = engine.play(board, chess.engine.Limit(time=time_limit), options=options)
    ai_move = result.move

    # ✅ Introduce 75% blunder chance in easy mode
    if difficulty == "easy" and random.random() < 0.75:
        legal_moves = list(board.legal_moves)  # Get all legal moves
        if len(legal_moves) > 1 and not board.is_check():  # ✅ Ensure not in check
            blunder_move = random.choice(legal_moves)  # Pick a random move
            ai_move = blunder_move  # Replace AI move with a blunder

    # ✅ Force AI promotion to be only to a Queen
    if board.piece_at(ai_move.from_square) and board.piece_at(ai_move.from_square).piece_type == chess.PAWN:
        if chess.square_rank(ai_move.to_square) in [0, 7]:  # If AI pawn reaches last rank
            print("♛ AI Pawn Promotion Detected! Forcing Queen Promotion.")
            ai_move = chess.Move(ai_move.from_square, ai_move.to_square, promotion=chess.QUEEN)

    # **Ensure AI move is valid (retry if illegal)**
    while board.is_castling(ai_move) or board.is_en_passant(ai_move):
        print("⚠ AI generated a special move (castling/en passant), recalculating...")
        result = engine.play(board, chess.engine.Limit(time=time_limit), options=options)
        ai_move = result.move

    return ai_move


########################################################################################    


# Initialize Stockfish Engine (Ensure the path is correct)
engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)

board = chess.Board()  # tracks all moves properly
initial_fen = board.fen()  # Store correct default FEN
previous_fen = initial_fen  # Set previous FEN to the starting position
game_started = False
difficulty = None  # "easy" or "hard"

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        print("Failed to grab frame")
        break

    # Run YOLO inference
    results = model(frame, verbose=False, conf=0.4)  # Suppress unnecessary logs

    # Detected board state
    board_state = {}

    for r in results:
        for box in r.boxes:
            x1, y1, x2, y2 = box.xyxy[0]  
            class_id = int(box.cls[0])  
            piece_label = model.names[class_id]  
            piece_short = piece_short_names.get(piece_label, piece_label)

            center_x = (x1 + x2) / 2
            center_y = (y1 + y2) / 2
            square = get_chess_square(center_x, center_y)

            if square:
                board_state[square] = piece_short

            # Draw bounding box
            cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 0), 2)
            cv2.putText(frame, f"{piece_short}-{square}", (int(x1), int(y1) - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)

    # Draw the chessboard grid
    frame = draw_chess_grid(frame)

    # Display camera with detected chessboard
    cv2.imshow("YOLOv8 Chess Detection", frame)

    # 'q' to exit
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        print("❌ Exiting program...")
        break

    # 'r' to reset the board
    if key == ord('r'):
        print("🔄 Resetting the board...")
        game_started = False
        previous_fen = initial_fen
        board.reset()
        print("🔄 Game Ended. Press Start Button for a new game.")

    # ✅ Read Serial Data from Arduino
    if ser:
        arduino_data = read_arduino()
        if arduino_data:
            print(f"📡 Arduino Sent: {arduino_data}")  # Debugging

            if arduino_data == "START":
                detected_fen = board_state_to_fen(board_state, board)
                print("🔎 Detected FEN:", detected_fen)

                if detected_fen == initial_fen:
                    print("✅ Board is in the correct initial position. Ready to play!")
                    game_started = True
                    ser.write(b"START_OK\n")  # ✅ Send confirmation to Arduino
                else:
                    print("⚠️ Incorrect board setup! Please adjust and try again.")
                    ser.write(b"START_ERROR\n")  # ✅ Send error message to Arduino

            elif arduino_data == "EASY":
                if game_started:
                    difficulty = "easy"
                    print("🎯 Difficulty set to: EASY")

            elif arduino_data == "HARD":
                if game_started:
                    difficulty = "hard"
                    print("🔥 Difficulty set to: HARD")

            elif arduino_data == "END":
                game_started = False
                previous_fen = initial_fen
                board.reset()
                print("🔄 Game Ended. Press Start Button for a new game.")

            elif arduino_data == "MOVE_CONFIRM":
                if game_started and difficulty:
                    fen = board_state_to_fen(board_state, board)  # Convert detected board to FEN

                    if fen != previous_fen:  # Only proceed if board has changed
                        print("🔎 New board state detected!")
                        print("FEN:", fen)

                        previous_board = chess.Board(previous_fen)
                        current_board = chess.Board(fen)

                        # ✅ Validate human move
                        human_move = detect_human_move(previous_board, current_board)

                        if human_move is None:
                            if previous_board.is_check():
                                # Human was in check but made invalid move
                                legal_moves = [m.uci() for m in previous_board.legal_moves]
                                moves_str = " ".join(legal_moves)
                                print(f"⚠️ Invalid Move While In Check! Legal moves: {moves_str}")
                                ser.write(f"MOVE_ERROR_CHECK:{moves_str}\n".encode())
                            else:
                                print("❌ No valid White move detected! Please try again.")
                                ser.write(b"MOVE_ERROR\n")
                        elif current_board.turn == chess.BLACK:  # White should be moving
                            print("❌ Black (AI) moved out of turn! Invalid board setup.")
                            ser.write(b"MOVE_ERROR\n")
                        else:
                            print(f"✅ White Move Detected: {human_move.uci()}")
                            ser.write(b"MOVE_OK\n")
                            board.push(human_move)  # Apply human move

                            # ✅ Check game status after Human Move
                            while True:
                                arduino_response = read_arduino()
                                if arduino_response == "ASK_HUMAN_STATUS":
                                    break                 
                            if not process_game_status(board, "Human", "AI"):
                                continue  # Skip AI move if the game ended

                            # ✅ AI's Turn
                            while True:
                                arduino_response = read_arduino()
                                if arduino_response == "ASK_AI_MOVE":
                                    break
                            ai_move = get_ai_move(board, difficulty)
                            is_capture = 0 if board.is_capture(ai_move) else 1  
                            board.push(ai_move)  # Apply AI move
                            print(board)
                            print(f"🤖 AI Move (Black): {ai_move.uci()}:{is_capture}")

                            # Handle Pawn Promotion (Remove last character if promotion occurs)
                            move_str = ai_move.uci()
                            if len(move_str) == 5:
                                move_str = move_str[:4]

                            # Send AI move to Arduino
                            send_ai_move(f"{move_str}:{is_capture}")

                            # ✅ Check game status after AI Move
                            while True:
                                arduino_response = read_arduino()
                                if arduino_response == "ASK_AI_STATUS":
                                    break
                            process_game_status(board, "AI", "Human")

                            # Store board state for next turn
                            previous_fen = board.fen()
                    else:
                        print("♟ No changes detected in board state.")
                        ser.write(b"MOVE_NO_CHANGE\n")

# Release resources
cap.release()
cv2.destroyAllWindows()
engine.quit()
