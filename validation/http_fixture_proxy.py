"""Read-only HTTP proxy for one public network-test URL.
Uses the environment's configured curl proxy route, preserving its egress policy.
A successful response proves guest TCP/HTTP through a proxy, not direct guest DNS.
"""
from http.server import BaseHTTPRequestHandler,HTTPServer
from pathlib import Path
import subprocess,hashlib,json,datetime
url='http://deb.debian.org/debian/README'
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  if self.path!=url:self.send_error(403,'Only the configured public test URL is allowed');return
  r=subprocess.run(['curl','--fail','--silent','--show-error','--max-time','20',url],capture_output=True)
  if r.returncode:self.send_error(502,'Upstream test fetch failed');return
  self.send_response(200);self.send_header('Content-Type','text/plain');self.send_header('Content-Length',str(len(r.stdout)));self.send_header('Connection','close');self.end_headers();self.wfile.write(r.stdout)
  print(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'url':url,'bytes':len(r.stdout),'sha256':hashlib.sha256(r.stdout).hexdigest()}),flush=True)
HTTPServer(('127.0.0.1',18080),Handler).serve_forever()
