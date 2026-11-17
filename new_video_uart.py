# AI assistance disclosure:
# Parts of this file were developed with assistance from ChatGPT.

import cv2
import numpy as np
import serial
import threading
import time


# ==============================================================================
# CONFIGURATION
# ==============================================================================

# UART
COM_PORT = "COM3"
BAUD_RATE = 9600
TX_INTERVAL = 0.020          # 20 Hz maximum
COORD_MAX = 254              # 0xFF reserved as packet header

# Camera
CAMERA_INDEX = 1

# White-mask parameters
DEFAULT_BRIGHTNESS = 224
DEFAULT_MIN_S = 0
DEFAULT_MAX_S = 21

# Expected ball size
BALL_MIN_AREA_PERCENT = 0.2
BALL_MAX_AREA_PERCENT = 0.8


# ==============================================================================
# 1. UART CONNECTION
# ==============================================================================

def connect_uart():
    """Open the UART connection."""

    try:
        ser = serial.Serial(
            COM_PORT,
            BAUD_RATE,
            timeout=0.1
        )

        print(f"[+] UART connected to {COM_PORT}")

        return ser

    except serial.SerialException as error:
        print(f"[-] UART connection failed: {error}")
        return None


def uart_reader(ser):
    """Continuously print messages received from the embedded node."""

    while ser.is_open:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode(
                    "utf-8",
                    errors="replace"
                )

                print(
                    f"[Node 1 RX] {line}",
                    end="",
                    flush=True
                )

        except Exception:
            break


def start_uart_reader(ser):
    """Start UART reception in a separate thread."""

    thread = threading.Thread(
        target=uart_reader,
        args=(ser,),
        daemon=True
    )

    thread.start()


# ==============================================================================
# 2. CAMERA CONNECTION
# ==============================================================================

def open_camera():
    """Open the camera."""
    for i in range(5):
        print("camera test",i)
        cap = cv2.VideoCapture(
            i,
            cv2.CAP_DSHOW
        )
        if cap.isOpened():
            break
        

    if not cap.isOpened():
        print("[-] Could not open camera")
        return None

    print("[+] Camera opened")

    return cap


# ==============================================================================
# 3. USER CONTROLS
# ==============================================================================

def setup_windows():
    """Create camera, heatmap and control windows."""

    cv2.namedWindow(
        "Controls",
        cv2.WINDOW_NORMAL
    )

    cv2.resizeWindow(
        "Controls",
        600,
        450
    )

    cv2.namedWindow(
        "Camera",
        cv2.WINDOW_NORMAL
    )

    cv2.namedWindow(
        "White Detection Heatmap",
        cv2.WINDOW_NORMAL
    )

    cv2.createTrackbar(
        "Brightness",
        "Controls",
        DEFAULT_BRIGHTNESS,
        255,
        lambda x: None
    )

    cv2.createTrackbar(
        "Min S",
        "Controls",
        DEFAULT_MIN_S,
        255,
        lambda x: None
    )

    cv2.createTrackbar(
        "Max S",
        "Controls",
        DEFAULT_MAX_S,
        255,
        lambda x: None
    )


def get_control_values():
    """Read current HSV thresholds from the sliders."""

    brightness = cv2.getTrackbarPos(
        "Brightness",
        "Controls"
    )

    min_saturation = cv2.getTrackbarPos(
        "Min S",
        "Controls"
    )

    max_saturation = cv2.getTrackbarPos(
        "Max S",
        "Controls"
    )

    return brightness, min_saturation, max_saturation


# ==============================================================================
# 4. CAMERA FRAME PROCESSING
# ==============================================================================

def process_frame(frame, brightness, min_saturation, max_saturation):
    """
    Process one camera frame.

    Returns:
        clean_mask
    """

    height, width = frame.shape[:2]

    # Convert image to HSV
    hsv = cv2.cvtColor(
        frame,
        cv2.COLOR_BGR2HSV
    )

    # Invalid saturation range
    if min_saturation > max_saturation:
        return np.zeros(
            (height, width),
            dtype=np.uint8
        )

    lower_white = np.array(
        [
            0,
            min_saturation,
            brightness
        ],
        dtype=np.uint8
    )

    upper_white = np.array(
        [
            179,
            max_saturation,
            255
        ],
        dtype=np.uint8
    )

    # Create initial white-object mask
    mask = cv2.inRange(
        hsv,
        lower_white,
        upper_white
    )

    # Remove noise / fill small gaps
    kernel = cv2.getStructuringElement(
        cv2.MORPH_ELLIPSE,
        (5, 5)
    )

    clean_mask = cv2.morphologyEx(
        mask,
        cv2.MORPH_CLOSE,
        kernel,
        iterations=2
    )

    clean_mask = cv2.morphologyEx(
        clean_mask,
        cv2.MORPH_OPEN,
        kernel,
        iterations=1
    )

    return clean_mask


# ==============================================================================
# 5. BALL DETECTION
# ==============================================================================

def find_ball(clean_mask, frame_width, frame_height):
    """
    Search the processed mask for an object with the expected ball size.

    Returns:
        ball_contour
        ball_area_percent
        number_of_contours
        number_of_candidates
    """

    frame_area = frame_width * frame_height

    contours, _ = cv2.findContours(
        clean_mask,
        cv2.RETR_EXTERNAL,
        cv2.CHAIN_APPROX_SIMPLE
    )

    candidates = []

    for contour in contours:

        area = cv2.contourArea(contour)

        area_percent = (
            100.0 * area / frame_area
        )

        if (
            BALL_MIN_AREA_PERCENT
            <= area_percent
            <= BALL_MAX_AREA_PERCENT
        ):
            candidates.append(
                (
                    contour,
                    area_percent
                )
            )

    # No possible ball found
    if not candidates:
        return None, 0.0, len(contours), 0

    # Expected ball size is approximately the middle
    # of the allowed range
    target_area = (
        BALL_MIN_AREA_PERCENT
        + BALL_MAX_AREA_PERCENT
    ) / 2.0

    # Select candidate closest to expected size
    ball_contour, ball_area_percent = min(
        candidates,
        key=lambda candidate:
        abs(candidate[1] - target_area)
    )

    return (
        ball_contour,
        ball_area_percent,
        len(contours),
        len(candidates)
    )


# ==============================================================================
# 6. BALL COORDINATES
# ==============================================================================

def get_ball_coordinates(ball_contour):
    """
    Return the centre of the detected ball.

    Returns:
        x, y

    If no valid ball exists:
        None, None
    """

    if ball_contour is None:
        return -1, -1

    moments = cv2.moments(ball_contour)

    if moments["m00"] == 0:
        return -1, -1

    x = int(
        moments["m10"] / moments["m00"]
    )

    y = int(
        moments["m01"] / moments["m00"]
    )

    return x, y


# ==============================================================================
# 7. COORDINATE CONVERSION
# ==============================================================================

def scale_coordinate(value, maximum):
    """
    Convert camera pixel coordinate to UART range 0...254.
    """

    if maximum <= 0:
        return 0

    scaled = round(
        value * COORD_MAX / maximum
    )

    return max(
        0,
        min(COORD_MAX, scaled)
    )


def camera_to_uart_coordinates(x, y, frame_width, frame_height):
    """
    Convert camera coordinates to UART coordinates.

    Camera:
        x = 0 ... width-1
        y = 0 ... height-1

    UART:
        x = 0 ... 254
        y = 0 ... 254
    """
    if x == -1 or y == -1:
        return 255, 255
    x_uart = scale_coordinate(
        x,
        frame_width - 1
    )

    y_uart = scale_coordinate(
        y,
        frame_height - 1
    )

    return x_uart, y_uart


# ==============================================================================
# 8. UART TRANSMISSION
# ==============================================================================

def send_ball_position(ser, x, y):
    """
    Send ball position using packet:

        [0xFF] [X] [Y] [checksum]
    """

    checksum = (
        0xFF ^ x ^ y
    ) & 0xFF

    packet = bytes([
        0xFF,
        x,
        y,
        checksum
    ])
    if ser:
        ser.write(packet)

    #print(
    #    f"[PYTHON TX] "
    #    f"X={x:3d}, "
    #    f"Y={y:3d}"
    #)


# ==============================================================================
# 9. VISUALISATION
# ==============================================================================

def draw_ball(
    frame,
    ball_contour,
    x,
    y,
    x_uart,
    y_uart,
    area_percent
):
    """Draw detected ball and coordinate information."""

    cv2.drawContours(
        frame,
        [ball_contour],
        -1,
        (0, 255, 0),
        3
    )

    cv2.circle(
        frame,
        (x, y),
        6,
        (0, 0, 255),
        -1
    )

    cv2.putText(
        frame,
        f"BALL pixel=({x},{y})",
        (x + 15, max(30, y - 25)),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        (0, 255, 0),
        2
    )

    cv2.putText(
        frame,
        f"UART=({x_uart},{y_uart})",
        (x + 15, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        (255, 255, 0),
        2
    )

    cv2.putText(
        frame,
        f"Area={area_percent:.2f}%",
        (x + 15, y + 25),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (0, 255, 255),
        2
    )


def draw_no_ball(frame):
    """Show that no ball is currently detected."""

    cv2.putText(
        frame,
        "BALL NOT DETECTED",
        (20, 40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (0, 0, 255),
        2
    )


def create_controls_panel(
    brightness,
    min_saturation,
    max_saturation,
    contour_count,
    candidate_count,
    ball_position,
    uart_position
):
    """Create information panel."""

    panel = np.zeros(
        (280, 580, 3),
        dtype=np.uint8
    )

    if ball_position is None:
        ball_text = "Ball: NOT DETECTED"
        uart_text = "UART: not sending"

    else:
        x, y = ball_position
        x_uart, y_uart = uart_position

        ball_text = f"Ball: ({x},{y})"
        uart_text = f"UART: ({x_uart},{y_uart})"

    info = [
        f"Minimum Brightness (V): {brightness}",
        f"Minimum Saturation (S): {min_saturation}",
        f"Maximum Saturation (S): {max_saturation}",
        f"Expected Ball Area: "
        f"{BALL_MIN_AREA_PERCENT}-{BALL_MAX_AREA_PERCENT}%",
        f"Mask Objects: {contour_count}",
        f"Ball Candidates: {candidate_count}",
        ball_text,
        uart_text
    ]

    for i, text in enumerate(info):

        cv2.putText(
            panel,
            text,
            (15, 28 + i * 31),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.58,
            (255, 255, 255),
            2
        )

    return panel


# ==============================================================================
# 10. MAIN PIPELINE
# ==============================================================================

def main():

    # --------------------------------------------------------------------------
    # Step 1: Connect UART
    # --------------------------------------------------------------------------
    ser = None
    ser = connect_uart()

    if ser is None:
        return

    start_uart_reader(ser)

    # --------------------------------------------------------------------------
    # Step 2: Open camera
    # --------------------------------------------------------------------------
    print("camera test");
    cap = open_camera()

    if cap is None:
        ser.close()
        return

    # --------------------------------------------------------------------------
    # Step 3: Create interface
    # --------------------------------------------------------------------------

    setup_windows()

    last_tx_time = 0

    try:

        while True:

            # ------------------------------------------------------------------
            # Step 4: Read camera
            # ------------------------------------------------------------------

            success, frame = cap.read()

            if not success:
                print("[-] Could not read camera frame")
                break

            height, width = frame.shape[:2]

            # ------------------------------------------------------------------
            # Step 5: Get processing parameters
            # ------------------------------------------------------------------

            (
                brightness,
                min_saturation,
                max_saturation
            ) = get_control_values()

            # ------------------------------------------------------------------
            # Step 6: Process camera image
            # ------------------------------------------------------------------

            mask = process_frame(
                frame,
                brightness,
                min_saturation,
                max_saturation
            )

            # ------------------------------------------------------------------
            # Step 7: Find the ball
            # ------------------------------------------------------------------

            (
                ball_contour,
                ball_area_percent,
                contour_count,
                candidate_count
            ) = find_ball(
                mask,
                width,
                height
            )

            # ------------------------------------------------------------------
            # Step 8: Get ball X,Y coordinates
            # ------------------------------------------------------------------

            ball_x, ball_y = get_ball_coordinates(
                ball_contour
            )

            ball_position = None
            uart_position = None

            # ------------------------------------------------------------------
            # Step 9: If ball exists -> convert and send coordinates
            # ------------------------------------------------------------------

            ball_position = (
                ball_x,
                ball_y
            )

            x_uart, y_uart = camera_to_uart_coordinates(
                ball_x,
                ball_y,
                width,
                height
            )

            uart_position = (
                x_uart,
                y_uart
            )
            # ------------------------------------------------------------------
            # Step 10: UART transmission rate limiting
            # ------------------------------------------------------------------

            current_time = time.time()

            if current_time - last_tx_time >= TX_INTERVAL:

                send_ball_position(
                    ser,
                    x_uart,
                    y_uart
                )

                last_tx_time = current_time
            # ------------------------------------------------------------------            
            # Step 11:
            # ------------------------------------------------------------------
            if ball_x != -1 and ball_y != -1:
                draw_ball(
                    frame,
                    ball_contour,
                    ball_x,
                    ball_y,
                    x_uart,
                    y_uart,
                    ball_area_percent
                )
            else:
                draw_no_ball(frame)

            # ------------------------------------------------------------------
            # Step 12: Display results
            # ------------------------------------------------------------------

            heatmap = cv2.applyColorMap(
                mask,
                cv2.COLORMAP_JET
            )

            controls_panel = create_controls_panel(
                brightness,
                min_saturation,
                max_saturation,
                contour_count,
                candidate_count,
                ball_position,
                uart_position
            )

            cv2.imshow(
                "Camera",
                frame
            )

            cv2.imshow(
                "White Detection Heatmap",
                heatmap
            )

            cv2.imshow(
                "Controls",
                controls_panel
            )

            # Q -> quit
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    except KeyboardInterrupt:
        print("\n[+] Stopped by user.")

    finally:

        cap.release()
        if ser:
            ser.close()

        cv2.destroyAllWindows()

        print("[+] Camera and UART closed.")


# ==============================================================================
# START
# ==============================================================================

if __name__ == "__main__":
    main()