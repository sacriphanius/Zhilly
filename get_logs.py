import serial, time
try:
    s = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
    s.dtr = False; s.rts = False; time.sleep(0.1)
    s.dtr = True; s.rts = True; time.sleep(0.1)
    s.dtr = False; s.rts = False
    start = time.time()
    with open("boot.log", "w") as f:
        while time.time() - start < 8:
            l = s.readline()
            if l: f.write(l.decode("utf-8", "ignore"))
except Exception as e:
    open("boot.log", "w").write(str(e))
