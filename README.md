# SmartPark — Smart Parking Management System

## Installation

```powershell
pip install -r requirements.txt
python app.py
```

Open http://127.0.0.1:5000.

## Administrator login

- Username: `admin`
- Password: `admin123`

The SQLite database is created automatically at first startup and contains parking slots P01–P10.

## Verification

Run `python verify_app.py` to exercise booking, release, and dashboard statistics using Flask's test client.
