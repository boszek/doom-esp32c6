import serial, time, sys
dur = float(sys.argv[1]) if len(sys.argv) > 1 else 20
t0 = time.time()
s = None
skip = (b'load:', b'ESP-ROM', b'Build', b'mode:', b'entry', b'SPIWP')
while time.time() - t0 < dur:
    try:
        if s is None:
            s = serial.Serial('/dev/ttyACM1', 115200, timeout=0.5)
        l = s.readline()
        if l and not l.startswith(skip):
            print(l.decode(errors='replace').rstrip(), flush=True)
    except Exception as e:
        s = None
        time.sleep(0.3)
