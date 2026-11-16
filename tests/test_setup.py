"""USB provisioning must never report success before Wi-Fi and storage succeed."""
import json
from pathlib import Path
import sys
from types import SimpleNamespace
import pytest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import setup_device as setup


@pytest.fixture
def device(monkeypatch):
    class Device:
        def __init__(self):self.lines=[];self.written=[]
        def __enter__(self):return self
        def __exit__(self,*args):pass
        def write(self,data):self.written.append(data)
        def flush(self):pass
        def readline(self):
            assert self.lines, 'Unexpected read after the end of the protocol exchange'
            return (self.lines.pop(0)+'\n').encode()
    result=Device()
    monkeypatch.setattr(setup,'resolve_port',lambda requested:'/dev/test')
    monkeypatch.setattr(setup.serial,'Serial',lambda **kwargs:result)
    monkeypatch.setattr(setup.time,'sleep',lambda seconds:None)
    return result


def test_failed_wifi_cannot_report_settings_saved(device):
    device.lines=['NTRAIN_READY_V1','NTRAIN_WIFI_SETUP_V1',
                  'NTRAIN_WIFI_RESULT_V1 {"ok":false,"code":"authentication_failed","reason":202}']
    with pytest.raises(ValueError,match='Settings were not changed'):
        setup.provision({'version':1,'mock':False,'ssid':'Home','password':'test-password'})


def test_success_waits_for_persistent_storage_ack(device):
    device.lines=['NTRAIN_WIFI_SETUP_V1',
                  'NTRAIN_WIFI_RESULT_V1 {"ok":true,"code":"connected","rssi":-49}',
                  'NTRAIN_CONFIG_SAVED_V1']
    result=setup.provision({'version':1,'mock':False,'ssid':'Home','password':'test-password'})
    assert result['ok'] is True and device.lines==[]
    configs=[json.loads(v) for v in device.written if v.startswith(b'{')]
    assert len(configs)==1 and configs[0]['ssid']=='Home'


def test_storage_failure_is_not_wifi_success(device):
    device.lines=['NTRAIN_WIFI_SETUP_V1',
                  'NTRAIN_WIFI_RESULT_V1 {"ok":true,"code":"connected"}',
                  'NTRAIN_CONFIG_STORAGE_ERROR']
    with pytest.raises(ValueError,match='rejected'):
        setup.provision({'version':1,'mock':False,'ssid':'Home'})


def test_saved_wifi_diagnostic_sends_no_credentials(device):
    device.lines=['NTRAIN_WIFI_SETUP_V1',
                  'NTRAIN_WIFI_RESULT_V1 {"ok":false,"code":"network_not_found","reason":201}']
    result=setup.usb_exchange('TEST_SAVED_WIFI',expected='wifi')
    assert result['ok'] is False and '2.4 GHz' in result['message']
    assert device.written==[b'PING\n',b'TEST_SAVED_WIFI\n']


def test_scan_returns_device_networks_without_saving(device):
    device.lines=['NTRAIN_WIFI_SETUP_V1',
                  'NTRAIN_WIFI_SCAN_V1 {"ok":true,"networks":[{"ssid":"Home","rssi":-51}]}']
    result=setup.usb_exchange('SCAN_WIFI',expected='scan')
    assert result['networks'][0]['ssid']=='Home'
    assert device.written==[b'PING\n',b'SCAN_WIFI\n']


def test_demo_ack_does_not_claim_wifi_verified(device):
    device.lines=['NTRAIN_WIFI_SETUP_V1','NTRAIN_CONFIG_SAVED_V1']
    assert not setup.provision({'version':1,'mock':True}).get('ok')


def test_port_selection_follows_device_and_rejects_ambiguity(monkeypatch):
    def port(name,vid=None,pid=None):return SimpleNamespace(device=name,vid=vid,pid=pid)
    usb=port('/dev/cu.usbserial-10',0x1a86,0x7523)
    ports=[port('/dev/cu.Bluetooth-Incoming-Port'),usb]
    monkeypatch.setattr(setup.list_ports,'comports',lambda:ports)
    assert setup.resolve_port()==usb.device
    with pytest.raises(ValueError,match='disconnected'):setup.resolve_port('/dev/cu.usbserial-110')
    ports.append(port('/dev/cu.usbserial-20',0x1a86,0x7523))
    with pytest.raises(ValueError,match='More than one'):setup.resolve_port()
    ports.clear()
    with pytest.raises(ValueError,match='USB data cable'):setup.resolve_port()

@pytest.mark.parametrize('value',[
    'family@group.calendar.google.com',
    'https://calendar.google.com/calendar/embed?src=family%40group.calendar.google.com&ctz=America%2FLos_Angeles',
    'https://calendar.google.com/calendar/ical/family%40group.calendar.google.com/public/basic.ics',
])
def test_calendar_public_inputs_normalize_to_same_subscription(value):
    assert setup.normalize_calendar(value)=='https://calendar.google.com/calendar/ical/family%40group.calendar.google.com/public/basic.ics'

def test_private_subscription_is_preserved():
    url='https://calendar.google.com/calendar/ical/family%40group.calendar.google.com/private-token/basic.ics'
    assert setup.normalize_calendar(' '+url+' ')==url

@pytest.mark.parametrize('value',[
    'https://calendar.google.com.evil.example/calendar/embed?src=x@y',
    'https://calendar.google.com@evil.example/calendar/embed?src=x@y',
    'https://calendar.google.com/calendar/embed?src=a@b&src=c@d',
    'https://calendar.google.com/calendar/ical/x/public/basic.ics?redirect=evil',
    'http://calendar.google.com/calendar/embed?src=x@y',
    'bad\n@group.calendar.google.com',
])
def test_calendar_invalid_inputs_are_rejected(value):
    with pytest.raises(ValueError):setup.normalize_calendar(value)

@pytest.mark.parametrize('status,content',[(404,b''),(403,b''),(302,b''),(200,b'<html>Sign in</html>')])
def test_public_calendar_permission_failure_explains_secret_link(monkeypatch,status,content):
    class Response:
        status_code=status
        def __enter__(self):return self
        def __exit__(self,*args):pass
        def iter_bytes(self):yield content
    monkeypatch.setattr(setup.httpx,'stream',lambda *a,**k:Response())
    with pytest.raises(ValueError,match='public URL does not grant access'):
        setup.check_calendar(setup.normalize_calendar('family@group.calendar.google.com'))

def test_accessible_ical_subscription(monkeypatch):
    class Response:
        status_code=200
        def __enter__(self):return self
        def __exit__(self,*args):pass
        def iter_bytes(self):yield b'BEGIN:VCALENDAR\r\nVERSION:2.0\r\nEND:VCALENDAR\r\n'
    monkeypatch.setattr(setup.httpx,'stream',lambda *a,**k:Response())
    assert setup.check_calendar(setup.normalize_calendar('family@group.calendar.google.com'))['ok']
