// ============================================================================
// Car DIY Controller - Web Interface
// ============================================================================

const MAX_RPM = 130;
const CMD_INTERVAL = 50;    // Send commands every 50ms (20 Hz)
const STATUS_INTERVAL = 200;

const HOST = window.location.hostname || '192.168.4.1';
const API_BASE = `http://${HOST}`;
const STREAM_URL = `http://${HOST}:81/stream`;

let currentMode = 'drive';
let targetLeft = 0;
let targetRight = 0;
let cmdTimer = null;
let statusTimer = null;
let braking = false;
let coastMode = localStorage.getItem('coastMode') === 'true';

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
    } catch (e) {}
}

async function sendStop() {
    targetLeft = 0;
    targetRight = 0;
    braking = false;
    try {
        await fetch(`${API_BASE}/api/stop`, { method: 'POST', mode: 'no-cors' });
    } catch (e) {}
}

async function sendBrake() {
    targetLeft = 0;
    targetRight = 0;
    braking = true;
    try {
        await fetch(`${API_BASE}/api/brake`, { method: 'POST', mode: 'no-cors' });
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

function startCommandLoop() {
    if (cmdTimer) clearInterval(cmdTimer);
    cmdTimer = setInterval(() => {
        if (braking) {
            sendBrake();
        } else if (targetLeft !== 0 || targetRight !== 0) {
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
        targetLeft = 0;
        targetRight = 0;
        braking = false;
        sendStop();
    });
});

// ============================================================================
// Drive Mode: Throttle (left) + Steering (right) + Brake
// ============================================================================

(function initDrive() {
    let throttle = 0;   // -1 to +1
    let steering = 0;   // -1 to +1
    let throttleActive = false;
    let steeringActive = false;

    // --- Throttle slider ---
    const tTrack = document.getElementById('throttle-track');
    const tThumb = document.getElementById('throttle-thumb');
    const tFill = document.getElementById('throttle-fill');
    const tValue = document.getElementById('throttle-value');

    function updateThrottle(clientY) {
        const rect = tTrack.getBoundingClientRect();
        const centerY = rect.top + rect.height / 2;
        const maxDist = rect.height / 2 - 14;

        let dy = centerY - clientY;
        dy = constrain(dy, -maxDist, maxDist);
        throttle = dy / maxDist;

        const thumbY = 50 - (throttle * 46);
        tThumb.style.top = thumbY + '%';

        if (throttle >= 0) {
            tFill.style.bottom = '50%';
            tFill.style.top = 'auto';
            tFill.style.height = (throttle * 50) + '%';
        } else {
            tFill.style.top = '50%';
            tFill.style.bottom = 'auto';
            tFill.style.height = (-throttle * 50) + '%';
        }

        tValue.textContent = Math.round(throttle * MAX_RPM);
        applyDrive();
    }

    function resetThrottle() {
        throttle = 0;
        tThumb.style.top = '50%';
        tFill.style.height = '0%';
        tValue.textContent = '0';
        if (coastMode) {
            // Coast: don't send any command, let car roll on inertia
            // Only clear local targets so the command loop stops sending
            targetLeft = 0;
            targetRight = 0;
        } else {
            applyDrive();  // Sends RPM 0 → PID will actively slow to stop
        }
    }

    tTrack.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        throttleActive = true;
        braking = false;
        document.getElementById('brake-btn').classList.remove('active');
        tTrack.setPointerCapture(e.pointerId);
        updateThrottle(e.clientY);
    });
    tTrack.addEventListener('pointermove', (e) => {
        e.preventDefault();
        if (!throttleActive) return;
        updateThrottle(e.clientY);
    });
    tTrack.addEventListener('pointerup', (e) => { e.preventDefault(); throttleActive = false; resetThrottle(); });
    tTrack.addEventListener('pointercancel', () => { throttleActive = false; resetThrottle(); });
    tTrack.addEventListener('touchmove', (e) => { e.preventDefault(); }, { passive: false });

    // --- Steering slider (horizontal: left/right) ---
    const sTrack = document.getElementById('steering-track');
    const sThumb = document.getElementById('steering-thumb');
    const sValue = document.getElementById('steering-value');

    function updateSteering(clientX) {
        const rect = sTrack.getBoundingClientRect();
        const centerX = rect.left + rect.width / 2;
        const maxDist = rect.width / 2 - 24;

        let dx = clientX - centerX;
        dx = constrain(dx, -maxDist, maxDist);
        steering = dx / maxDist;

        const thumbX = 50 + (steering * 46);
        sThumb.style.left = thumbX + '%';

        const pctLabel = Math.round(steering * 100);
        sValue.textContent = (pctLabel > 0 ? 'R' : pctLabel < 0 ? 'L' : '') + Math.abs(pctLabel);
        applyDrive();
    }

    function resetSteering() {
        steering = 0;
        sThumb.style.left = '50%';
        sValue.textContent = '0';
        applyDrive();
    }

    sTrack.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        steeringActive = true;
        sTrack.setPointerCapture(e.pointerId);
        updateSteering(e.clientX);
    });
    sTrack.addEventListener('pointermove', (e) => {
        e.preventDefault();
        if (!steeringActive) return;
        updateSteering(e.clientX);
    });
    sTrack.addEventListener('pointerup', (e) => { e.preventDefault(); steeringActive = false; resetSteering(); });
    sTrack.addEventListener('pointercancel', () => { steeringActive = false; resetSteering(); });
    sTrack.addEventListener('touchmove', (e) => { e.preventDefault(); }, { passive: false });

    // --- Brake button ---
    const brakeBtn = document.getElementById('brake-btn');
    brakeBtn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        braking = true;
        brakeBtn.classList.add('active');
        throttle = 0;
        steering = 0;
        resetThrottle();
        resetSteering();
        sendBrake();
    });
    brakeBtn.addEventListener('pointerup', () => {
        braking = false;
        brakeBtn.classList.remove('active');
        sendStop();
    });
    brakeBtn.addEventListener('pointercancel', () => {
        braking = false;
        brakeBtn.classList.remove('active');
        sendStop();
    });

    // --- Mix throttle + steering to differential drive ---
    function applyDrive() {
        if (braking || currentMode !== 'drive') return;
        const tRPM = throttle * MAX_RPM;
        const sRPM = steering * MAX_RPM * 0.7;  // Steering slightly less aggressive than throttle
        targetLeft = Math.round(constrain(tRPM + sRPM, -MAX_RPM, MAX_RPM));
        targetRight = Math.round(constrain(tRPM - sRPM, -MAX_RPM, MAX_RPM));
    }
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

            let dy = centerY - clientY;
            dy = constrain(dy, -maxDist, maxDist);

            const pct = dy / maxDist;
            const rpm = Math.round(pct * MAX_RPM);

            const thumbY = 50 - (pct * 50);
            thumb.style.top = `${thumbY}%`;

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
            e.preventDefault();
            active = true;
            track.setPointerCapture(e.pointerId);
            updateSlider(e.clientY);
        });

        track.addEventListener('pointermove', (e) => {
            e.preventDefault();
            if (!active) return;
            updateSlider(e.clientY);
        });

        track.addEventListener('pointerup', (e) => { e.preventDefault(); active = false; resetSlider(); });
        track.addEventListener('pointercancel', () => { active = false; resetSlider(); });
        track.addEventListener('touchmove', (e) => { e.preventDefault(); }, { passive: false });
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

// Prevent all default touch behaviors (pull-to-refresh, scroll, zoom)
document.addEventListener('touchmove', (e) => { e.preventDefault(); }, { passive: false });
document.addEventListener('touchstart', (e) => {
    if (e.touches.length > 1) e.preventDefault(); // prevent pinch zoom
}, { passive: false });

// Request fullscreen on first tap (hides browser chrome + Android nav bar)
let fullscreenRequested = false;
document.addEventListener('click', () => {
    if (fullscreenRequested) return;
    fullscreenRequested = true;
    const el = document.documentElement;
    const rfs = el.requestFullscreen || el.webkitRequestFullscreen || el.msRequestFullscreen;
    if (rfs) rfs.call(el).catch(() => {});
}, { once: false });

initStream();
startCommandLoop();
statusTimer = setInterval(fetchStatus, STATUS_INTERVAL);
fetchStatus();
