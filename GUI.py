import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import datetime

# ----------------- MOCK SERIAL -----------------
class MockSerial:
    def __init__(self):
        self.buffer = []
    @property
    def in_waiting(self):
        return len(self.buffer)
    def write(self, data):
        print("SERIAL OUT ->", data.decode().strip())
        # simulate Arduino response
        if b"MANUAL" in data or b"RECT" in data or b"CIRCLE" in data:
            self.buffer.append(b"BUSY\n")
            self.buffer.append(b"DONE\n")
    def readline(self):
        if self.buffer:
            return self.buffer.pop(0)
        return b""
    
# -------------- ACTUAL SERIAL------------------------
def connect_arduino():
    ports = serial.tools.list_ports.comports()
    if len(ports) == 0:
        messagebox.showerror("Error", "No serial ports found. Connect your Arduino and restart.")
        exit()
    elif len(ports) == 1:
        port = ports[0]
    else:
        # Ask user to select port
        port_list = [f"{p.device} ({p.description})" for p in ports]
        selection = simpledialog.askinteger("Select Port",
                                            "Multiple ports found:\n" + "\n".join([f"{i}: {p}" for i,p in enumerate(port_list)]) + "\nEnter index:")
        if selection is None or selection < 0 or selection >= len(ports):
            messagebox.showerror("Error", "Invalid selection")
            exit()
        port = ports[selection]
    try:
        ser = serial.Serial(port.device, 9600, timeout=0.1)
        messagebox.showinfo("Connected", f"Connected to {port.device}")
        return ser
    except Exception as e:
        messagebox.showerror("Error", f"Could not connect: {e}")
        exit()

ser = connect_arduino() #MockSerial()  # for testing without arduino

arduino_busy = False

# ----------------- GUI -----------------
root = tk.Tk()
root.title("Stage Controller")
root.geometry("600x600")
root.focus_set()

mode_var = tk.StringVar(value="MANUAL")

# Only allow integer input
def only_int(P):
    if P=="":
        return True
    return P.isdigit()
int_vcmd = (root.register(only_int), "%P")

def only_float(P):
    if P == "":
        return True
    try:
        float(P)
        return True
    except ValueError:
        return False
float_vcmd = (root.register(only_float), "%P")

# ----------------- Status -----------------
status_label = ttk.Label(root, text="Idle")
status_label.grid(row=0, column=0, columnspan=2, pady=5)

# ----------------- Mode Selector -----------------
ttk.Label(root, text="Mode:").grid(row=1, column=0, sticky="w")
mode_box = ttk.Combobox(root, textvariable=mode_var, state="readonly",
                        values=["MANUAL","RECT_NXN","RECT_MANUAL","CIRCLE_USER","CIRCLE_AUTO"])
mode_box.grid(row=1, column=1, sticky="w")

# ----------------- Frames -----------------
manual_frame = ttk.LabelFrame(root, text="Manual WASD")
rect_nxn_frame = ttk.LabelFrame(root, text="Rectangle NXN")
rect_manual_frame = ttk.LabelFrame(root, text="Rectangle Manual")
circle_user_frame = ttk.LabelFrame(root, text="Circle Scan (User)")
circle_auto_frame = ttk.LabelFrame(root, text="Circle Scan (Auto)")

# ----------------- Manual -----------------
man_step = tk.DoubleVar(value=0.5)
ttk.Label(manual_frame, text="Step Size (mm):").grid(row=0,column=0)
ttk.Entry(manual_frame, textvariable=man_step, width=10, validate = "key", validatecommand=float_vcmd).grid(row=0,column=1)
ttk.Label(manual_frame, text="Use W/A/S/D to move, Release to STOP", foreground="gray").grid(row=1,column=0,columnspan=2)

# Manual SCAN button
def send_manual_scan():
    if arduino_busy:
        messagebox.showwarning("Wait","Arduino is busy!")
        return
    msg = "MANUAL,SCAN\n"
    ser.write(msg.encode())
    status_label.config(text=msg.strip())

scan_button = ttk.Button(manual_frame, text="SCAN", command=send_manual_scan)
scan_button.grid(row=2,column=0,columnspan=2,pady=5)

def set_center():
    if ser:
        ser.write(b"MANUAL,CENTER\n")
        status_label.config(text="Center set at current position")

center_button = ttk.Button(manual_frame, text="Set Center", command=set_center)
center_button.grid(row=3, column=0, columnspan=2, pady=5)


# ----------------- RECT NXN -----------------
nxn_x = tk.IntVar(value=5)
nxn_y = tk.IntVar(value=5)
ttk.Label(rect_nxn_frame, text="X scans:").grid(row=0,column=0)
ttk.Entry(rect_nxn_frame, textvariable=nxn_x, width=10, validate="key", validatecommand=(int_vcmd, only_int)).grid(row=0,column=1)
ttk.Label(rect_nxn_frame, text="Y scans:").grid(row=1,column=0)
ttk.Entry(rect_nxn_frame, textvariable=nxn_y, width=10, validate="key", validatecommand=(int_vcmd, only_int)).grid(row=1,column=1)

# ----------------- RECT MANUAL -----------------
rect_step = tk.DoubleVar(value=0.2)
rect_total = tk.IntVar(value=20)
ttk.Label(rect_manual_frame, text="Step size (mm):").grid(row=0,column=0)
ttk.Entry(rect_manual_frame, textvariable=rect_step, width=10, validate = "key", validatecommand=float_vcmd).grid(row=0,column=1)
ttk.Label(rect_manual_frame, text="Total scans:").grid(row=1,column=0)
ttk.Entry(rect_manual_frame, textvariable=rect_total, width=10, validate="key", validatecommand=(int_vcmd, only_int)).grid(row=1,column=1)

# ----------------- CIRCLE USER -----------------
circle_idx = tk.IntVar(value=0)
circle_step = tk.DoubleVar(value=0.2)
circle_points = tk.IntVar(value=30)
ttk.Label(circle_user_frame, text="Circle index (0-3):").grid(row=0,column=0)
ttk.Entry(circle_user_frame, textvariable=circle_idx, width=10, validate="key", validatecommand=(int_vcmd, only_int)).grid(row=0,column=1)
ttk.Label(circle_user_frame, text="Step size (mm):").grid(row=1,column=0)
ttk.Entry(circle_user_frame, textvariable=circle_step, width=10, validate = "key", validatecommand=float_vcmd).grid(row=1,column=1)
ttk.Label(circle_user_frame, text="Total points:").grid(row=2,column=0)
ttk.Entry(circle_user_frame, textvariable=circle_points, width=10, validate="key", validatecommand=(int_vcmd, only_int)).grid(row=2,column=1)

# ----------------- CIRCLE AUTO -----------------
circle_auto_idx = tk.IntVar(value=0)
ttk.Label(circle_auto_frame, text="Circle index (0-3):").grid(row=0,column=0)
ttk.Entry(circle_auto_frame, textvariable=circle_auto_idx, width=10, validate="key", validatecommand=(int_vcmd, only_int)).grid(row=0,column=1)

# ----------------- SEND BUTTON -----------------
send_button = ttk.Button(root, text="SEND")

# ----------------- Show Frame -----------------
def show_frame(mode):
    global send_button
    for f in (manual_frame, rect_nxn_frame, rect_manual_frame, circle_user_frame, circle_auto_frame):
        f.grid_forget()
    send_button.grid_forget()
    if mode=="MANUAL":
        manual_frame.grid(row=2,column=0,columnspan=2,padx=10,pady=10,sticky="w")
    elif mode=="RECT_NXN":
        rect_nxn_frame.grid(row=2,column=0,columnspan=2,padx=10,pady=10,sticky="w")
        send_button.grid(row=3,column=0,columnspan=2,pady=5)
    elif mode=="RECT_MANUAL":
        rect_manual_frame.grid(row=2,column=0,columnspan=2,padx=10,pady=10,sticky="w")
        send_button.grid(row=3,column=0,columnspan=2,pady=5)
    elif mode=="CIRCLE_USER":
        circle_user_frame.grid(row=2,column=0,columnspan=2,padx=10,pady=10,sticky="w")
        send_button.grid(row=3,column=0,columnspan=2,pady=5)
    elif mode=="CIRCLE_AUTO":
        circle_auto_frame.grid(row=2,column=0,columnspan=2,padx=10,pady=10,sticky="w")
        send_button.grid(row=3,column=0,columnspan=2,pady=5)

mode_var.trace_add("write", lambda *_: show_frame(mode_var.get()))
show_frame("MANUAL")

# ----------------- Manual WASD -----------------
keys_held = set()
SEND_INTERVAL = 100

def manual_send_loop():
    if mode_var.get()=="MANUAL" and not arduino_busy:
        for key in keys_held:
            step = man_step.get()
            msg = f"MANUAL,{key.upper()},{step}\n"
            ser.write(msg.encode())
            status_label.config(text=msg.strip())
    root.after(SEND_INTERVAL, manual_send_loop)

def on_key_press(event):
    if mode_var.get() != "MANUAL": return
    if event.keysym.lower() in ("w","a","s","d"):
        keys_held.add(event.keysym.lower())

def on_key_release(event):
    if event.keysym.lower() in ("w","a","s","d"):
        keys_held.discard(event.keysym.lower())
        msg = "MANUAL,STOP\n"
        ser.write(msg.encode()) # not needed until fully connected
        status_label.config(text="MANUAL,STOP")

root.bind("<KeyPress>", on_key_press)
root.bind("<KeyRelease>", on_key_release)
root.after(SEND_INTERVAL, manual_send_loop)



# ----------------- Arduino Log -----------------
log_frame = ttk.LabelFrame(root, text="Arduino Log")
log_frame.grid(row=4, column=0, columnspan=2, padx=10, pady=5, sticky="we")
log_text = tk.Text(log_frame, height=10, width=60, state="disabled")
log_text.pack(fill="both", expand=True)
log_text.tag_config("done", foreground="green")
log_text.tag_config("busy", foreground="red")

def log(msg):
    log_text.config(state="normal")
    if msg=="DONE":
        log_text.insert("end", msg+"\n","done")
    elif msg=="BUSY":
        log_text.insert("end", msg+"\n","busy")
    else:
        log_text.insert("end", msg+"\n")
    log_text.see("end")
    log_text.config(state="disabled")

# ----------------- Send Command -----------------
def send_command():
    global arduino_busy
    if arduino_busy:
        messagebox.showwarning("Wait","Arduino is busy!")
        return
    mode = mode_var.get()
    msg = ""
    try:
        if mode=="RECT_NXN":
            msg = f"RECT_NXN,{nxn_x.get()},{nxn_y.get()},0.2\n"
        elif mode=="RECT_MANUAL":
            msg = f"RECT_MANUAL,{rect_total.get()},{rect_step.get()}\n"
        elif mode=="CIRCLE_USER":
            msg = f"CIRCLE,{circle_idx.get()},{circle_step.get()},{circle_points.get()}\n"
        elif mode=="CIRCLE_AUTO":
            msg = f"CIRCLE,{circle_auto_idx.get()},0.2,30\n"
        ser.write(msg.encode())
        status_label.config(text=msg.strip())
        arduino_busy=True
    except Exception as e:
        messagebox.showerror("Error",str(e))

send_button.config(command=send_command)

# ----------------- Arduino Status Check -----------------
def check_arduino():
    global arduino_busy
    while ser.in_waiting:
        msg = ser.readline().decode().strip()
        if msg:
            log(msg)
            if msg=="DONE":
                arduino_busy=False
                status_label.config(text="Arduino Ready")
            elif msg=="BUSY":
                arduino_busy=True
                status_label.config(text="Arduino Busy")
    root.after(100, check_arduino)

root.after(100, check_arduino)
root.mainloop()
