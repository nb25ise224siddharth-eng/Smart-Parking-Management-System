
const $ = (s, p = document) => p.querySelector(s);
const $$ = (s, p = document) => [...p.querySelectorAll(s)];

$('#menuButton')?.addEventListener('click', () => {
    $('#sidebar')?.classList.toggle('open');
});

function getCsrfToken() {
    return $('input[name="csrf_token"]')?.value ||
           $('meta[name="csrf-token"]')?.content ||
           '';
}

function showToast(message, error = false, title = 'Parking Updated') {
    const toast = $('#toast');
    if (!toast) return;

    $('#toastTitle').textContent = title;
    $('#toastMessage').textContent = message;
    toast.classList.toggle('error', error);
    $('#toastIcon').textContent = error ? '!' : '✓';
    toast.classList.add('show');

    clearTimeout(window.toastTimer);
    window.toastTimer = setTimeout(
        () => toast.classList.remove('show'), 3500
    );
}

const modal = $('#bookingModal');

function openBooking(slot) {
    if (!modal) return;

    $('#modalSlot').textContent = slot;
    $('#slotNumber').value = slot;
    $('#vehicleNumber').value = '';

    modal.classList.add('open');
    modal.setAttribute('aria-hidden', 'false');

    setTimeout(() => $('#vehicleNumber')?.focus(), 100);
}

function closeBooking() {
    if (!modal) return;

    modal.classList.remove('open');
    modal.setAttribute('aria-hidden', 'true');
}

$$('.book').forEach(button => {
    button.addEventListener('click', () => {
        openBooking(button.dataset.slot);
    });
});

$$('.release').forEach(button => {
    button.addEventListener('click', () => {
        const slot = button.dataset.slot;

        if (confirm(`Release parking slot ${slot}?`)) {
            releaseSlot(slot);
        }
    });
});

$('.modal-close')?.addEventListener('click', closeBooking);
$('.modal-backdrop')?.addEventListener('click', closeBooking);

document.addEventListener('keydown', e => {
    if (e.key === 'Escape') closeBooking();
});

async function apiRequest(url, data) {
    const token = getCsrfToken();

    if (!token) {
        throw new Error('Security token missing. Refresh the page and try again.');
    }

    const body = new URLSearchParams(data);
    body.set('csrf_token', token);

    const response = await fetch(url, {
        method: 'POST',
        credentials: 'same-origin',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'X-CSRF-Token': token
        },
        body
    });

    const result = await response.json();

    if (!response.ok) {
        throw new Error(result.message || 'Operation failed');
    }

    return result;
}

$('#bookingForm')?.addEventListener('submit', async e => {
    e.preventDefault();

    const submit = e.submitter || $('button[type="submit"]', e.target);
    const vehicle = $('#vehicleNumber').value
        .replace(/[^A-Za-z0-9]/g, '')
        .toUpperCase();

    if (!/^[A-Z0-9]{4,12}$/.test(vehicle)) {
        showToast(
            'Enter 4–12 letters or numbers.',
            true,
            'Invalid registration'
        );
        return;
    }

    if (submit) submit.disabled = true;

    try {
        const result = await apiRequest('/api/book', {
            slot_number: $('#slotNumber').value,
            vehicle_number: vehicle
        });

        closeBooking();
        showToast(result.message, false, 'Booking confirmed');

        setTimeout(() => location.reload(), 650);
    } catch (error) {
        showToast(error.message, true, 'Booking failed');
    } finally {
        if (submit) submit.disabled = false;
    }
});

async function releaseSlot(slot) {
    try {
        const result = await apiRequest('/api/release', {
            slot_number: slot
        });

        showToast(result.message, false, 'Slot released');
        setTimeout(() => location.reload(), 650);
    } catch (error) {
        showToast(error.message, true, 'Release failed');
    }
}

const canvas = $('#utilisationChart');

if (canvas && window.Chart) {
    new Chart(canvas, {
        type: 'doughnut',
        data: {
            labels: ['Available', 'Occupied'],
            datasets: [{
                data: [
                    Number($('#chartAvailable')?.textContent || 0),
                    Number($('#chartOccupied')?.textContent || 0)
                ],
                backgroundColor: ['#32c785', '#f05e70'],
                borderWidth: 0,
                hoverOffset: 4
            }]
        },
        options: {
            cutout: '74%',
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                tooltip: {
                    backgroundColor: '#111b2a',
                    titleColor: '#edf4ff',
                    bodyColor: '#aeb7c5',
                    padding: 10
                }
            }
        }
    });
}
