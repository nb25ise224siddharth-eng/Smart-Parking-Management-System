
const $ = (selector, parent = document) => parent.querySelector(selector);
const $$ = (selector, parent = document) => [...parent.querySelectorAll(selector)];

/* Get the latest CSRF token from the page */
function getCsrfToken() {
    return (
        $('input[name="csrf_token"]')?.value ||
        $('meta[name="csrf-token"]')?.content ||
        ''
    );
}

/* Toast notification */
function showToast(message, type = "success") {
    const toast = $("#toast");

    if (!toast) {
        alert(message);
        return;
    }

    toast.textContent = message;
    toast.className = `toast show ${type}`;

    setTimeout(() => {
        toast.classList.remove("show");
    }, 3000);
}

/* API request with CSRF protection */
async function apiRequest(url, data = {}) {
    const csrfToken = getCsrfToken();

    if (!csrfToken) {
        throw new Error("Security token missing. Please refresh the page and log in again.");
    }

    const body = new URLSearchParams(data);
    body.set("csrf_token", csrfToken);

    const response = await fetch(url, {
        method: "POST",
        credentials: "same-origin",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
            "X-CSRF-Token": csrfToken
        },
        body: body
    });

    const result = await response.json();

    if (!response.ok) {
        throw new Error(result.message || "Request failed");
    }

    return result;
}

/* Menu button */
$("#menuButton")?.addEventListener("click", () => {
    $(".sidebar")?.classList.toggle("open");
});

/* Booking modal */
$("#openBooking")?.addEventListener("click", () => {
    $("#bookingModal")?.classList.add("active");
});

$("#closeBooking")?.addEventListener("click", () => {
    $("#bookingModal")?.classList.remove("active");
});

/* Book parking slot */
$("#bookingForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();

    const form = event.currentTarget;
    const formData = new FormData(form);

    try {
        const result = await apiRequest("/api/book", {
            slot_number: formData.get("slot_number"),
            vehicle_number: formData.get("vehicle_number")
        });

        showToast(result.message, "success");
        form.reset();

        $("#bookingModal")?.classList.remove("active");

        setTimeout(() => {
            window.location.reload();
        }, 800);

    } catch (error) {
        showToast(error.message, "error");
    }
});

/* Release parking slot */
$$(".release").forEach((button) => {
    button.addEventListener("click", async () => {
        const slotNumber = button.dataset.slot;

        if (!slotNumber) {
            showToast("Parking slot number is missing.", "error");
            return;
        }

        if (!confirm(`Are you sure you want to release ${slotNumber}?`)) {
            return;
        }

        button.disabled = true;

        try {
            const result = await apiRequest("/api/release", {
                slot_number: slotNumber
            });

            showToast(result.message, "success");

            setTimeout(() => {
                window.location.reload();
            }, 800);

        } catch (error) {
            showToast(error.message, "error");
            button.disabled = false;
        }
    });
});

/* Occupancy bar */
$$("[data-width]").forEach((element) => {
    element.style.width = `${element.dataset.width}%`;
});

/* Close modal when clicking outside */
$("#bookingModal")?.addEventListener("click", (event) => {
    if (event.target.id === "bookingModal") {
        event.currentTarget.classList.remove("active");
    }
});
