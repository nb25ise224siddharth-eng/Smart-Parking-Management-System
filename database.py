import sqlite3
from pathlib import Path
from werkzeug.security import generate_password_hash

DB_PATH = Path(__file__).resolve().parent / "parking.db"


def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_db()
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS parking_slots (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slot_number TEXT UNIQUE NOT NULL,
            status TEXT NOT NULL DEFAULT 'available',
            vehicle_number TEXT,
            booking_time TEXT
        );

        CREATE TABLE IF NOT EXISTS parking_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slot_number TEXT NOT NULL,
            vehicle_number TEXT NOT NULL,
            booking_time TEXT NOT NULL,
            release_time TEXT,
            status TEXT NOT NULL DEFAULT 'active'
        );

        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'admin'
        );
    """)
    conn.execute("UPDATE parking_slots SET status = 'available' WHERE status IS NULL")

    for number in range(1, 11):
        conn.execute("INSERT OR IGNORE INTO parking_slots (slot_number, status) VALUES (?, 'available')", (f"P{number:02d}",))

    if conn.execute("SELECT id FROM users WHERE username = 'admin'").fetchone() is None:
        conn.execute(
            "INSERT INTO users (username, password_hash, role) VALUES (?, ?, 'admin')",
            ("admin", generate_password_hash("admin123")),
        )
    conn.commit()
    conn.close()
