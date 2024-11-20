from http.server import BaseHTTPRequestHandler, HTTPServer
import os

class PowerHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/shutdown":
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Shutting down...")

            # Run the shutdown command for Windows
            os.system("shutdown /s /t 5")

        elif self.path == "/sleep":
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Putting computer to sleep...")

            # Run the sleep command for Windows
            os.system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0")

        else:
            self.send_response(404)
            self.end_headers()

def run(server_class=HTTPServer, handler_class=PowerHandler, port=8081):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print(f"Starting HTTP server on port {port}")
    httpd.serve_forever()

if __name__ == "__main__":
    run()
