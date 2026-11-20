from app import app

with app.test_client() as client:
    response = client.get("/")
    html = response.get_data(as_text=True)
    marker = 'name="csrf_token" value="'
    token = html.split(marker, 1)[1].split('"', 1)[0]

    booked = client.post("/api/book", data={
        "csrf_token": token,
        "slot_number": "P01",
        "vehicle_number": "MH12AB1234",
    })
    print("BOOK", booked.status_code, booked.json)

    released = client.post("/api/release", data={
        "csrf_token": token,
        "slot_number": "P01",
    })
    print("RELEASE", released.status_code, released.json)

    slots = client.get("/api/slots")
    print("STATS", slots.status_code, slots.json["stats"])
