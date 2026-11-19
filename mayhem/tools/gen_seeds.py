#!/usr/bin/env python3
"""Regenerate the committed seed inputs of the RC-protocol, FTP-upload and param-defaults targets.

Usage: python3 -I mayhem/tools/gen_seeds.py <repo root>

replay/testsuite/*.bin are the first 48 kB / 200 kB of a DataFlash log written by
`arducopter --model quad -w` with LOG_DISARMED=1, LOG_REPLAY=1 (SITL, about 30 s).
json/testsuite/*.json are copies of upstream's Tools/autotest/models/*.json.
"""
import os
import re
import struct
import sys

root = sys.argv[1]
out = os.path.join(root, 'mayhem')

# --- RC protocols: the byte streams upstream's RCProtocolTest example decodes ---------------------
BAUDS = {100000: 0, 115200: 1, 416666: 2, 420000: 3}
src = open(os.path.join(root, 'libraries/AP_RCProtocol/examples/RCProtocolTest/RCProtocolTest.cpp')).read()
arrays = {}
for m in re.finditer(r'(?:static\s+)?const\s+uint8_t\s+(\w+)\s*\[\]\s*=\s*\{(.*?)\};', src, re.S):
    body = re.sub(r'//.*', '', m.group(2))
    arrays[m.group(1)] = [int(x, 0) for x in re.findall(r'0[xX][0-9a-fA-F]+|\d+', body)]
n = 0
for m in re.finditer(r'test_protocol(?:_bytesonly)?\("(\w+)",\s*(\d+),\s*(\w+),\s*sizeof\(\w+\),\s*[^,]+,\s*[^,]+,\s*(\d+)(?:,\s*(\d+))?', src):
    name, baud, arr, repeats, pause_at = m.group(1), int(m.group(2)), m.group(3), int(m.group(4)), int(m.group(5) or 0)
    if baud not in BAUDS or arr not in arrays:
        continue
    data = arrays[arr]
    step = pause_at if pause_at else 255
    blob = bytes([BAUDS[baud]])
    for _ in range(repeats + 4):
        for i in range(0, len(data), step):
            chunk = data[i:i + min(step, 255)]
            blob += bytes([len(chunk)]) + bytes(chunk)
    open(os.path.join(out, 'rcprotocol/testsuite', '%s.bin' % name.lower()), 'wb').write(blob)
    n += 1
print('rcprotocol seeds:', n)

# --- MAVLink FTP uploads ----------------------------------------------------------------------------
def item(seq, cmd, p1=0.0, p2=0.0, p3=0.0, p4=0.0, x=0, y=0, z=0.0, frame=3, mtype=0):
    # mavlink_mission_item_int_t in wire order (MAVLINK_MSG_ID_MISSION_ITEM_INT_LEN = 38)
    return struct.pack('<ffffiifHHBBBBBB', p1, p2, p3, p4, x, y, z, seq, cmd, 1, 1, frame, 0, 1, mtype)

def mission_file(mtype, items, options=0, start=0):
    return struct.pack('<HHHHH', 0x763d, mtype, options, start, len(items)) + b''.join(items)

lat, lng = -353632620, 1491652370
mission = [item(0, 16, x=lat, y=lng, z=584, frame=0), item(1, 22, z=20), item(2, 16, x=lat + 1000, y=lng, z=30),
           item(3, 177, p1=2, p2=3), item(4, 183, p1=9, p2=1500), item(5, 20)]
fence = [item(i, 5001, p1=4, x=lat + dx, y=lng + dy, mtype=1, frame=5)
         for i, (dx, dy) in enumerate([(0, 0), (2000, 0), (2000, 2000), (0, 2000)])]
fence.append(item(4, 5003, p1=150.0, x=lat, y=lng, mtype=1, frame=5))
rally = [item(0, 5100, x=lat + 500, y=lng + 500, z=50, mtype=2), item(1, 5100, x=lat - 500, y=lng, z=40, mtype=2)]

def pck(params):
    body, prev = b'', b''
    for name, ptype, value in params:
        name = name.encode()
        common = 0
        while common < min(len(prev), len(name), 15) and prev[common] == name[common]:
            common += 1
        rest = name[common:]
        fmt = {1: '<b', 2: '<h', 3: '<i', 4: '<f'}[ptype]
        body += bytes([ptype, ((len(rest) - 1) << 4) | common]) + rest + struct.pack(fmt, value)
        prev = name
    total = 6 + len(body)
    return struct.pack('<HHH', 0x671b, len(params), total) + body

seeds = {
    'mission.bin': b'\x00' + mission_file(0, mission),
    'fence.bin': b'\x01' + mission_file(1, fence),
    'rally.bin': b'\x02' + mission_file(2, rally),
    'params.bin': b'\x03' + pck([('FZ_FLT', 4, 2.5), ('FZ_I16', 2, 300), ('FZ_I32', 3, 70000), ('FZ_I8', 1, 5),
                                 ('MIS_TOTAL', 2, 10), ('RALLY_LIMIT_KM', 4, 1.0)]),
}
for k, v in seeds.items():
    open(os.path.join(out, 'ftp_upload/testsuite', k), 'wb').write(v)
print('ftp_upload seeds:', len(seeds))

# --- parameter defaults files ------------------------------------------------------------------------
defaults = {
    'basic.parm': 'FZ_I8 7\nFZ_I16,300\nFZ_I32\t70000\nFZ_FLT 1.25\n# comment\nMIS_TOTAL 10\n',
    'readonly.parm': 'FZ_I8 3 @READONLY\nFENCE_ENABLE 1\nFENCE_RADIUS 150\nRALLY_TOTAL 2\nFZ_VEC_X 1.5\n',
    'longline.parm': 'FZ_FLT ' + '1' * 140 + '\nFZ_I8 1\n',
}
for k, v in defaults.items():
    open(os.path.join(out, 'param_defaults/testsuite', k), 'w').write(v)
print('param_defaults seeds:', len(defaults))
