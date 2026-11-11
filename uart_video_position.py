import cv2
import numpy as np
import serial
import threading
import time
import sys


# ==============================================================================
# CONFIGURATION
# ==============================================================================

# Serial
COM_PORT = "COM3"
BAUD_RATE = 9600

# Camera
CAMERA_INDEX = 2

# Send ball coordinates at 20 Hz maximum
TX_INTERVAL = 0.050

# Working HSV configuration
DEFAULT_BRIGHTNESS = 224
DEFAULT_MIN_S = 0
DEFAULT_MAX_S = 21

# Expected ball size
BALL_MIN_AREA_PERCENT = 0.5
BALL_MAX_AREA_PERCENT = 1.0

# Keep 0xFF free for packet header
COORD_MAX = 254


# ==============================================================================
# SERIAL READER
# ==============================================================================

def reader_thread(ser):
    """
    Read printf/debug messages coming from Node 1.
    """

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


# ==============================================================================
# COORDINATE CONVERSION
# ==============================================================================

def scale_coordinate(value, maximum):
    """
    Convert pixel coordinate to range 0...254.
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


# ==============================================================================
# UART SEND
# ==============================================================================

def send_ball_position(ser, x, y):
    """
    Packet:

        [0xFF] [X] [Y] [checksum]

    X and Y are 0...254.
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

    ser.write(packet)

    print(
        f"[PYTHON TX] "
        f"X: {x:3d}, "
        f"Y: {y:3d} | "
        f"Packet: {[hex(b) for b in packet]}",
        flush=True
    )


# ==============================================================================
# MAIN
# ==============================================================================

def main():

    # ==========================================================================
    # SERIAL
    # ==========================================================================

    try:

        ser = serial.Serial(
            COM_PORT,
            BAUD_RATE,
            timeout=0.1
        )

        print(
            f"[+] Connected to {COM_PORT}\n",
            flush=True
        )

    except serial.SerialException as e:

        print(
            f"[-] Serial error: {e}",
            flush=True
        )

        print(
            "Ensure PuTTY or other serial terminals are closed.",
            flush=True
        )

        sys.exit(1)


    threading.Thread(
        target=reader_thread,
        args=(ser,),
        daemon=True
    ).start()


    # ==========================================================================
    # CAMERA
    # ==========================================================================

    cap = cv2.VideoCapture(
        CAMERA_INDEX,
        cv2.CAP_V4L2
    )

    if not cap.isOpened():

        print("[-] Error opening camera")

        ser.close()

        sys.exit(1)


    # ==========================================================================
    # WINDOWS
    # ==========================================================================

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


    # ==========================================================================
    # TRACKBARS
    # ==========================================================================

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


    last_tx = 0


    # ==========================================================================
    # LOOP
    # ==========================================================================

    try:

        while True:

            ret, frame = cap.read()

            if not ret:
                print("[-] Error reading camera")
                break


            height, width = frame.shape[:2]

            frame_area = width * height


            # ==================================================================
            # READ CONTROLS
            # ==================================================================

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


            # ==================================================================
            # HSV FILTER
            # ==================================================================

            hsv = cv2.cvtColor(
                frame,
                cv2.COLOR_BGR2HSV
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


            if min_saturation <= max_saturation:

                mask = cv2.inRange(
                    hsv,
                    lower_white,
                    upper_white
                )

            else:

                mask = np.zeros(
                    (height, width),
                    dtype=np.uint8
                )


            # ==================================================================
            # CLEAN MASK
            # ==================================================================

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


            # ==================================================================
            # FIND OBJECTS
            # ==================================================================

            contours, _ = cv2.findContours(
                clean_mask,
                cv2.RETR_EXTERNAL,
                cv2.CHAIN_APPROX_SIMPLE
            )


            ball_candidates = []


            for contour in contours:

                area = cv2.contourArea(
                    contour
                )

                area_percent = (
                    100.0
                    * area
                    / frame_area
                )


                # Ball occupies 0.5% ... 1.0%
                if (
                    BALL_MIN_AREA_PERCENT
                    <= area_percent
                    <= BALL_MAX_AREA_PERCENT
                ):

                    ball_candidates.append(
                        (
                            contour,
                            area,
                            area_percent
                        )
                    )


            # ==================================================================
            # SELECT BALL
            # ==================================================================

            ball_detected = False

            ball_contour = None
            ball_x = 0
            ball_y = 0
            ball_area_percent = 0


            if ball_candidates:

                # Prefer size closest to middle of expected range
                target_area_percent = (
                    BALL_MIN_AREA_PERCENT
                    + BALL_MAX_AREA_PERCENT
                ) / 2.0


                (
                    ball_contour,
                    ball_area,
                    ball_area_percent
                ) = min(
                    ball_candidates,
                    key=lambda obj:
                        abs(
                            obj[2]
                            - target_area_percent
                        )
                )


                M = cv2.moments(
                    ball_contour
                )


                if M["m00"] != 0:

                    ball_x = int(
                        M["m10"]
                        / M["m00"]
                    )

                    ball_y = int(
                        M["m01"]
                        / M["m00"]
                    )

                    ball_detected = True


            # ==================================================================
            # IF BALL DETECTED
            # ==================================================================

            if ball_detected:

                # --------------------------------------------------------------
                # Scale coordinates
                #
                # Camera:
                #
                # x = 0 ........ width-1
                # y = 0 ........ height-1
                #
                # UART:
                #
                # x = 0 ........ 254
                # y = 0 ........ 254
                # --------------------------------------------------------------

                x_uart = scale_coordinate(
                    ball_x,
                    width - 1
                )

                y_uart = scale_coordinate(
                    ball_y,
                    height - 1
                )


                # --------------------------------------------------------------
                # Send maximum 20 packets / second
                # --------------------------------------------------------------

                current_time = time.time()

                if (
                    current_time
                    - last_tx
                    >= TX_INTERVAL
                ):

                    send_ball_position(
                        ser,
                        x_uart,
                        y_uart
                    )

                    last_tx = current_time


                # --------------------------------------------------------------
                # Draw contour
                # --------------------------------------------------------------

                cv2.drawContours(
                    frame,
                    [ball_contour],
                    -1,
                    (0, 255, 0),
                    3
                )


                # Centre
                cv2.circle(
                    frame,
                    (ball_x, ball_y),
                    6,
                    (0, 0, 255),
                    -1
                )


                # --------------------------------------------------------------
                # Text
                # --------------------------------------------------------------

                cv2.putText(
                    frame,
                    f"BALL pixel=({ball_x},{ball_y})",
                    (
                        max(10, ball_x + 15),
                        max(30, ball_y - 25)
                    ),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.65,
                    (0, 255, 0),
                    2
                )


                cv2.putText(
                    frame,
                    f"UART=({x_uart},{y_uart})",
                    (
                        max(10, ball_x + 15),
                        max(50, ball_y)
                    ),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.65,
                    (255, 255, 0),
                    2
                )


                cv2.putText(
                    frame,
                    f"Area={ball_area_percent:.2f}%",
                    (
                        max(10, ball_x + 15),
                        min(height - 10, ball_y + 25)
                    ),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.55,
                    (0, 255, 255),
                    2
                )


            else:

                cv2.putText(
                    frame,
                    "BALL NOT DETECTED",
                    (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.8,
                    (0, 0, 255),
                    2
                )


            # ==================================================================
            # HEATMAP
            # ==================================================================

            heatmap = cv2.applyColorMap(
                clean_mask,
                cv2.COLORMAP_JET
            )


            # ==================================================================
            # CONTROLS PANEL
            # ==================================================================

            controls_panel = np.zeros(
                (280, 580, 3),
                dtype=np.uint8
            )


            if ball_detected:

                status_text = (
                    f"Ball: ({ball_x},{ball_y})"
                )

                uart_text = (
                    f"UART: ({x_uart},{y_uart})"
                )

            else:

                status_text = (
                    "Ball: NOT DETECTED"
                )

                uart_text = (
                    "UART: not sending"
                )


            info = [

                f"Minimum Brightness (V): "
                f"{brightness}",

                f"Minimum Saturation (S): "
                f"{min_saturation}",

                f"Maximum Saturation (S): "
                f"{max_saturation}",

                "Expected Ball Area: "
                "0.5-1.0%",

                f"Mask Objects: "
                f"{len(contours)}",

                f"Ball Candidates: "
                f"{len(ball_candidates)}",

                status_text,

                uart_text
            ]


            for i, line in enumerate(info):

                cv2.putText(
                    controls_panel,
                    line,
                    (
                        15,
                        28 + i * 31
                    ),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.58,
                    (255, 255, 255),
                    2
                )


            # ==================================================================
            # DISPLAY
            # ==================================================================

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


            if (
                cv2.waitKey(1)
                & 0xFF
                == ord("q")
            ):
                break


    except KeyboardInterrupt:

        print(
            "\n[+] Stopped by user."
        )


    finally:

        cap.release()

        cv2.destroyAllWindows()

        ser.close()

        print(
            "[+] Camera and serial port closed."
        )


# ==============================================================================
# START
# ==============================================================================

if __name__ == "__main__":
    main()
