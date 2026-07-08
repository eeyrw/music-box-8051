#!/usr/bin/env python3
"""
Launch the Web Serial ADSR editor.

Usage:
  python3 tools/adsr_web.py
  python3 tools/adsr_web.py --port 8010 --no-browser
"""

import argparse
import functools
import http.server
import os
import socket
import socketserver
import sys
import webbrowser


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PAGE = "/tools/adsr_web.html"


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


class ReuseTcpServer(socketserver.TCPServer):
    allow_reuse_address = True


def port_available(host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(0.2)
        return sock.connect_ex((host, port)) != 0


def choose_port(host, preferred, attempts):
    for port in range(preferred, preferred + attempts):
        if port_available(host, port):
            return port
    raise SystemExit(f"no free TCP port in {preferred}-{preferred + attempts - 1}")


def main():
    parser = argparse.ArgumentParser(description="Launch Music Box ADSR Web Serial editor")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP bind address")
    parser.add_argument("--port", type=int, default=8000, help="preferred HTTP port")
    parser.add_argument("--port-attempts", type=int, default=20,
                        help="number of consecutive ports to try")
    parser.add_argument("--no-browser", action="store_true", help="do not open a browser")
    args = parser.parse_args()

    port = choose_port(args.host, args.port, args.port_attempts)
    handler = functools.partial(QuietHandler, directory=ROOT)
    url = f"http://{args.host}:{port}{PAGE}"

    with ReuseTcpServer((args.host, port), handler) as httpd:
        print("Music Box ADSR Web Serial editor")
        print(f"Serving: {url}")
        print("Press Ctrl+C to stop.")
        if not args.no_browser:
            webbrowser.open(url)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
