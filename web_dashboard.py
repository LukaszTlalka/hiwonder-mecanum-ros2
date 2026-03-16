#!/usr/bin/env python3
"""
RRC Lite Web Dashboard
- Streams IMU + battery data to browser via SSE
- Motor control via REST API
- Open http://localhost:5000 in browser
"""
import json
import math
import queue
import struct
import threading
import time

import serial
from flask import Flask, Response, jsonify, request

from config import PORT, BAUD

app = Flask(__name__)

# Latest data — updated by reader thread
state = {
    'imu': {'ax': 0, 'ay': 0, 'az': 0, 'gx': 0, 'gy': 0, 'gz': 0},
    'orientation': {'roll': 0, 'pitch': 0, 'yaw': 0},
    'battery_mv': 0,
    'encoders': {'m1': 0, 'm2': 0, 'm3': 0, 'm4': 0},
    'connected': False,
}

# Complementary filter state
_cf = {'roll': 0.0, 'pitch': 0.0, 'yaw': 0.0, 'last_t': None}
_CF_ALPHA = 0.98   # trust gyro 98%, accel 2%

# SSE subscribers
subscribers: list[queue.Queue] = []
subscribers_lock = threading.Lock()

# Serial port (shared)
ser: serial.Serial | None = None
ser_lock = threading.Lock()

# ── Protocol ────────────────────────────────────────────────────────────────

crc8_table = [
    0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
    157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
    35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
    190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
    70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
    219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
    101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
    248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
    140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
    17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
    175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
    50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
    202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
    87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
    233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
    116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
]

def crc8(data: bytes) -> int:
    check = 0
    for b in data:
        check = crc8_table[check ^ b]
    return check & 0xFF

def build_packet(func: int, data: bytes) -> bytes:
    payload = bytes([func, len(data)]) + data
    return b'\xaa\x55' + payload + bytes([crc8(payload)])

def set_motor(motor_id: int, speed: float):
    if ser and ser.is_open:
        data = bytes([0x01, 1]) + struct.pack('<Bf', motor_id - 1, speed)
        with ser_lock:
            ser.write(build_packet(0x03, data))

# ── Serial reader thread ─────────────────────────────────────────────────────

def reader_thread():
    global ser
    while True:
        try:
            s = serial.Serial()
            s.port = PORT
            s.baudrate = BAUD
            s.timeout = 1
            s.xonxoff = False
            s.rtscts = False
            s.dtr = False   # must be set BEFORE open() to prevent bootloader trigger
            s.rts = False
            s.open()
            # Pulse RTS to reset MCU into application (BOOT0=0 since DTR stays low)
            s.rts = True
            time.sleep(0.05)
            s.rts = False
            time.sleep(0.3)   # wait for MCU to boot
            ser = s
            state['connected'] = True
            print(f'Connected to {PORT}')
            try:
                _read_loop(s)
            finally:
                s.close()
        except serial.SerialException as e:
            state['connected'] = False
            ser = None
            print(f'Serial error: {e} — retrying in 2s')
            time.sleep(2)

def _read_loop(s: serial.Serial):
    ps = 'SYNC1'
    func = length = 0
    buf = []
    while True:
        b = s.read(1)
        if not b:
            continue
        b = b[0]
        if ps == 'SYNC1':
            if b == 0xAA:
                ps = 'SYNC2'
        elif ps == 'SYNC2':
            ps = 'FUNC' if b == 0x55 else 'SYNC1'
        elif ps == 'FUNC':
            func = b
            ps = 'LEN'
        elif ps == 'LEN':
            length = b
            buf = []
            ps = 'DATA' if length else 'CRC'
        elif ps == 'DATA':
            buf.append(b)
            if len(buf) >= length:
                ps = 'CRC'
        elif ps == 'CRC':
            frame = bytes([func, length] + buf)
            if crc8(frame) == b:
                _handle_packet(func, bytes(buf))
            ps = 'SYNC1'

def _complementary_filter(ax, ay, az, gx, gy, gz):
    now = time.monotonic()
    dt = now - _cf['last_t'] if _cf['last_t'] else 0.0
    _cf['last_t'] = now
    dt = min(dt, 0.1)  # cap at 100ms to avoid jumps on reconnect

    # Roll/pitch from accelerometer (degrees); accel in g → atan2 ratio is scale-independent
    accel_roll  = math.degrees(math.atan2(ay, az))
    accel_pitch = math.degrees(math.atan2(-ax, math.sqrt(ay*ay + az*az)))

    # Complementary filter — gyro is in deg/s so multiply directly by dt (already degrees)
    _cf['roll']  = _CF_ALPHA * (_cf['roll']  + gx * dt) + (1 - _CF_ALPHA) * accel_roll
    _cf['pitch'] = _CF_ALPHA * (_cf['pitch'] + gy * dt) + (1 - _CF_ALPHA) * accel_pitch

    # Yaw: gyro only (no magnetometer → drifts over time)
    _cf['yaw'] = (_cf['yaw'] + gz * dt) % 360

    return round(_cf['roll'], 2), round(_cf['pitch'], 2), round(_cf['yaw'], 2)

def _handle_packet(func: int, payload: bytes):
    if func == 0x07 and len(payload) == 24:  # IMU
        ax, ay, az, gx, gy, gz = struct.unpack('<6f', payload)
        state['imu'] = {
            'ax': round(ax, 4), 'ay': round(ay, 4), 'az': round(az, 4),
            'gx': round(gx, 4), 'gy': round(gy, 4), 'gz': round(gz, 4),
        }
        roll, pitch, yaw = _complementary_filter(ax, ay, az, gx, gy, gz)
        state['orientation'] = {'roll': roll, 'pitch': pitch, 'yaw': yaw}
        _broadcast({'type': 'imu', **state['imu'],
                    'roll': roll, 'pitch': pitch, 'yaw': yaw})

    elif func == 0x00 and len(payload) >= 3 and payload[0] == 0x04:  # battery
        mv = struct.unpack('<H', payload[1:3])[0]
        state['battery_mv'] = mv
        _broadcast({'type': 'battery', 'mv': mv, 'v': round(mv / 1000, 2)})

    elif func == 0x00 and len(payload) >= 3 and payload[0] == 0xFE:  # IMU WHO_AM_I probe
        i2c_ret  = payload[1]
        who_am_i = payload[2]
        ok = (i2c_ret == 0 and who_am_i == 0x05)
        msg = (f"IMU WHO_AM_I: i2c={'OK' if i2c_ret == 0 else f'ERR({i2c_ret})'}, "
               f"chip=0x{who_am_i:02X} ({'QMI8658 OK' if ok else 'WRONG — expected 0x05'})")
        print(f'[IMU DEBUG] {msg}', flush=True)
        _broadcast({'type': 'imu_debug', 'msg': msg, 'ok': ok})

    elif func == 0x00 and len(payload) >= 2 and payload[0] == 0xFF:  # IMU data-ready timeout
        msg = 'IMU data-ready timeout — QMI8658 not responding'
        print(f'[IMU DEBUG] {msg}', flush=True)
        _broadcast({'type': 'imu_debug', 'msg': msg, 'ok': False})

    elif func == 0x0A and len(payload) == 32:  # encoder ticks (PACKET_FUNC_ENCODER)
        m1, m2, m3, m4 = struct.unpack('<4q', payload)
        now = time.monotonic()
        prev = state['encoders']
        prev_t = state.get('encoders_t', now)
        dt = now - prev_t
        if dt > 0 and prev['m1'] != 0:
            delta = {
                'd1': round((m1 - prev['m1']) / dt),
                'd2': round((m2 - prev['m2']) / dt),
                'd3': round((m3 - prev['m3']) / dt),
                'd4': round((m4 - prev['m4']) / dt),
            }
        else:
            delta = {'d1': 0, 'd2': 0, 'd3': 0, 'd4': 0}
        state['encoders'] = {'m1': m1, 'm2': m2, 'm3': m3, 'm4': m4}
        state['encoders_t'] = now
        _broadcast({'type': 'encoders', 'm1': m1, 'm2': m2, 'm3': m3, 'm4': m4, **delta})

def _broadcast(data: dict):
    msg = f"data: {json.dumps(data)}\n\n"
    with subscribers_lock:
        dead = []
        for q in subscribers:
            try:
                q.put_nowait(msg)
            except queue.Full:
                dead.append(q)
        for q in dead:
            subscribers.remove(q)

# ── Flask routes ─────────────────────────────────────────────────────────────

@app.route('/')
def index():
    return Response(HTML, mimetype='text/html')

@app.route('/stream')
def stream():
    q = queue.Queue(maxsize=100)
    with subscribers_lock:
        subscribers.append(q)

    def generate():
        # Send current state immediately on connect
        enc = state['encoders']
        yield f"data: {json.dumps({'type': 'state', **state['imu'], 'battery_mv': state['battery_mv'], 'm1': enc['m1'], 'm2': enc['m2'], 'm3': enc['m3'], 'm4': enc['m4'], 'd1': 0, 'd2': 0, 'd3': 0, 'd4': 0, 'connected': state['connected']})}\n\n"
        try:
            while True:
                try:
                    msg = q.get(timeout=15)
                    yield msg
                except queue.Empty:
                    yield ': keepalive\n\n'
        except GeneratorExit:
            pass
        finally:
            with subscribers_lock:
                if q in subscribers:
                    subscribers.remove(q)

    return Response(generate(), mimetype='text/event-stream',
                    headers={'Cache-Control': 'no-cache', 'X-Accel-Buffering': 'no'})

@app.route('/motor', methods=['POST'])
def motor():
    data = request.get_json()
    motor_id = int(data.get('id', 1))
    speed = float(data.get('speed', 0.0))
    if 1 <= motor_id <= 4 and -10.0 <= speed <= 10.0:
        set_motor(motor_id, speed)
        return jsonify({'ok': True, 'id': motor_id, 'speed': speed})
    return jsonify({'ok': False, 'error': 'invalid params'}), 400

@app.route('/stop', methods=['POST'])
def stop_all():
    for i in range(1, 5):
        set_motor(i, 0.0)
    return jsonify({'ok': True})

@app.route('/reset_yaw', methods=['POST'])
def reset_yaw():
    _cf['yaw'] = 0.0
    return jsonify({'ok': True})

@app.route('/status')
def status():
    return jsonify({**state['imu'],
                    'battery_mv': state['battery_mv'],
                    'connected': state['connected']})

# ── HTML / JS frontend ───────────────────────────────────────────────────────

HTML = '''<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="UTF-8">
<title>RRC Lite Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: monospace; background: #0d0d0d; color: #e0e0e0; padding: 20px; }
  h1 { color: #4af; margin-bottom: 20px; font-size: 1.4em; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; max-width: 900px; }
  .card { background: #1a1a1a; border: 1px solid #333; border-radius: 8px; padding: 16px; }
  .card h2 { color: #4af; font-size: 0.9em; margin-bottom: 12px; text-transform: uppercase; letter-spacing: 1px; }
  .value { font-size: 1.8em; color: #fff; }
  .unit { font-size: 0.7em; color: #888; margin-left: 4px; }
  .row { display: flex; justify-content: space-between; align-items: center; margin: 6px 0; }
  .label { color: #888; font-size: 0.85em; }
  .val { color: #4f4; font-size: 1em; min-width: 80px; text-align: right; }
  .val.neg { color: #f84; }
  .status { display: inline-block; width: 10px; height: 10px; border-radius: 50%; background: #f44; margin-right: 8px; }
  .status.ok { background: #4f4; }
  .motor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
  .motor-btn { padding: 10px; border: 1px solid #444; border-radius: 6px; background: #222;
               color: #fff; cursor: pointer; font-family: monospace; font-size: 0.85em; }
  .motor-btn:hover { background: #333; border-color: #4af; }
  .stop-btn { grid-column: 1/-1; background: #400; border-color: #f44; color: #f88; }
  .stop-btn:hover { background: #600; }
  input[type=range] { width: 100%; margin: 8px 0; accent-color: #4af; }
  .speed-label { color: #4af; font-size: 1.1em; }
  .bar-wrap { background: #111; border-radius: 4px; height: 8px; overflow: hidden; margin-top: 2px; }
  .bar { height: 100%; border-radius: 4px; transition: width 0.1s; }
  .bar-pos { background: #4af; }
  .bar-neg { background: #f84; }
  #log { font-size: 0.75em; color: #666; margin-top: 8px; max-height: 80px; overflow-y: auto; }
</style>
</head>
<body>
<h1><span id="dot" class="status"></span>RRC Lite Dashboard</h1>
<div class="grid">

  <div class="card">
    <h2>Akcelerometr (g)</h2>
    <div class="row"><span class="label">ax</span><span class="val" id="ax">—</span></div>
    <div class="bar-wrap"><div class="bar bar-pos" id="bar-ax" style="width:50%"></div></div>
    <div class="row"><span class="label">ay</span><span class="val" id="ay">—</span></div>
    <div class="bar-wrap"><div class="bar bar-pos" id="bar-ay" style="width:50%"></div></div>
    <div class="row"><span class="label">az</span><span class="val" id="az">—</span></div>
    <div class="bar-wrap"><div class="bar bar-pos" id="bar-az" style="width:50%"></div></div>
  </div>

  <div class="card">
    <h2>Żyroskop (°/s)</h2>
    <div class="row"><span class="label">gx</span><span class="val" id="gx">—</span></div>
    <div class="bar-wrap"><div class="bar bar-pos" id="bar-gx" style="width:50%"></div></div>
    <div class="row"><span class="label">gy</span><span class="val" id="gy">—</span></div>
    <div class="bar-wrap"><div class="bar bar-pos" id="bar-gy" style="width:50%"></div></div>
    <div class="row"><span class="label">gz</span><span class="val" id="gz">—</span></div>
    <div class="bar-wrap"><div class="bar bar-pos" id="bar-gz" style="width:50%"></div></div>
  </div>

  <div class="card">
    <h2>Orientacja (filtr komplementarny)</h2>
    <div style="display:flex;gap:16px;align-items:center">
      <canvas id="adi" width="110" height="110"></canvas>
      <div style="flex:1">
        <div class="row"><span class="label">Roll</span><span class="val" id="roll">—</span><span class="label"> °</span></div>
        <div class="row"><span class="label">Pitch</span><span class="val" id="pitch">—</span><span class="label"> °</span></div>
        <div class="row"><span class="label">Yaw ⚠drift</span><span class="val" id="yaw">—</span><span class="label"> °</span></div>
        <button class="motor-btn" style="margin-top:8px;width:100%" onclick="resetYaw()">Reset Yaw</button>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>Zasilanie</h2>
    <div class="row">
      <span class="label">Napięcie</span>
      <span class="value" id="batt">—<span class="unit">V</span></span>
    </div>
  </div>

  <div class="card" id="imu-debug-card" style="display:none;grid-column:1/-1">
    <h2>IMU Debug</h2>
    <div id="imu-debug-log" style="font-size:0.8em;color:#fa4;white-space:pre-wrap"></div>
  </div>

  <div class="card">
    <h2>Enkodery</h2>
    <div style="display:grid;grid-template-columns:2em 1fr 5em 5em;gap:4px 8px;align-items:center;font-size:0.85em">
      <span></span><span style="color:#555;font-size:0.8em">prędkość (tiki/s)</span><span style="color:#555;font-size:0.8em;text-align:right">tiki</span><span></span>
      <span class="label">M1</span>
      <div class="bar-wrap" style="height:14px"><div class="bar bar-pos" id="bar-enc-m1" style="width:50%"></div></div>
      <span class="val" id="enc-m1" style="text-align:right">—</span>
      <span class="val" id="enc-d1" style="color:#4af;text-align:right">—</span>
      <span class="label">M2</span>
      <div class="bar-wrap" style="height:14px"><div class="bar bar-pos" id="bar-enc-m2" style="width:50%"></div></div>
      <span class="val" id="enc-m2" style="text-align:right">—</span>
      <span class="val" id="enc-d2" style="color:#4af;text-align:right">—</span>
      <span class="label">M3</span>
      <div class="bar-wrap" style="height:14px"><div class="bar bar-pos" id="bar-enc-m3" style="width:50%"></div></div>
      <span class="val" id="enc-m3" style="text-align:right">—</span>
      <span class="val" id="enc-d3" style="color:#4af;text-align:right">—</span>
      <span class="label">M4</span>
      <div class="bar-wrap" style="height:14px"><div class="bar bar-pos" id="bar-enc-m4" style="width:50%"></div></div>
      <span class="val" id="enc-m4" style="text-align:right">—</span>
      <span class="val" id="enc-d4" style="color:#4af;text-align:right">—</span>
    </div>
  </div>

  <div class="card">
    <h2>Sterowanie silnikiem</h2>
    <div class="row">
      <span class="label">Silnik</span>
      <select id="motor-id" style="background:#222;color:#fff;border:1px solid #444;padding:4px;font-family:monospace">
        <option>1</option><option>2</option><option>3</option><option>4</option>
      </select>
    </div>
    <div class="row" style="margin-top:10px">
      <span class="label">Prędkość</span>
      <span class="speed-label"><span id="speed-val">0.0</span> m/s</span>
    </div>
    <input type="range" id="speed" min="-3" max="3" step="0.1" value="0">
    <div class="motor-grid" style="margin-top:10px">
      <button class="motor-btn" onclick="sendMotor()">Wyslij</button>
      <button class="motor-btn" onclick="setSpeed(0);sendMotor()">Stop M</button>
      <button class="motor-btn stop-btn" onclick="stopAll()">STOP WSZYSTKIE</button>
    </div>
    <div id="log"></div>
  </div>

</div>

<script>
  const $ = id => document.getElementById(id);

  // SSE
  const es = new EventSource('/stream');
  es.onmessage = e => {
    const d = JSON.parse(e.data);
    if (d.type === 'imu' || d.type === 'state') updateIMU(d);
    if (d.type === 'battery' || d.type === 'state') updateBattery(d);
    if (d.type === 'encoders' || d.type === 'state') updateEncoders(d);
    if (d.connected !== undefined) setConnected(d.connected);
    if (d.type === 'imu_debug') showIMUDebug(d);
  };

  const ENC_MAX_TICKS_S = 3000; // bar full-scale: ticks/s at max speed
  function updateEncoders(d) {
    [1,2,3,4].forEach(i => {
      const tick = d['m'+i];
      const vel  = d['d'+i];
      if (tick !== undefined) $('enc-m'+i).textContent = tick;
      if (vel  !== undefined) {
        $('enc-d'+i).textContent = (vel >= 0 ? '+' : '') + vel;
        // bar: centre=50%, positive → right, negative → left
        const pct = Math.min(100, Math.max(0, (vel / ENC_MAX_TICKS_S + 1) / 2 * 100));
        const bar = $('bar-enc-m'+i);
        bar.style.width = Math.abs(pct - 50) + '%';
        bar.style.marginLeft = vel >= 0 ? '50%' : (pct + '%');
        bar.className = 'bar ' + (vel >= 0 ? 'bar-pos' : 'bar-neg');
      }
    });
  }
  es.onopen = () => setConnected(true);
  es.onerror = () => setConnected(false);

  function setConnected(ok) {
    $('dot').className = 'status' + (ok ? ' ok' : '');
  }

  function updateIMU(d) {
    ['ax','ay','az','gx','gy','gz'].forEach(k => {
      if (d[k] === undefined) return;
      const v = d[k];
      const el = $(k);
      el.textContent = v.toFixed(3);
      el.className = 'val' + (v < 0 ? ' neg' : '');
      const pct = Math.min(100, Math.max(0, (v + 10) / 20 * 100));
      $('bar-' + k).style.width = pct + '%';
    });
    if (d.roll !== undefined) updateOrientation(d.roll, d.pitch, d.yaw);
  }

  // ── Attitude Indicator ───────────────────────────────────────────────────
  const canvas = $('adi');
  const ctx = canvas.getContext('2d');
  const CX = canvas.width / 2, CY = canvas.height / 2, R = CX - 4;

  function updateOrientation(roll, pitch, yaw) {
    $('roll').textContent  = roll.toFixed(1);
    $('pitch').textContent = pitch.toFixed(1);
    $('yaw').textContent   = yaw.toFixed(1);
    ['roll','pitch','yaw'].forEach(k => {
      const v = parseFloat($(k).textContent);
      $(k).className = 'val' + (v < 0 ? ' neg' : '');
    });
    drawADI(roll, pitch);
  }

  function drawADI(roll, pitch) {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.save();
    ctx.beginPath();
    ctx.arc(CX, CY, R, 0, Math.PI * 2);
    ctx.clip();

    // Rotate by roll
    ctx.translate(CX, CY);
    ctx.rotate(-roll * Math.PI / 180);

    // Pitch offset: 1° = 1.5px
    const py = pitch * 1.5;

    // Sky
    ctx.fillStyle = '#1a4a8a';
    ctx.fillRect(-R, -R + py, R * 2, R);
    // Ground
    ctx.fillStyle = '#6b3a1f';
    ctx.fillRect(-R, py, R * 2, R);
    // Horizon line
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(-R, py);
    ctx.lineTo(R, py);
    ctx.stroke();

    // Pitch lines
    ctx.strokeStyle = 'rgba(255,255,255,0.5)';
    ctx.lineWidth = 1;
    ctx.font = '8px monospace';
    ctx.fillStyle = '#fff';
    [-20,-10,10,20].forEach(deg => {
      const y = py + deg * 1.5;
      const w = deg % 20 === 0 ? 24 : 16;
      ctx.beginPath();
      ctx.moveTo(-w, y); ctx.lineTo(w, y);
      ctx.stroke();
      ctx.fillText(deg > 0 ? '+'+deg : deg, w + 2, y + 3);
    });

    ctx.restore();

    // Fixed overlay — cross
    ctx.strokeStyle = '#ff0';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(CX - 28, CY); ctx.lineTo(CX - 10, CY);
    ctx.moveTo(CX + 10, CY); ctx.lineTo(CX + 28, CY);
    ctx.moveTo(CX, CY - 5);  ctx.lineTo(CX, CY + 5);
    ctx.stroke();

    // Border
    ctx.strokeStyle = '#444';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(CX, CY, R, 0, Math.PI * 2);
    ctx.stroke();
  }

  function showIMUDebug(d) {
    const card = $('imu-debug-card');
    const log  = $('imu-debug-log');
    card.style.display = '';
    const color = d.ok ? '#4f4' : '#fa4';
    log.innerHTML = `<span style="color:${color}">${d.msg}</span>\n` + log.innerHTML;
  }

  function resetYaw() {
    fetch('/reset_yaw', {method: 'POST'});
  }

  drawADI(0, 0);

  function updateBattery(d) {
    const v = d.v ?? (d.battery_mv / 1000);
    if (v) $('batt').innerHTML = v.toFixed(2) + '<span class="unit">V</span>';
  }

  // Motor control
  $('speed').addEventListener('input', () => {
    $('speed-val').textContent = parseFloat($('speed').value).toFixed(1);
  });

  function setSpeed(v) {
    $('speed').value = v;
    $('speed-val').textContent = v.toFixed(1);
  }

  function log(msg) {
    const el = $('log');
    el.textContent = msg + '\\n' + el.textContent;
  }

  function sendMotor() {
    const id = parseInt($('motor-id').value);
    const speed = parseFloat($('speed').value);
    fetch('/motor', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id, speed})
    }).then(r => r.json()).then(d => log(`M${id} → ${speed} m/s`));
  }

  function stopAll() {
    fetch('/stop', {method: 'POST'})
      .then(() => { setSpeed(0); log('STOP wszystkie silniki'); });
  }
</script>
</body>
</html>
'''

# ── Main ─────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    threading.Thread(target=reader_thread, daemon=True).start()
    print('Dashboard: http://localhost:5000')
    app.run(host='0.0.0.0', port=5000, threaded=True)
