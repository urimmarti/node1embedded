import os
os.environ["OPENCV_LOG_LEVEL"] = "OFF"

import cv2
import numpy as np
import serial
import threading
import time
import sys

# ==============================================================================
# CONFIGURATION
# ==============================================================================

# Serial Configuration
COM_PORT = 'COM3'        # Change to your Node 1 COM port in Windows
BAUD_RATE = 9600         # Must match Node 1 UART_Init (9600 baud)

# Camera Configuration
CAMERA_INDEX = 0         # Change index if using external USB camera
TX_INTERVAL = 0.050      # Transmit target at 20 Hz (every 50 ms)
COORD_MAX = 254          # Reserve 255 (0xFF) exclusively for packet header


# ==============================================================================
# SERIAL READER THREAD
# ==============================================================================

def reader_thread(ser):
    """Background thread to capture and print Node 1 printf output in real time."""
    while ser.is_open:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace')
                print(f"[Node 1 RX] {line}", end='', flush=True)
        except Exception:
            break


# ==============================================================================
# COORDINATE CONVERSION
# ==============================================================================

def scale_coordinate(value, max_val):
    """Scales pixel coordinate (0..max_val) to 8-bit UART payload range (0..254)."""
    if max_val <= 0:
        return 0
    scaled = round((value * COORD_MAX) / max_val)
    return max(0, min(COORD_MAX, scaled))


# ==============================================================================
# MAIN FUNCTION
# ==============================================================================

def main():
    # --------------------------------------------------------------------------
    # SERIAL PORT INITIALIZATION
    # --------------------------------------------------------------------------
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
        print(f"[+] Connected to serial port {COM_PORT} at {BAUD_RATE} baud\n", flush=True)
    except serial.SerialException as e:
        print(f"[-] Serial error: {e}", flush=True)
        print("    Ensure PuTTY or other serial terminals are closed.", flush=True)
        sys.exit(1)

    # Start background listener thread for Node 1 console feedback
    threading.Thread(target=reader_thread, args=(ser,), daemon=True).start()

    # --------------------------------------------------------------------------
    # CAMERA INITIALIZATION
    # --------------------------------------------------------------------------
    cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_MSMF)
    if not cap.isOpened():
        print("[-] Error: Could not open camera.", flush=True)
        ser.close()
        sys.exit(1)

    print("[+] Camera running. Tracking white ball & streaming coordinates over UART...\n", flush=True)

    # --------------------------------------------------------------------------
    # GUI WINDOWS & TRACKBARS
    # --------------------------------------------------------------------------
    cv2.namedWindow("Controls", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Controls", 600, 450)
    cv2.namedWindow("Camera", cv2.WINDOW_NORMAL)
    cv2.namedWindow("White Detection Heatmap", cv2.WINDOW_NORMAL)

    cv2.createTrackbar("Brightness", "Controls", 220, 255, lambda x: None)
    cv2.createTrackbar("Min S", "Controls", 0, 255, lambda x: None)
    cv2.createTrackbar("Max S", "Controls", 40, 255, lambda x: None)
    cv2.createTrackbar("Min Area %", "Controls", 1, 100, lambda x: None)

    last_tx_time = 0

    # --------------------------------------------------------------------------
    # MAIN PROCESSING LOOP
    # --------------------------------------------------------------------------
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("[-] Error reading frame from camera.", flush=True)
                break

            height, width = frame.shape[:2]
            frame_area = width * height

            # Read trackbar values
            brightness = cv2.getTrackbarPos("Brightness", "Controls")
            min_saturation = cv2.getTrackbarPos("Min S", "Controls")
            max_saturation = cv2.getTrackbarPos("Max S", "Controls")
            min_area_percent = cv2.getTrackbarPos("Min Area %", "Controls")

            minimum_area_pixels = (min_area_percent / 100.0) * frame_area

            # HSV Mask Generation
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

            if min_saturation <= max_saturation:
                lower_white = np.array([0, min_saturation, brightness], dtype=np.uint8)
                upper_white = np.array([179, max_saturation, 255], dtype=np.uint8)
                mask = cv2.inRange(hsv, lower_white, upper_white)
            else:
                mask = np.zeros((height, width), dtype=np.uint8)

            # Morphological noise filtering
            kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
            clean_mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)
            clean_mask = cv2.morphologyEx(clean_mask, cv2.MORPH_OPEN, kernel, iterations=1)

            # Contour Detection & Filtering
            contours, _ = cv2.findContours(clean_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            valid_contours = [c for c in contours if cv2.contourArea(c) >= minimum_area_pixels]

            ball_contour = None
            if len(valid_contours) > 0:
                ball_contour = max(valid_contours, key=cv2.contourArea)

            ball_detected = ball_contour is not None
            ball_x, ball_y = 0, 0
            ball_area = 0
            ball_radius = 0

            if ball_detected:
                ball_area = cv2.contourArea(ball_contour)
                M = cv2.moments(ball_contour)
                if M["m00"] != 0:
                    ball_x = int(M["m10"] / M["m00"])
                    ball_y = int(M["m01"] / M["m00"])
                else:
                    x, y, w, h = cv2.boundingRect(ball_contour)
                    ball_x = x + w // 2
                    ball_y = y + h // 2

                _, radius = cv2.minEnclosingCircle(ball_contour)
                ball_radius = int(radius)

                # Convert pixel coordinates to 8-bit range [0..254]
                x_uart = scale_coordinate(ball_x, width - 1)
                y_uart = scale_coordinate(ball_y, height - 1)

                # Transmit UART packet at 20 Hz
                current_time = time.time()
                if (current_time - last_tx_time) >= TX_INTERVAL:
                    last_tx_time = current_time

                    # 4-Byte Packet: [Header (0xFF), X, Y, Checksum]
                    checksum = (0xFF ^ x_uart ^ y_uart) & 0xFF
                    packet = bytes([0xFF, x_uart, y_uart, checksum])

                    ser.write(packet)

                    print(f"[PYTHON TX] Ball Pixels: ({ball_x:3d}, {ball_y:3d}) | "
                          f"UART Payload: X={x_uart:3d}, Y={y_uart:3d} | "
                          f"Packet: {[hex(b) for b in packet]}", flush=True)

                # Visual overlay on camera feed
                cv2.drawContours(frame, [ball_contour], -1, (0, 255, 0), 3)
                cv2.circle(frame, (ball_x, ball_y), ball_radius, (255, 0, 255), 2)
                cv2.circle(frame, (ball_x, ball_y), 6, (0, 0, 255), -1)

                cv2.putText(frame, f"BALL ({ball_x},{ball_y})",
                            (max(10, ball_x + 15), max(30, ball_y - 15)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

                cv2.putText(frame, f"UART: ({x_uart},{y_uart})",
                            (max(10, ball_x + 15), min(height - 10, ball_y + 20)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255), 2)
            else:
                cv2.putText(frame, "BALL NOT DETECTED", (20, 40),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)

            # Heatmap & Controls panel HUD
            heatmap = cv2.applyColorMap(clean_mask, cv2.COLORMAP_JET)

            controls_panel = np.zeros((260, 580, 3), dtype=np.uint8)
            ball_text = f"Ball: ({ball_x}, {ball_y})" if ball_detected else "Ball: NOT DETECTED"
            uart_text = f"UART TX: ({x_uart}, {y_uart})" if ball_detected else "UART TX: Idle"
            area_text = f"Ball Area: {ball_area:.0f}px" if ball_detected else "Ball Area: -"

            info = [
                f"Minimum Brightness (V): {brightness}",
                f"Minimum Saturation (S): {min_saturation}",
                f"Maximum Saturation (S): {max_saturation}",
                f"Minimum Object Area: {min_area_percent}%",
                f"Mask Objects: {len(contours)}",
                f"Valid Objects: {len(valid_contours)}",
                ball_text,
                uart_text
            ]

            for i, line in enumerate(info):
                cv2.putText(controls_panel, line, (15, 28 + i * 29),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.58, (255, 255, 255), 2)

            # Display GUI Windows
            cv2.imshow("Camera", frame)
            cv2.imshow("White Detection Heatmap", heatmap)
            cv2.imshow("Controls", controls_panel)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    except KeyboardInterrupt:
        print("\n[+] Script stopped by user.", flush=True)
    finally:
        cap.release()
        ser.close()
        cv2.destroyAllWindows()
        print("[+] Camera and Serial connection closed.", flush=True)

if __name__ == "__main__":
    main()