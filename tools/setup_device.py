"""One-time, loopback-only USB setup. This is not a runtime companion service."""
import argparse
import json
import os
from pathlib import Path
import secrets
import re
import httpx
import subprocess
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlsplit, parse_qs, quote
import serial
from serial.tools import list_ports
from build_host import ROOT, build

DEFAULTS = {'version':1,'mock':False,'stop':'13911','stop_name':'Carl & Cole','route':'N',
            'direction_ref':'','direction':'Downtown / Caltrain','timezone':'PST8PDT,M3.2.0,M11.1.0',
            'transit_seconds':61,'calendar_seconds':900,'night_seconds':1800,'active_start':6,'active_end':23}

def normalize_calendar(value):
    """IDs/embed links address public feeds; they never bypass calendar sharing."""
    if not isinstance(value,str):raise ValueError('Enter a Google Calendar link or calendar ID.')
    value=value.strip()
    if not value:return ''
    if len(value)>2048 or any(c in value for c in '\r\n\x00'):
        raise ValueError('Enter a valid Google Calendar link or calendar ID.')
    if value.startswith('https://'):
        url=urlsplit(value)
        if url.netloc!='calendar.google.com' or url.fragment:
            raise ValueError('Use a link from calendar.google.com.')
        if url.path.startswith('/calendar/ical/') and url.path.endswith('/basic.ics') and not url.query:
            return value
        if url.path=='/calendar/embed':
            ids=parse_qs(url.query).get('src',[])
            if len(ids)!=1:raise ValueError('Use a link for one calendar at a time.')
            value=ids[0]
        else:raise ValueError('Use the public calendar URL, calendar ID, or iCal subscription link.')
    if not re.fullmatch(r'[A-Za-z0-9._+%#-]+@[A-Za-z0-9.-]+',value) or len(value)>512:
        raise ValueError('Enter a calendar ID such as name@group.calendar.google.com, or a Google Calendar link.')
    return 'https://calendar.google.com/calendar/ical/'+quote(value,safe='')+'/public/basic.ics'

def check_calendar(url):
    if not url:return {'ok':True,'message':'No calendar selected yet.'}
    public='/public/basic.ics' in url
    unavailable=('This calendar is private or the ID is incorrect. Its public URL does not grant access. '
                 'Use the owner’s Secret address in iCal format, or leave this field empty for now.') if public else (
                 'Google could not open this subscription. Copy the current Secret address in iCal format from the calendar owner’s settings.')
    try:
        with httpx.stream('GET',url,follow_redirects=False,timeout=20) as response:
            if response.status_code!=200:raise ValueError(unavailable)
            content=bytearray()
            for chunk in response.iter_bytes():
                content.extend(chunk)
                if len(content)>1024*1024:raise ValueError('This calendar feed is too large for the display (1 MB maximum).')
            if b'BEGIN:VCALENDAR' not in content[:256] or b'END:VCALENDAR' not in content[-256:]:
                raise ValueError(unavailable)
    except httpx.HTTPError:
        raise ValueError('Could not reach Google Calendar. Check the connection and try again.') from None
    return {'ok':True,'message':'Calendar feed is accessible without Google sign-in. Ready to save to the display.'}

def resolve_port(requested=None):
    ports=list(list_ports.comports())
    if requested:
        if any(p.device==requested for p in ports):return requested
        raise ValueError('The selected USB device is disconnected. Reconnect your reTerminal and try again.')
    # The E1001's CH340 USB-to-UART bridge; do not select Bluetooth or debug ports.
    matches=[p.device for p in ports if (p.vid,p.pid)==(0x1a86,0x7523)]
    if len(matches)==1:return matches[0]
    if not matches:raise ValueError('Connect your reTerminal to this Mac with a USB data cable.')
    raise ValueError('More than one compatible USB device is connected. Disconnect the other device or start setup with --port.')

def save_private(config):
    folder=ROOT/'private';folder.mkdir(exist_ok=True);os.chmod(folder,0o700)
    path=folder/'config.json'
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
    with os.fdopen(fd,'w') as f: json.dump(config,f,indent=2)
    os.chmod(path,0o600)
    return path

WIFI_MESSAGES={
    'connected':'Wi-Fi connected successfully. The display received a network address.',
    'network_not_found':'The display cannot find that 2.4 GHz network. Choose it from Scan networks, or check the network name and move closer to the router.',
    'authentication_failed':'The Wi-Fi login failed. Check the password carefully. If it is correct, check that the router supports WPA2 or WPA2/WPA3 mixed mode.',
    'association_failed':'The router did not accept the connection. Move the display closer and check whether the router blocks new devices.',
    'dhcp_timeout':'The display joined Wi-Fi, but the router did not assign a network address. Check the router’s DHCP settings and retry.',
    'connection_timeout':'The Wi-Fi connection timed out. Move the display closer to the router and check the network name and password.',
}

def usb_exchange(command,port=None,expected='saved'):
    port=resolve_port(port)
    wire=command.encode()
    if len(wire)>4120:raise ValueError('Configuration is too large.')
    # Construct without a port to set control lines before opening; never echo secrets.
    device=serial.Serial(port=None,baudrate=115200,timeout=.2,write_timeout=2)
    device.dtr=False;device.rts=False;device.port=port
    with device:
        device.rts=True;time.sleep(.15);device.rts=False
        deadline=time.monotonic()+18
        sent=False;last_ping=0;result={}
        while time.monotonic()<deadline:
            if not sent and time.monotonic()-last_ping>1:
                device.write(b'PING\n');last_ping=time.monotonic()
            line=device.readline().decode(errors='replace').strip()
            if line=='NTRAIN_WIFI_SETUP_V1' and not sent:
                device.write(wire+b'\n');device.flush();sent=True;deadline=time.monotonic()+50
            if line.startswith('NTRAIN_WIFI_RESULT_V1 ') and sent:
                result=json.loads(line.split(' ',1)[1])
                result['message']=WIFI_MESSAGES.get(result.get('code'),WIFI_MESSAGES['connection_timeout'])
                if expected=='wifi':return result
                if not result.get('ok'):raise ValueError(result['message']+' Settings were not changed on the display.')
            if line.startswith('NTRAIN_WIFI_SCAN_V1 ') and sent and expected=='scan':
                return json.loads(line.split(' ',1)[1])
            if line=='NTRAIN_CONFIG_SAVED_V1' and sent and expected=='saved':return result
            if line.startswith('NTRAIN_CONFIG_REJECTED') or line.startswith('NTRAIN_CONFIG_STORAGE_ERROR'):
                raise ValueError('The device rejected the settings. Check the field values.')
        raise TimeoutError('No response from the updated setup firmware. Keep USB connected, close any serial monitor and retry.')

def provision(config,port=None):
    wire=json.dumps(config,separators=(',',':'),ensure_ascii=False)
    if len(wire.encode())>4096:raise ValueError('Configuration is too large.')
    return usb_exchange(wire,port)

def validated_config(body,host):
    allowed={'ssid','password','api_key','calendar_url','calendar_url_2','mock','active_start','active_end'}
    if not isinstance(body,dict) or any(k not in allowed for k in body):raise ValueError('Unexpected settings.')
    config=DEFAULTS|body
    for key in ['calendar_url','calendar_url_2']:config[key]=normalize_calendar(config.get(key,''))
    # Validate with exactly the same C++ function used by the ESP32, before saving.
    folder=ROOT/'private';folder.mkdir(exist_ok=True);os.chmod(folder,0o700)
    path=folder/'pending-config.json'
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
    try:
        with os.fdopen(fd,'w') as f:json.dump(config,f)
        result=subprocess.run([str(host),'config',str(path)],capture_output=True,text=True,check=True,timeout=5)
        if not json.loads(result.stdout)['ok']:raise ValueError('Check Wi-Fi details, hours, and the Google Calendar iCal URL.')
    finally:path.unlink(missing_ok=True)
    return config

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',help='USB serial port (default: detect the connected reTerminal automatically)')
    parser.add_argument('--http-port',type=int,default=8765)
    parser.add_argument('--no-browser',action='store_true')
    parser.add_argument('--file',type=Path,help='Provision an existing private config without the setup page')
    args=parser.parse_args()
    if args.file:
        config=json.loads(args.file.read_text());provision(config,args.port);print('Settings saved to the device.');return
    host=build();token=secrets.token_urlsafe(32)
    origin=f'http://127.0.0.1:{args.http_port}'
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def respond(self,status,data,kind='application/json'):
            self.send_response(status);self.send_header('Content-Type',kind+'; charset=utf-8')
            self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
            self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self'; connect-src 'self'; frame-ancestors 'none'")
            self.end_headers();self.wfile.write(data if isinstance(data,bytes) else data.encode())
        def do_GET(self):
            if self.headers.get('Host')!=urlsplit(origin).netloc:return self.respond(403,'{}')
            if self.path=='/':
                html=(ROOT/'tools/setup.html').read_text().replace('__CSRF__',token)
                return self.respond(200,html,'text/html')
            if self.path=='/preview.png':return self.respond(200,(ROOT/'preview/mock.png').read_bytes(),'image/png')
            if self.path=='/device':
                try:
                    resolve_port(args.port)
                    status={'connected':True,'message':'USB is ready. Use Test Wi-Fi to verify the wireless connection.'}
                except ValueError as exc:
                    status={'connected':False,'message':str(exc)}
                return self.respond(200,json.dumps(status))
            return self.respond(404,'{}')
        def do_POST(self):
            if self.path not in ('/configure','/calendar/test','/wifi/scan','/wifi/test','/wifi/saved-test') or self.headers.get('Host')!=urlsplit(origin).netloc or self.headers.get('Origin')!=origin or not secrets.compare_digest(self.headers.get('X-Setup-Token',''),token):return self.respond(403,'{}')
            try:
                length=int(self.headers.get('Content-Length','0'))
                if not 0<length<=8192:raise ValueError('Invalid request size.')
                body=json.loads(self.rfile.read(length))
                if self.path=='/calendar/test':
                    results=[]
                    for i,key in enumerate(['calendar_url','calendar_url_2']):
                        url=normalize_calendar(body.get(key,''))
                        if not url:continue
                        try:results.append(check_calendar(url))
                        except ValueError as exc:raise ValueError(f'Calendar {i+1}: {exc}') from None
                    message=f'{len(results)} calendar feeds are accessible without Google sign-in. Ready to save.' if results else 'Add at least one calendar link or ID.'
                    return self.respond(200,json.dumps({'ok':bool(results),'message':message}))
                if self.path=='/wifi/scan':
                    result=usb_exchange('SCAN_WIFI',args.port,'scan')
                    result['message']='Choose your network below.' if result.get('ok') else 'The Wi-Fi scan failed. Retry with the display closer to your router.'
                    return self.respond(200,json.dumps(result))
                if self.path=='/wifi/saved-test':
                    return self.respond(200,json.dumps(usb_exchange('TEST_SAVED_WIFI',args.port,'wifi')))
                config=validated_config(body,host)
                if self.path=='/wifi/test':
                    wire='TEST_WIFI '+json.dumps(config,separators=(',',':'),ensure_ascii=False)
                    return self.respond(200,json.dumps(usb_exchange(wire,args.port,'wifi')))
                if not config['mock']:
                    for i,key in enumerate(['calendar_url','calendar_url_2']):
                        if config.get(key):
                            try:check_calendar(config[key])
                            except ValueError as exc:raise ValueError(f'Calendar {i+1}: {exc}') from None
                result=provision(config,args.port)
                save_private(config)
                message='Demo settings saved to your reTerminal.' if config['mock'] else 'Wi-Fi tested successfully and settings saved. The display is restarting to fetch live data. Keep it powered on; calendar and transit are checked separately.'
                self.respond(200,json.dumps({'message':message,'wifi_verified':bool(result.get('ok'))}))
            except (ValueError,TimeoutError) as exc:self.respond(400,json.dumps({'message':str(exc)}))
            except (serial.SerialException,OSError):self.respond(503,json.dumps({'message':'Setup could not finish saving. Keep USB connected, close any other serial connection and retry.'}))
            except Exception:self.respond(500,json.dumps({'message':'Setup could not complete. No credentials were logged. Check the device connection and retry.'}))
    server=HTTPServer(('127.0.0.1',args.http_port),Handler)
    print(f'Local setup: {origin} — press Ctrl+C when finished.',flush=True)
    if not args.no_browser:webbrowser.open(origin)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()

if __name__=='__main__':main()
