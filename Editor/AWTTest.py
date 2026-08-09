import tkinter as tk

root = tk.Tk()
root.title("Python Tkinter GUI Test")
root.geometry("300x200")

label = tk.Label(root, text="Hello from Python GUI!", font=("Arial", 14))
label.pack(pady=50)

root.mainloop()
