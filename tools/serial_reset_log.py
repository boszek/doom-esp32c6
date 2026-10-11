import serial, sys, time
# Resetuje plytke (RTS) i zapisuje surowe wyjscie przez N sekund. Uzycie: serial_reset_log.py SEK
s = serial.Serial('/dev/ttyACM1', 115200, timeout=0.5)
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
t = time.time()
while time.time() - t < float(sys.argv[1]):
    l = s.readline()
    if l: print(l.decode(errors='replace').rstrip(), flush=True)
