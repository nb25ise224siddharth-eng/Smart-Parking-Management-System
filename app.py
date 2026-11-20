import os
import re
import secrets
from datetime import datetime
from functools import wraps

from flask import Flask, jsonify, render_template, request, redirect, url_for, session, flash
from werkzeug.security import check_password_hash

from database import get_db, init_db

app = Flask(__name__)
app.config.update(
    SECRET_KEY=os.environ.get("SMARTPARK_SECRET", secrets.token_hex(32)),
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Lax",
)

init_db()


def now_text():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def csrf_token():
    token = session.get("csrf_token")
    if not token:
        token = secrets.token_hex(32)
        session["csrf_token"] = token
    return token


app.jinja_env.globals["csrf_token"] = csrf_token


@app.before_request
def protect_mutations():
    if request.method in ("POST", "PUT", "PATCH", "DELETE"):
        supplied = request.form.get("csrf_token") or request.headers.get("X-CSRF-Token")
        if not supplied or supplied != session.get("csrf_token"):
            return jsonify({"success": False, "message": "Invalid or missing security token."}), 403


def login_required(role=None):
    def decorator(view):
        @wraps(view)
        def wrapped(*args, **kwargs):
            if not session.get("user_id"):
                return redirect(url_for("login", next=request.path))
            if role and session.get("role") != role:
                flash("Administrator access is required.", "error")
                return redirect(url_for("index"))
            return view(*args, **kwargs)
        return wrapped
    return decorator


def stats_and_slots(search=None):
    conn = get_db()
    slots = conn.execute("SELECT * FROM parking_slots ORDER BY slot_number").fetchall()
    total = len(slots)
    occupied = sum(1 for s in slots if s["status"] == "occupied")
    available = total - occupied
    stats = {"total": total, "available": available, "occupied": occupied,
             "percentage": round((occupied / total * 100) if total else 0, 1)}
    history = conn.execute("SELECT * FROM parking_history ORDER BY id DESC LIMIT 8").fetchall()

    if search:
        term = f"%{search.strip()}%"
        slots = conn.execute("SELECT * FROM parking_slots WHERE slot_number LIKE ? OR vehicle_number LIKE ? ORDER BY slot_number", (term, term)).fetchall()
        history = conn.execute("SELECT * FROM parking_history WHERE slot_number LIKE ? OR vehicle_number LIKE ? ORDER BY id DESC", (term, term)).fetchall()
    conn.close()
    return stats, slots, history


@app.route("/")
def index():
    search = request.args.get("q", "").strip()
    stats, slots, history = stats_and_slots(search)
    return render_template("index.html", stats=stats, slots=slots, history=history, search=search)


@app.route("/api/slots")
def api_slots():
    stats, slots, _ = stats_and_slots(request.args.get("q", ""))
    return jsonify({"success": True, "stats": stats, "slots": [dict(s) for s in slots]})


@app.route("/api/book", methods=["POST"])
def book_slot():
    slot_number = request.form.get("slot_number", "").strip().upper()
    vehicle_number = re.sub(r"[^A-Z0-9]", "", request.form.get("vehicle_number", "").upper())
    if not re.fullmatch(r"[A-Z0-9]{4,12}", vehicle_number):
        return jsonify({"success": False, "message": "Enter a valid vehicle registration number (4–12 letters/numbers)."}), 400

    conn = get_db()
    slot = conn.execute("SELECT * FROM parking_slots WHERE slot_number = ?", (slot_number,)).fetchone()
    if not slot:
        conn.close()
        return jsonify({"success": False, "message": "Parking slot not found."}), 404
    if slot["status"] == "occupied":
        conn.close()
        return jsonify({"success": False, "message": "This parking slot is already occupied."}), 409
    if conn.execute("SELECT id FROM parking_slots WHERE vehicle_number = ?", (vehicle_number,)).fetchone():
        conn.close()
        return jsonify({"success": False, "message": "This vehicle is already parked in another slot."}), 409

    booked = now_text()
    conn.execute("UPDATE parking_slots SET status = 'occupied', vehicle_number = ?, booking_time = ? WHERE slot_number = ?", (vehicle_number, booked, slot_number))
    conn.execute("INSERT INTO parking_history (slot_number, vehicle_number, booking_time, status) VALUES (?, ?, ?, 'active')", (slot_number, vehicle_number, booked))
    conn.commit()
    conn.close()
    return jsonify({"success": True, "message": f"Vehicle {vehicle_number} booked in {slot_number}."})


@app.route("/api/release", methods=["POST"])
def release_slot():
    slot_number = request.form.get("slot_number", "").strip().upper()
    conn = get_db()
    slot = conn.execute("SELECT * FROM parking_slots WHERE slot_number = ?", (slot_number,)).fetchone()
    if not slot:
        conn.close()
        return jsonify({"success": False, "message": "Parking slot not found."}), 404
    if slot["status"] != "occupied":
        conn.close()
        return jsonify({"success": False, "message": "This parking slot is already available."}), 409

    released = now_text()
    conn.execute("UPDATE parking_slots SET status = 'available', vehicle_number = NULL, booking_time = NULL WHERE slot_number = ?", (slot_number,))
    conn.execute("UPDATE parking_history SET release_time = ?, status = 'released' WHERE slot_number = ? AND vehicle_number = ? AND status = 'active'", (released, slot_number, slot["vehicle_number"]))
    conn.commit()
    conn.close()
    return jsonify({"success": True, "message": f"{slot_number} is now available."})


@app.route("/history")
def history():
    search = request.args.get("q", "").strip()
    conn = get_db()
    if search:
        term = f"%{search}%"
        records = conn.execute("SELECT * FROM parking_history WHERE slot_number LIKE ? OR vehicle_number LIKE ? ORDER BY id DESC", (term, term)).fetchall()
    else:
        records = conn.execute("SELECT * FROM parking_history ORDER BY id DESC").fetchall()
    conn.close()
    return render_template("history.html", records=records, search=search)


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")
        conn = get_db()
        user = conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()
        conn.close()
        if user and check_password_hash(user["password_hash"], password):
            session.clear()
            session["csrf_token"] = csrf_token()
            session["user_id"] = user["id"]
            session["username"] = user["username"]
            session["role"] = user["role"]
            return redirect(request.args.get("next") or url_for("admin"))
        flash("Invalid username or password.", "error")
    return render_template("login.html")


@app.route("/logout")
def logout():
    session.clear()
    return redirect(url_for("index"))


@app.route("/admin")
@login_required("admin")
def admin():
    stats, slots, history = stats_and_slots(request.args.get("q", ""))
    return render_template("admin.html", stats=stats, slots=slots, history=history)


@app.errorhandler(500)
def internal_error(error):
    return jsonify({"success": False, "message": "An unexpected server error occurred."}), 500


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000, debug=True)
