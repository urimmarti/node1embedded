import serial
import random
import threading
import time
import sys

# ==============================================================================
# CONFIGURATION
# ==============================================================================
COM_PORT = 'COM3'   # Change to your Node 1 COM port in Windows
BAUD_RATE = 9600    # Must match Node 1 UART_Init
# ==============================================================================

def reader_thread(ser):
    """Reads and displays incoming Node 1 printf output instantly."""
    while ser.is_open:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace')
                print(f"[Node 1 RX] {line}", end='', flush=True)
        except Exception:
            break

def main():
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
        print(f"[+] Connected to {COM_PORT}\n", flush=True)
    except serial.SerialException as e:
        print(f"[-] Serial error: {e}", flush=True)
        print("    Ensure PuTTY or other serial terminals are closed.", flush=True)
        sys.exit(1)

    # Start background thread to listen to Node 1 logs
    threading.Thread(target=reader_thread, args=(ser,), daemon=True).start()

    print("[+] Sending random (X, Y) packets at 20 Hz. Press Ctrl+C to stop.\n", flush=True)

    try:
        while True:
            # Generate random 8-bit values [0..254] (avoid 255/0xFF header collision)
            rand_x = random.randint(0, 254)
            rand_y = random.randint(0, 254)

            # 4-Byte Packet: [Header (0xFF), X, Y, Checksum]
            checksum = (rand_x ^ rand_y ^ 0xFF) & 0xFF
            packet = bytes([0xFF, rand_x, rand_y, checksum])

            # Send over UART
            ser.write(packet)

            # Print what Python sent
            print(f"[PYTHON TX] Sent X: {rand_x:3d}, Y: {rand_y:3d} | Packet: {[hex(b) for b in packet]}", flush=True)

            time.sleep(0.050) # 20 Hz transmission rate

    except KeyboardInterrupt:
        print("\n[+] Transmission stopped by user.", flush=True)
    finally:
        ser.close()

if __name__ == "__main__":
    main()