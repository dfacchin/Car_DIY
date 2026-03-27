// ============================================================================
// Car DIY Controller - Web Interface
// ============================================================================

const MAX_RPM = 130;
const CMD_INTERVAL = 50;    // Send commands every 50ms (20 Hz)
const STATUS_INTERVAL = 200;

// Determine server address (same host)
const HOST = window.location.hostname || '192.168.4.1';
const API_BASE = `http://${HOST}`;
const STREAM_URL = `http://${HOST}:81/stream`;

let currentMode = 'joystick';
let targetLeft = 0;
let targetRight = 0;
let cmdTimer = null;
let statusTimer = null;

// ============================================================================
// Video Stream
// ============================================================================

function initStream() {
    const img = document.getElementById('video-stream');
    img.src = STREAM_URL;
    img.onerror = () => {
        setTimeout(() => { img.src = STREAM_URL; }, 2000);
    };
}

// ============================================================================
// API Communication
// ============================================================================

async function sendControl(left, right) {
    try {
        const url = `${API_BASE}/api/control?left=${Math.round(left)}&right=${Math.round(right)}`;
        await fetch(url, { method: 'POST', mode: 'no-cors' });
    } catch (e) {
        // Silent fail - will retry
    }
}

async function sendStop() {
    targetLeft = 0;
    targetRight = 0;
    try {
        await fetch(`${API_BASE}/api/stop`, { method: 'POST', mode: 'no-cors' });
    } catch (e) {}
}

async function fetchStatus() {
    try {
        const res = await fetch(`${API_BASE}/api/status`);
        const data = await res.json();
        document.getElementById('rpm-display').textContent =
            `L: ${data.left_rpm} | R: ${data.right_rpm}`;
        const status = document.getElementById('connection-status');
        status.textContent = 'Connected';
        status.classList.remove('disconnected');
        if (data.error) {
            status.textContent = `Error: ${data.error}`;
            status.classList.add('disconnected');
        }
    } catch (e) {
        const status = document.getElementById('connection-status');
        status.textContent = 'Disconnected';
        status.classList.add('disconnected');
    }
}

// Periodic command sender
function startCommandLoop() {
    if (cmdTimer) clearInterval(cmdTimer);
    cmdTimer = setInterval(() => {
        if (targetLeft !== 0 || targetRight !== 0) {
            sendControl(targetLeft, targetRight);
        }
    }, CMD_INTERVAL);
}

// ============================================================================
// Tab Switching
// ============================================================================

document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.control-mode').forEach(m => m.classList.remove('active'));
        tab.classList.add('active');
        currentMode = tab.dataset.mode;
        document.getElementById(`${currentMode}-mode`).classList.add('active');
        // Reset on mode switch
        targetLeft = 0;
        targetRight = 0;
        sendStop();
    });
});

// ============================================================================
// Joystick Control
// ============================================================================

(function initJoystick() {
    const base = document.getElementById('joystick-base');
    const handle = document.getElementById('joystick-handle');
    const baseRect = () => base.getBoundingClientRect();
    let active = false;

    function updateJoystick(clientX, clientY) {
        const rect = baseRect();
        const centerX = rect.left + rect.width / 2;
        const centerY = rect.top + rect.height / 2;
        const maxDist = rect.width / 2 - 30;

        let dx = clientX - centerX;
        let dy = clientY - centerY;
        const dist = Math.sqrt(dx * dx + dy * dy);

        if (dist > maxDist) {
            dx = (dx / dist) * maxDist;
            dy = (dy / dist) * maxDist;
        }

        handle.style.transform = `translate(${dx}px, ${dy}px)`;

        // Convert to differential drive:
        // Y axis = throttle (forward/back), X axis = steering
        const throttle = -(dy / maxDist) * MAX_RPM;   // Up = forward
        const steering = (dx / maxDist) * MAX_RPM;

        // Mix: differential steering
        targetLeft = Math.round(constrain(throttle + steering, -MAX_RPM, MAX_RPM));
        targetRight = Math.round(constrain(throttle - steering, -MAX_RPM, MAX_RPM));
    }

    function resetJoystick() {
        handle.style.transform = 'translate(0, 0)';
        targetLeft = 0;
        targetRight = 0;
        sendControl(0, 0);
    }

    base.addEventListener('pointerdown', (e) => {
        active = true;
        base.setPointerCapture(e.pointerId);
        updateJoystick(e.clientX, e.clientY);
    });

    base.addEventListener('pointermove', (e) => {
        if (!active) return;
        updateJoystick(e.clientX, e.clientY);
    });

    base.addEventListener('pointerup', () => { active = false; resetJoystick(); });
    base.addEventListener('pointercancel', () => { active = false; resetJoystick(); });
})();

// ============================================================================
// Tank Slider Control
// ============================================================================

(function initTankSliders() {
    const sliders = document.querySelectorAll('.vertical-slider-track');

    sliders.forEach(track => {
        const motor = track.dataset.motor;
        const thumb = track.querySelector('.vertical-slider-thumb');
        const fill = track.querySelector('.vertical-slider-fill');
        let active = false;

        function updateSlider(clientY) {
            const rect = track.getBoundingClientRect();
            const centerY = rect.top + rect.height / 2;
            const maxDist = rect.height / 2 - 10;

            let dy = centerY - clientY;  // Up = positive
            dy = constrain(dy, -maxDist, maxDist);

            const pct = dy / maxDist;    // -1 to +1
            const rpm = Math.round(pct * MAX_RPM);

            // Position thumb
            const thumbY = 50 - (pct * 50);  // 0% = top, 50% = center, 100% = bottom
            thumb.style.top = `${thumbY}%`;

            // Update fill
            if (pct >= 0) {
                fill.style.bottom = '50%';
                fill.style.top = 'auto';
                fill.style.height = `${pct * 50}%`;
            } else {
                fill.style.top = '50%';
                fill.style.bottom = 'auto';
                fill.style.height = `${-pct * 50}%`;
            }

            if (motor === 'left') {
                targetLeft = rpm;
                document.getElementById('left-value').textContent = rpm;
            } else {
                targetRight = rpm;
                document.getElementById('right-value').textContent = rpm;
            }
        }

        function resetSlider() {
            thumb.style.top = '50%';
            fill.style.height = '0%';
            if (motor === 'left') {
                targetLeft = 0;
                document.getElementById('left-value').textContent = '0';
            } else {
                targetRight = 0;
                document.getElementById('right-value').textContent = '0';
            }
            sendControl(targetLeft, targetRight);
        }

        track.addEventListener('pointerdown', (e) => {
            active = true;
            track.setPointerCapture(e.pointerId);
            updateSlider(e.clientY);
        });

        track.addEventListener('pointermove', (e) => {
            if (!active) return;
            updateSlider(e.clientY);
        });

        track.addEventListener('pointerup', () => { active = false; resetSlider(); });
        track.addEventListener('pointercancel', () => { active = false; resetSlider(); });
    });
})();

// ============================================================================
// Emergency Stop
// ============================================================================

document.getElementById('stop-btn').addEventListener('click', sendStop);
document.getElementById('stop-btn').addEventListener('touchstart', (e) => {
    e.preventDefault();
    sendStop();
});

// ============================================================================
// Utility
// ============================================================================

function constrain(val, min, max) {
    return Math.min(Math.max(val, min), max);
}

// ============================================================================
// Init
// ============================================================================

initStream();
startCommandLoop();
statusTimer = setInterval(fetchStatus, STATUS_INTERVAL);
fetchStatus();
