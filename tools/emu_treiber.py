# Emulator-Treiber fuer tools/screenshots.sh - OHNE die Uhrzeit neu zu setzen.
#
# Jeder `pebble`-Befehl stellt die Uhr des Emulators auf die echte Zeit. Fuer
# Bilder zu einer festen Tageszeit geht darum alles nach dem Installieren
# hierueber: Zeit setzen, Tasten, Bildschirmfotos. Die Fotos werden wie bei
# `pebble screenshot` umgerechnet (Farbkorrektur, runde Ecken), damit sie
# zeigen, was die Uhr zeigt.
#
# Laeuft mit dem Python des pebble-Werkzeugs:
#   <python> emu_treiber.py <plattform> schritt [schritt ...]
#     t=JJJJ-MM-TTTHH:MM:SS   Uhrzeit (Europe/Zurich)
#     b=back|up|select|down   Taste kurz
#     lang=select             Taste lang (1 s)
#     w=sek                   warten
#     s=pfad.png              Bildschirmfoto
import datetime
import json
import os
import sys
import tempfile
import time
from zoneinfo import ZoneInfo

import png
from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.communication.transports.websocket import WebsocketTransport
from libpebble2.protocol.system import SetUTC, TimeMessage
from libpebble2.services.screenshot import Screenshot
from pebble_tool.commands.emucontrol import send_data_to_qemu
from pebble_tool.commands.screenshot import ScreenshotCommand

TASTEN = {'back': QemuButton.Button.Back, 'up': QemuButton.Button.Up,
          'select': QemuButton.Button.Select, 'down': QemuButton.Button.Down}


def verbinden(plattform):
    reg = os.path.join(tempfile.gettempdir(), 'pb-emulator.json')
    eintrag = json.load(open(reg))[plattform]
    info = eintrag[sorted(eintrag)[-1]]
    pc = PebbleConnection(WebsocketTransport('ws://localhost:%d/' % info['pypkjs']['port']))
    pc.connect()
    pc.run_async()
    return pc


def taste(pc, name, halten):
    send_data_to_qemu(pc.transport, QemuButton(state=TASTEN[name]))
    time.sleep(halten)
    send_data_to_qemu(pc.transport, QemuButton(state=0))
    time.sleep(0.8)


def foto(pc, plattform, pfad):
    roh = Screenshot(pc).grab_image()
    zeilen = [list(r) for r in roh]

    class Ersatz:          # _roundify liest nur die Plattform
        class pebble:
            watch_platform = plattform
    bild = ScreenshotCommand._correct_colours(None, zeilen)
    bild = ScreenshotCommand._roundify(Ersatz(), bild)
    png.from_array(bild, mode='RGBA;8').save(pfad)
    print('  Bild', pfad)


def main():
    plattform = sys.argv[1]
    pc = verbinden(plattform)
    for schritt in sys.argv[2:]:
        k, v = schritt.split('=', 1)
        if k == 't':
            dt = datetime.datetime.strptime(v, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=ZoneInfo('Europe/Zurich'))
            off = int(dt.utcoffset().total_seconds() // 60)
            pc.send_packet(TimeMessage(message=SetUTC(unix_time=int(dt.timestamp()), utc_offset=off,
                                                      tz_name='UTC%+d' % (off // 60))))
            time.sleep(1)
        elif k == 'b':
            taste(pc, v, 0.1)
        elif k == 'lang':
            taste(pc, v, 1.0)
        elif k == 'w':
            time.sleep(float(v))
        elif k == 's':
            foto(pc, plattform, v)
        else:
            sys.exit('unbekannter Schritt: ' + schritt)


if __name__ == '__main__':
    main()
