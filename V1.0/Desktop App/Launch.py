"""
ESP32 Cheap Yellow Display Electronic Shelf Label with Google Firebase
Distributed under the MIT License
© Copyright Maxim Bortnikov 2024
For more information please visit
https://sourceforge.net/projects/esp32-cyd-esl-with-firebase/
https://github.com/Northstrix/ESP32-Cheap-Yellow-Display-Electronic-Shelf-Label-with-Google-Firebase
Required libraries:
https://github.com/mobizt/Firebase-ESP-Client
https://github.com/Northstrix/AES_in_CBC_mode_for_microcontrollers
https://github.com/intrbiz/arduino-crypto
"""
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
import firebase_admin
from firebase_admin import db, credentials
import sv_ttk
import sqlite3
import random
import string
import numpy as np
import os
import time
import hashlib
import secrets

# Connect to the database (creates it if it doesn't exist)
conn = sqlite3.connect('esl.db')
# Create a cursor object
cursor = conn.cursor()
cursor.execute('''CREATE TABLE IF NOT EXISTS ESLs 
                  (id text, label TEXT, encryption_key Text)''')

selected_image_path = ""

string_for_data = ""
array_for_CBC_mode = bytearray(16)
back_aes_key = bytearray(32)
decract = 0

aes_key = bytearray([
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
])

'''
def back_aes_k():
    global back_aes_key
    back_aes_key = bytearray(aes_key)

def rest_aes_k():
    global aes_key
    aes_key = bytearray(back_aes_key)
'''
def incr_aes_key():
    global aes_key
    i = 15
    while i >= 0:
        if aes_key[i] == 255:
            aes_key[i] = 0
            i -= 1
        else:
            aes_key[i] += 1
            break

def encrypt_iv_for_aes():
    iv = bytearray(secrets.token_bytes(16))
    global array_for_CBC_mode
    array_for_CBC_mode = iv[:]
    encrypt_with_aes(iv)

def encrypt_with_aes(to_be_encrypted):
    global string_for_data
    global decract
    to_be_encrypted = bytearray(to_be_encrypted)  # Convert to mutable bytearray
    if decract > 0:
        for i in range(16):
            to_be_encrypted[i] ^= array_for_CBC_mode[i]
            
    cipher = AES.new(aes_key, AES.MODE_ECB)
    encrypted_data = cipher.encrypt(pad(to_be_encrypted, AES.block_size))
    incr_aes_key()
    if decract > 0:
        for i in range(16):
            if i < 16:
                array_for_CBC_mode[i] = int(encrypted_data[i])
    
    for i in range(16):
        if encrypted_data[i] < 16:
            string_for_data += "0"
        string_for_data += hex(encrypted_data[i])[2:]
    
    decract += 11

class App(ttk.Frame):
    def __init__(self, parent):
        ttk.Frame.__init__(self)

        db_url_file_name = open("db_url.txt", "r")
        db_url = db_url_file_name.read()
        db_url_file_name.close()
        cred = credentials.Certificate("firebase key.json")
        firebase_admin.initialize_app(cred, {"databaseURL": db_url})

        # Make the app responsive
        for index in [0, 1, 2]:
            self.columnconfigure(index=index, weight=1)
            self.rowconfigure(index=index, weight=1)

        # Create widgets :)
        self.setup_widgets()
        self.refresh_treeview()

    def setup_widgets(self):
        # Create a Frame for input widgets
        self.widgets_frame = ttk.Frame(self, padding=(0, 0, 0, 10))
        self.widgets_frame.grid(
            row=0, column=1, padx=10, pady=(30, 10), sticky="nsew", rowspan=3
        )
        self.widgets_frame.columnconfigure(index=0, weight=1)

        # Button to select image
        self.accentbutton = ttk.Button(
            self.widgets_frame, text="Select Image For ESL", style="Accent.TButton", command=self.select_image
        )
        self.accentbutton.grid(row=0, column=0, padx=5, pady=10, sticky="nsew")
        
        self.button = ttk.Button(self.widgets_frame, text="Upload Image To Cloud", command=self.upload_image)
        self.button.grid(row=1, column=0, padx=5, pady=10, sticky="nsew")
        
        self.accentbutton = ttk.Button(
            self.widgets_frame, text="Edit ESL Settings", style="Accent.TButton", command=self.edit_esl_settings
        )
        self.accentbutton.grid(row=2, column=0, padx=5, pady=10, sticky="nsew")
        
        self.button = ttk.Button(self.widgets_frame, text="Remove ESL From DB", command=self.remove_esl)
        self.button.grid(row=3, column=0, padx=5, pady=10, sticky="nsew")

        self.accentbutton = ttk.Button(
            self.widgets_frame, text="About", style="Accent.TButton"
        )
        self.accentbutton.grid(row=4, column=0, padx=5, pady=10, sticky="nsew")
        
        # Panedwindow
        self.paned = ttk.PanedWindow(self)
        self.paned.grid(row=0, column=2, pady=(25, 5), sticky="nsew", rowspan=3)

        # Pane #1
        self.pane_1 = ttk.Frame(self.paned, padding=5)
        self.paned.add(self.pane_1, weight=1)

        # Scrollbar
        self.scrollbar = ttk.Scrollbar(self.pane_1)
        self.scrollbar.pack(side="right", fill="y")

        # Treeview
        self.treeview = ttk.Treeview(
            self.pane_1,
            selectmode="browse",
            yscrollcommand=self.scrollbar.set,
            columns=(1, ),
            height=10,
        )
        self.treeview.pack(expand=True, fill="both")
        self.scrollbar.config(command=self.treeview.yview)

        # Treeview columns
        self.treeview.column("#0", anchor="w", width=100)
        self.treeview.column(1, anchor="w", width=340)

        # Treeview headings
        self.treeview.heading("#0", text="  ID", anchor="w")
        self.treeview.heading(1, text="  Label", anchor="w")

        # Notebook, pane #2
        self.pane_2 = ttk.Frame(self.paned, padding=5)
        self.paned.add(self.pane_2, weight=3)

        # Notebook, pane #2
        self.notebook = ttk.Notebook(self.pane_2)
        self.notebook.pack(fill="both", expand=True)

        # Tab #1
        self.tab_1 = ttk.Frame(self.notebook)
        for index in [0, 1]:
            self.tab_1.columnconfigure(index=index, weight=1)
            self.tab_1.rowconfigure(index=index, weight=1)
        self.notebook.add(self.tab_1, text="ESL Image")
        
        # Create a Canvas on Tab #1
        self.canvas = tk.Canvas(self.tab_1, width=320, height=240, highlightthickness=0, borderwidth=0)
        self.canvas.pack()

        # Tab #2
        self.tab_2 = ttk.Frame(self.notebook)
        self.notebook.add(self.tab_2, text="Add ESL")
        
        # Labels for entries
        label_1 = ttk.Label(self.tab_2, text=" Label: ")
        label_1.grid(row=0, column=0, padx=5, pady=5, sticky="e")

        label_2 = ttk.Label(self.tab_2, text=" ID: ")
        label_2.grid(row=1, column=0, padx=5, pady=5, sticky="e")

        label_3 = ttk.Label(self.tab_2, text=" Encryption Key: ")
        label_3.grid(row=2, column=0, padx=5, pady=5, sticky="e")

        # Entries
        self.entry_1 = ttk.Entry(self.tab_2)
        self.entry_1.grid(row=0, column=1, padx=5, pady=5, sticky="ew")

        self.entry_2 = ttk.Entry(self.tab_2)
        self.entry_2.grid(row=1, column=1, padx=5, pady=5, sticky="ew")

        self.entry_3 = ttk.Entry(self.tab_2)
        self.entry_3.grid(row=2, column=1, padx=5, pady=5, sticky="ew")

        # Buttons
        button_clear = ttk.Button(self.tab_2, text="Clear", command=self.clear_fields)
        button_clear.grid(row=3, column=0, padx=5, pady=5, sticky="ew")

        button_add = ttk.Button(self.tab_2, text="Add", style="Accent.TButton", command=self.on_add_button_click)
        button_add.grid(row=3, column=1, padx=5, pady=5, sticky="ew")

        button_generate = ttk.Button(self.tab_2, text="Generate", style="Accent.TButton", command=self.on_generate_button_click)
        button_generate.grid(row=1, column=2, padx=5, pady=5, sticky="w")

        # Bind treeview selection event
        self.treeview.bind("<<TreeviewSelect>>", self.on_treeview_select)

    def on_treeview_select(self, event):
        global selected_image_path
        # Get the selected item's ID
        selected_items = self.treeview.selection()
        if selected_items:
            selected_id = self.treeview.item(selected_items[0], "text")
            # Check if the corresponding image file exists
            image_file_path = os.path.join("images", f"{selected_id}.png")
            if os.path.exists(image_file_path):
                selected_image_path = image_file_path
                # Load and display the image on the canvas
                image = Image.open(image_file_path)
                image.thumbnail((320, 240))
                photo = ImageTk.PhotoImage(image)
                self.canvas.create_image(0, 0, anchor=tk.NW, image=photo)
                self.canvas.image = photo  # Keep a reference to prevent garbage collection
            else:
                # Fill the canvas with black color
                self.canvas.create_rectangle(0, 0, 320, 240, fill="black")
                selected_image_path = None
        else:
            # No item selected, fill the canvas with black color
            self.canvas.create_rectangle(0, 0, 320, 240, fill="black")
            selected_image_path = None

    def select_image(self):
        global selected_image_path
        selected_items = self.treeview.selection()
        if selected_items:
            file_path = filedialog.askopenfilename(filetypes=[("Image files", "*.png;*.jpg;*.jpeg;*.gif")])
            if file_path:
                selected_image_path = file_path
                image = Image.open(file_path)
                # Resize image to fit canvas
                image.thumbnail((320, 240))
                photo = ImageTk.PhotoImage(image)
                self.canvas.create_image(0, 0, anchor=tk.NW, image=photo)
                self.canvas.image = photo  # Keep a reference to prevent garbage collection
        else:
            messagebox.showwarning("Warning", "Select an ESL to continue.")

    def upload_image(self):
        selected_items = self.treeview.selection()
        if selected_items:
            if not selected_image_path:
                messagebox.showwarning("Warning", "No image is loaded to the canvas.")
                return
            
            global aes_key
            cursor.execute("SELECT encryption_key FROM ESLs WHERE id=?", (self.treeview.item(selected_items[0], "text"),))
            encryption_key_hex = cursor.fetchone()[0]
            # Convert encryption key from hexadecimal string to bytes
            encryption_key_bytes = bytes.fromhex(encryption_key_hex)
            # Replace data in aes_key with encryption_key_bytes
            aes_key[:len(encryption_key_bytes)] = encryption_key_bytes
            # Open the selected image
            img = Image.open(selected_image_path)
            width, height = img.size[:2]
            px = np.array(img)
            px_bytes = px.tobytes()
            # Hash the bytes using SHA-256
            hash_sha256 = hashlib.sha256(px_bytes).hexdigest()
            # Print the hash
            #print(self.treeview.item(selected_items[0])['text'])
            #print(hash_sha256)
            print("Encrypting the image and sending it to the Firebase...")
            print("Please, wait for a while")
            db.reference("/").update({self.treeview.item(selected_items[0])['text'] : hash_sha256})
            byteList = bytearray()
            byteList.clear()
            #Print each pixel to the console
            for i in range(height):
                global string_for_data
                global decract
                if i % 3 == 0:
                    string_for_data = ""
                    decract = 0
                    encrypt_iv_for_aes()
                for j in range(width):
                    pixel = px[i, j]
                    # Extract RGB components
                    r = pixel[0]
                    g = pixel[1]
                    b = pixel[2]
                    # Convert to 565 format
                    r_565 = (r >> 3) & 0x1F
                    g_565 = (g >> 2) & 0x3F
                    b_565 = (b >> 3) & 0x1F
                    # Combine RGB components into a single 16-bit value
                    color_565 = (r_565 << 11) | (g_565 << 5) | b_565
                    byte_1 = color_565 & 0xFF
                    byte_2 = (color_565 >> 8) & 0xFF
                    #print(i, j, color_565, byte_1, byte_2)
                    byteList.append(byte_1)
                    byteList.append(byte_2)
                    if len(byteList) == 16:
                        # Print all elements to the serial terminal
                        #for byte in byteList:
                            #print(hex(byte), end=' ')
                        encrypt_with_aes(byteList)
                        byteList.clear()
                    if len(string_for_data) == 3872:
                        #print(self.treeview.item(selected_items[0])['text'] + str(i))
                        #print(string_for_data)
                        db.reference("/").update({self.treeview.item(selected_items[0])['text'] + str(i) : string_for_data})
                        print("Progress: " + '{0:.2f}'.format(((i + 1)/ 240) * 100) + "%")
                        aes_key[:len(encryption_key_bytes)] = encryption_key_bytes
                    
            
            img_resized = img.resize((320, 240))  # Resize image to fit 320x240 bitmap
            selected_id = self.treeview.item(selected_items[0], "text")
            img_resized.save(os.path.join("images", f"{selected_id}.png"), format="PNG")
        else:
            messagebox.showwarning("Warning", "Select an ESL to continue.")

    def generate_random_filename(self):
        # Generate a random filename of length 10 consisting of numbers, lowercase, and uppercase letters
        characters = string.ascii_letters + string.digits
        return ''.join(random.choice(characters) for i in range(10))

    def edit_esl_settings(self):
        selected_items = self.treeview.selection()
        if selected_items:
            selected_id = self.treeview.item(selected_items[0], "text")
            selected_label = self.treeview.item(selected_items[0], "values")[0]

            # Open a new window for editing
            edit_window = EditForm(self, selected_id, selected_label)
            self.wait_window(edit_window)  # Wait for the edit window to close
        else:
            messagebox.showwarning("Warning", "Select an ESL to continue.")

    def remove_esl(self):
        selected_items = self.treeview.selection()
        if selected_items:
            # Extract the ID of the selected item
            selected_id = self.treeview.item(selected_items[0], "text")
            # Ask for confirmation
            confirmation = messagebox.askyesno("Confirmation", "Are you sure you want to remove this record from the database?")
            
            if confirmation:
                # Delete the record from the database corresponding to the selected ID
                self.delete_record_from_database(selected_id)
                # Refresh the treeview
                self.refresh_treeview()
            else:
                messagebox.showinfo("Operation Cancelled", "Operation was cancelled by user.")
        else:
            messagebox.showwarning("Warning", "Select an ESL to continue.")
            
    def delete_record_from_database(self, id_value):
        # Delete the record from the database based on the provided ID
        cursor.execute("DELETE FROM ESLs WHERE id=?", (id_value,))
        # Commit the transaction
        conn.commit()

    def clear_treeview(self):
        # Clear all items from the treeview
        self.treeview.delete(*self.treeview.get_children())
        
    def add_to_treeview(self, id_value, label_value):
        self.treeview.insert("", "end", text=id_value, values=(label_value,))
        
    def clear_fields(self):
        # Clear all entry fields
        self.entry_1.delete(0, tk.END)
        self.entry_2.delete(0, tk.END)
        self.entry_3.delete(0, tk.END)

    def on_generate_button_click(self):
        # Generate a random ID
        generated_id = self.generate_random_id()
        # Check if the generated ID already exists in the database
        while self.check_id_existence(generated_id):
            generated_id = self.generate_random_id()
            # If record with the generated ID already exists, generate a new ID and check again
            
        # Set the generated ID to the ID entry field
        self.entry_2.delete(0, tk.END)
        self.entry_2.insert(0, generated_id)

    def generate_random_id(self):
        # Generate a random ID of length 10 consisting of numbers, lowercase, and uppercase letters
        characters = string.ascii_letters + string.digits
        generated_id = ''.join(random.choice(characters) for i in range(8))
        return generated_id

    def check_id_existence(self, id_value):
        # Check if the ID already exists in the database
        cursor.execute("SELECT id FROM ESLs WHERE id=?", (id_value,))
        existing_id = cursor.fetchone()
        return existing_id is not None
    
    def on_add_button_click(self):
        # Extract data from the entry fields
        label_value = self.entry_1.get()
        id_value = self.entry_2.get()
        encryption_key_value = self.entry_3.get()
        allowed_chars = string.digits + string.ascii_letters
        encryption_key_value = ''.join(char for char in encryption_key_value if char in allowed_chars)
    
        # Check if the label_value is empty
        if not label_value.strip():
            tk.messagebox.showwarning("Warning", "Label value cannot be empty.")
            return

        # Check if the record with the defined ID already exists
        if self.check_id_existence(id_value):
            tk.messagebox.showwarning("Warning", "Can't add the record. Record with that ID already exists.")
        else:
            # Check if the encryption_key_value is a hexadecimal string with exactly 64 characters
            if len(encryption_key_value) != 64 or not all(c in string.hexdigits for c in encryption_key_value):
                tk.messagebox.showwarning("Warning", "Encryption key must be a hexadecimal string with exactly 64 characters.")
                return
        
            # Add the record to the database
            self.add_record_to_database(id_value, label_value, encryption_key_value)
            # Inform the user that the record has been added
            tk.messagebox.showinfo("Success", "Record added to the database.")
            self.refresh_treeview()

    def add_record_to_database(self, id_value, label_value, encryption_key_value):
        # Insert the record into the database
        cursor.execute("INSERT INTO ESLs (id, label, encryption_key) VALUES (?, ?, ?)",
                       (id_value, label_value, encryption_key_value))
        # Commit the transaction
        conn.commit()
        
    def refresh_treeview(self):
        # Clear the treeview
        self.clear_treeview()
        # Retrieve all records from the database
        cursor.execute("SELECT id, label FROM ESLs")
        records = cursor.fetchall()
        # Add each record to the treeview
        for record in records:
            id_value, label_value = record
            self.add_to_treeview(id_value, label_value)
            
class EditForm(tk.Toplevel):
    def __init__(self, parent, selected_id, selected_label):
        tk.Toplevel.__init__(self, parent)
        self.parent = parent
        self.selected_id = selected_id

        self.title("Edit ESL Settings")
        self.geometry("259x124")
        self.resizable(True, True)  # Allow the window to be resizable
        self.position_window()  # Center the window on the screen

        self.label = ttk.Label(self, text="Label:")
        self.label.grid(row=0, column=0, padx=5, pady=5, sticky="e")

        self.label_entry = ttk.Entry(self)
        self.label_entry.insert(0, selected_label)
        self.label_entry.grid(row=0, column=1, padx=5, pady=5, sticky="ew")

        self.encryption_key = ttk.Label(self, text="Encryption Key:")
        self.encryption_key.grid(row=1, column=0, padx=5, pady=5, sticky="e")

        # Fetch encryption key from the database
        cursor.execute("SELECT encryption_key FROM ESLs WHERE id=?", (selected_id,))
        encryption_key = cursor.fetchone()[0]

        self.encryption_key_entry = ttk.Entry(self)
        self.encryption_key_entry.insert(0, encryption_key)
        self.encryption_key_entry.grid(row=1, column=1, padx=5, pady=5, sticky="ew")

        self.update_button = ttk.Button(self, text="Update", style="Accent.TButton", command=self.update_record)
        self.update_button.grid(row=2, column=0, padx=5, pady=5, sticky="ew")

        self.cancel_button = ttk.Button(self, text="Cancel", command=self.destroy)
        self.cancel_button.grid(row=2, column=1, padx=5, pady=5, sticky="ew")

    def update_record(self):
        label_value = self.label_entry.get()
        encryption_key_value = self.encryption_key_entry.get()

        # Update the record in the database
        cursor.execute("UPDATE ESLs SET label=?, encryption_key=? WHERE id=?", (label_value, encryption_key_value, self.selected_id))
        conn.commit()

        # Refresh the treeview in the main window
        self.parent.refresh_treeview()
        self.destroy()

    def position_window(self):
        # Center the window on the screen
        self.update_idletasks()
        width = self.winfo_width()
        height = self.winfo_height()
        x = (self.winfo_screenwidth() // 2) - (width // 2)
        y = (self.winfo_screenheight() // 2) - (height // 2)
        self.geometry('{}x{}+{}+{}'.format(width, height, x, y))

if __name__ == "__main__":
    root = tk.Tk()
    root.title("Electronic Shelf Label Management Software")
    assert sv_ttk.get_theme(root=root) == ttk.Style(root).theme_use()

    sv_ttk.set_theme("dark", root=root)
    assert sv_ttk.get_theme(root=root) == "dark"

    app = App(root)
    app.pack(fill="both", expand=True)
    root.geometry("740x765")
    root.update()
    # Check if the directory exists
    if not os.path.exists("images"):
        # Create the directory if it doesn't exist
        os.makedirs("images")
    root.minsize(root.winfo_width(), root.winfo_height())
    root.mainloop()