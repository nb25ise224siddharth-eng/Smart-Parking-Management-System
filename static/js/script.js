
const $ = (selector, parent = document) => parent.querySelector(selector);
const $$ = (selector, parent = document) => [...parent.querySelectorAll(selector)];

function getCsrfToken() {
    return (
        $('input[name="csrf_token"]')?.value ||
        $('meta[name="csrf-token"]')?.content ||
        ''
    );
}

function showToast(message, type = "success") {
    const toast = $("#toast");

    if (!toast) {
        alert(message);
        return;
    }

    const title = $("#toastTitle");
    const messageElement = $("#toastMessage");

    if (title) {
        title.textContent = type === "error" ? "Error" : "Success";
    }

    if (messageElement) {
        messageElement.textContent = message;
    }

    toast.className = `toast show ${type}`;

    setTimeout(() => {
        toast.classList.remove("show");
    }, 3000);
}

async function apiRequest(url, data = {}) {
    const csrfToken = getCsrfToken();

    if (!csrfToken) {
        throw new Error("Security token missing. Refresh the page and log in again.");
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
        throw new Error(result.message || "Request failed.");
    }

    return result;
}

/* Mobile menu */
$("#menuButton")?.addEventListener("click", () => {
    $(".sidebar")?.classList.toggle("open");
});

/* Booking modal */
const bookingModal = $("#bookingModal");

function openBooking(slotNumber) {
    if (!bookingModal) {
        showToast("Booking modal not found.", "error");
        return;
    }

    const slotInput = $("#slotNumber");
    const modalSlot = $("#modalSlot");
    const vehicleInput = $("#vehicleNumber");

    if (slotInput) slotInput.value = slotNumber;
    if (modalSlot) modalSlot.textContent = slotNumber;

    if (vehicleInput) vehicleInput.value = "";

    bookingModal.classList.add("active");
    bookingModal.setAttribute("aria-hidden", "false");

    vehicleInput?.focus();
}

function closeBooking() {
    bookingModal?.classList.remove("active");
    bookingModal?.setAttribute("aria-hidden", "true");
}

/* Connect every Book button */
$$(".book").forEach((button) => {
    button.addEventListener("click", () => {
        openBooking(button.dataset.slot);
    });
});

/* Close buttons */
$("#closeBooking")?.addEventListener("click", closeBooking);
$(".modal-close")?.addEventListener("click", closeBooking);

$(".modal-backdrop")?.addEventListener("click", closeBooking);

/* Booking form submission */
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
        closeBooking();

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

/* Occupancy progress bar */
$$("[data-width]").forEach((element) => {
    element.style.width = `${element.dataset.width}%`;
});

/* Close modal using Escape */
document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
        closeBooking();
    }
});
