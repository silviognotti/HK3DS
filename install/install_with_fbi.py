#!/usr/bin/env python3
"""Install HK3DS on your own 3DS with FBI over Wi-Fi.

1. Run this on your PC (same Wi-Fi as the 3DS):   python install_with_fbi.py
2. On the 3DS open FBI -> Remote Install -> Scan QR Code, and point the camera at the QR code.
3. FBI downloads hk3ds.cia from this PC and installs it. Press Ctrl+C here when it's done.

Only the PC running this script serves the file, and only while it runs, on your local network.
"""

import http.server, os, socket, socketserver, sys, threading, webbrowser

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import segno

PORT = int(os.environ.get("HK3DS_PORT", "8080"))
ROOT = os.path.abspath(os.path.join(HERE, ".."))  # folder that contains hk3ds.cia
CIA = "hk3ds.cia"


def lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))
        return s.getsockname()[0]
    except Exception:
        return socket.gethostbyname(socket.gethostname())
    finally:
        s.close()


def main():
    if not os.path.exists(os.path.join(ROOT, CIA)):
        sys.exit(
            f'{CIA} not found next to the install folder ({ROOT}). Build it with "make cia" or unzip the release first.'
        )
    ip = lan_ip()
    url = f"http://{ip}:{PORT}/{CIA}"
    qr = segno.make(url, error="m")
    png = os.path.join(HERE, "fbi_qr.png")
    qr.save(png, scale=10, border=4)
    print("\nScan this with FBI -> Remote Install -> Scan QR Code:\n")
    qr.terminal(compact=True)
    print(
        f"\n  {url}\n  (QR image also saved to {png})\n  Press Ctrl+C to stop when FBI has finished installing.\n"
    )
    try:
        webbrowser.open("file://" + png)
    except Exception:
        pass
    os.chdir(ROOT)

    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, fmt, *a):
            sys.stderr.write("  3DS: " + (fmt % a) + "\n")

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("", PORT), Quiet) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
