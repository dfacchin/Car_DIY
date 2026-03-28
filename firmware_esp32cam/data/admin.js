// ============================================================================
// Car DIY Admin - Debug & PID Tuning Interface
// ============================================================================

const HOST = window.location.hostname || '192.168.4.1';
const API = `http://${HOST}`;

let pollTimer = null;
let pollRate = 200;
let debugEnabled = false;

// Chart data buffer (circular)
const CHART_LEN = 150;
const chartData = {
    leftRpm: new Array(CHART_LEN).fill(0),
    rightRpm: new Array(CHART_LEN).fill(0),
    targetL: new Array(CHART_LEN).fill(0),
    targetR: new Array(CHART_LEN).fill(0),
};

// ============================================================================
// Logging
// ============================================================================

function log(msg, cls = 'info') {
    const el = document.getElementById('log-console');
    const entry = document.createElement('div');
    entry.className = `log-entry ${cls}`;
    const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
    entry.textContent = `[${ts}] ${msg}`;
    el.appendChild(entry);
    if (document.getElementById('log-auto-scroll').checked) {
        el.scrollTop = el.scrollHeight;
    }
    // Keep max 500 entries
    while (el.children.length > 500) el.removeChild(el.firstChild);
}

function clearLog() {
    document.getElementById('log-console').innerHTML = '';
}

// ============================================================================
// API helpers
// ============================================================================

async function api(path, opts = {}) {
    try {
        const res = await fetch(`${API}${path}`, opts);
        const data = await res.json();
        return data;
    } catch (e) {
        log(`API error: ${path} - ${e.message}`, 'error');
        return null;
    }
}

// ============================================================================
// Status polling
// ============================================================================

async function pollStatus() {
    const endpoint = debugEnabled ? '/api/debug/status' : '/api/status';
    const data = await api(endpoint);
    if (!data) {
        setConnected(false);
        return;
    }
    setConnected(true);

    // Arduino connection status
    setMotorConnected(data.motor_connected === true);

    // Update status cards
    setText('s-left-rpm', data.left_rpm);
    setText('s-right-rpm', data.right_rpm);

    if (debugEnabled) {
        setText('s-target-l', data.target_left);
        setText('s-target-r', data.target_right);
        setText('s-pwm-l', data.pwm_a);
        setText('s-pwm-r', data.pwm_b);
        setBar('bar-pwm-l', data.pwm_a, 255);
        setBar('bar-pwm-r', data.pwm_b, 255);
        setText('s-enc-l', data.enc_a);
        setText('s-enc-r', data.enc_b);
        setText('s-uptime', formatUptime(data.uptime));
        if (data.pong_age !== undefined && data.pong_age > 0) {
            setText('s-pong-age', `${data.pong_age}ms ago`);
        }

        pushChart(data.left_rpm, data.right_rpm, data.target_left, data.target_right);
    } else {
        setText('s-target-l', data.target_left);
        setText('s-target-r', data.target_right);

        pushChart(data.left_rpm, data.right_rpm, data.target_left, data.target_right);
    }

    // Always show TX/RX counters
    setText('s-tx-count', data.tx_count);
    setText('s-rx-count', data.rx_count);

    // Firmware versions (always, regardless of debug mode)
    if (data.esp32_fw !== undefined) {
        document.getElementById('esp32-fw').textContent = `ESP32 FW: v${data.esp32_fw}`;
        document.getElementById('esp32-fw').style.color = '#4cc9f0';
    }
    if (data.expected_fw !== undefined) {
        document.getElementById('motor-fw-stored').textContent = `Stored: v${data.expected_fw}`;
    }
    if (data.fw_version !== undefined) {
        const el = document.getElementById('motor-fw-running');
        const running = data.fw_version;
        const stored = data.expected_fw || 0;
        el.textContent = running > 0 ? `Running: v${running}` : 'Running: --';
        if (running > 0 && running === stored) {
            el.style.background = '#0d421b';
            el.style.color = '#3fb950';
        } else if (running > 0) {
            el.style.background = '#3d1216';
            el.style.color = '#f85149';
        }
    }

    // Error display
    const errEl = document.getElementById('s-error');
    if (data.error && data.error !== 0) {
        errEl.textContent = errorName(data.error);
        errEl.style.color = '#f85149';
    } else {
        errEl.textContent = 'None';
        errEl.style.color = '#3fb950';
    }

    drawChart();
}

function setConnected(ok) {
    const el = document.getElementById('conn-status');
    el.textContent = ok ? 'ESP32: OK' : 'ESP32: Offline';
    el.className = `status-badge ${ok ? 'connected' : 'disconnected'}`;
}

function setMotorConnected(ok) {
    const el = document.getElementById('motor-status');
    el.textContent = ok ? 'Arduino: OK' : 'Arduino: No Response';
    el.className = `status-badge ${ok ? 'connected' : 'disconnected'}`;
}

function setText(id, val) {
    const el = document.getElementById(id);
    if (el) el.textContent = val !== undefined ? val : '--';
}

function setBar(id, val, max) {
    const el = document.getElementById(id);
    if (el) el.style.width = `${Math.min(100, (Math.abs(val) / max) * 100)}%`;
}

function formatUptime(s) {
    if (s === undefined) return '--';
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    return `${h}h ${m}m ${sec}s`;
}

function errorName(code) {
    const names = {
        0: 'None', 1: 'Stall Left', 2: 'Stall Right', 3: 'Stall Both',
        4: 'Timeout', 5: 'Overcurrent', 6: 'Watchdog'
    };
    return names[code] || `Unknown(${code})`;
}

function startPolling() {
    stopPolling();
    pollTimer = setInterval(pollStatus, pollRate);
    pollStatus();
}

function stopPolling() {
    if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
}

// ============================================================================
// Debug toggle
// ============================================================================

document.getElementById('btn-debug-toggle').addEventListener('click', async () => {
    const res = await api('/api/debug/toggle', { method: 'POST' });
    if (res && res.ok) {
        debugEnabled = !debugEnabled;
        document.getElementById('btn-debug-toggle').textContent =
            debugEnabled ? 'Disable Debug Stream' : 'Enable Debug Stream';
        log(`Debug stream ${debugEnabled ? 'enabled' : 'disabled'}`, 'success');
    }
});

// Ping toggle
document.getElementById('btn-ping-toggle').addEventListener('click', async () => {
    const res = await api('/api/debug/ping');
    if (res && res.ok) {
        const btn = document.getElementById('btn-ping-toggle');
        if (res.ping) {
            btn.textContent = 'Ping: ON';
            btn.className = 'btn accent';
            log('Ping enabled', 'success');
        } else {
            btn.textContent = 'Ping: OFF';
            btn.className = 'btn danger';
            log('Ping disabled', 'warn');
        }
    }
});

document.getElementById('poll-rate').addEventListener('change', (e) => {
    pollRate = parseInt(e.target.value);
    startPolling();
    log(`Poll rate changed to ${pollRate}ms`);
});

// ============================================================================
// PID Tuning
// ============================================================================

async function loadPid() {
    log('Loading PID params from motor controller...');
    const data = await api('/api/pid');
    if (!data) return;

    if (data.valid) {
        document.getElementById('pid-kp').value = data.kp.toFixed(2);
        document.getElementById('pid-ki').value = data.ki.toFixed(2);
        document.getElementById('pid-kd').value = data.kd.toFixed(2);
        document.getElementById('pid-imax').value = data.imax;
        document.getElementById('pid-ramp').value = data.ramp;
        log(`PID loaded: Kp=${data.kp} Ki=${data.ki} Kd=${data.kd} Imax=${data.imax} Ramp=${data.ramp}`, 'success');
    } else {
        log('PID params not yet received from motor controller', 'warn');
    }
}

async function setPid(param) {
    const el = document.getElementById(`pid-${param}`);
    const val = parseFloat(el.value);
    const res = await api(`/api/pid/set?param=${param}&value=${val}`, { method: 'POST' });
    if (res && res.ok) {
        log(`PID ${param} set to ${val}`, 'success');
    }
}

async function setAllPid() {
    const params = ['kp', 'ki', 'kd', 'imax', 'ramp'];
    for (const p of params) {
        await setPid(p);
    }
    log('All PID params sent', 'success');
}

async function savePid() {
    const res = await api('/api/pid/save', { method: 'POST' });
    if (res && res.ok) {
        log('PID params saved to EEPROM!', 'success');
    }
}

// ============================================================================
// Test Commands
// ============================================================================

const testLeft = document.getElementById('test-left');
const testRight = document.getElementById('test-right');

testLeft.addEventListener('input', () => {
    document.getElementById('test-left-val').textContent = testLeft.value;
});

testRight.addEventListener('input', () => {
    document.getElementById('test-right-val').textContent = testRight.value;
});

async function sendTestCmd() {
    const l = parseInt(testLeft.value);
    const r = parseInt(testRight.value);
    const res = await api(`/api/control?left=${l}&right=${r}`, { method: 'POST' });
    if (res && res.ok) {
        log(`Sent RPM: L=${l} R=${r}`, 'data');
    }
}

async function sendStop() {
    const res = await api('/api/stop', { method: 'POST' });
    if (res) {
        log('STOP sent', 'warn');
        resetSliders();
    }
}

function resetSliders() {
    testLeft.value = 0;
    testRight.value = 0;
    document.getElementById('test-left-val').textContent = '0';
    document.getElementById('test-right-val').textContent = '0';
}

// Raw PWM sliders
const rawLeft = document.getElementById('raw-left');
const rawRight = document.getElementById('raw-right');

rawLeft.addEventListener('input', () => {
    document.getElementById('raw-left-val').textContent = rawLeft.value;
});
rawRight.addEventListener('input', () => {
    document.getElementById('raw-right-val').textContent = rawRight.value;
});

async function sendRawPwm() {
    const l = parseInt(rawLeft.value);
    const r = parseInt(rawRight.value);
    const res = await api(`/api/debug/raw_pwm?left=${l}&right=${r}`, { method: 'POST' });
    if (res && res.ok) {
        log(`Raw PWM sent: L=${l} R=${r}`, 'data');
    }
}

function resetRawSliders() {
    rawLeft.value = 0;
    rawRight.value = 0;
    document.getElementById('raw-left-val').textContent = '0';
    document.getElementById('raw-right-val').textContent = '0';
}

// Safety toggle
let safetyEnabled = true;

async function toggleSafety() {
    safetyEnabled = !safetyEnabled;
    const res = await api(`/api/debug/safety?enabled=${safetyEnabled ? '1' : '0'}`, { method: 'POST' });
    if (res && res.ok) {
        const btn = document.getElementById('btn-safety');
        const status = document.getElementById('safety-status');
        if (safetyEnabled) {
            btn.textContent = 'Enabled';
            btn.className = 'btn sm accent';
            status.textContent = 'Watchdog + stall + timeout active';
            status.style.color = '#3fb950';
        } else {
            btn.textContent = 'DISABLED';
            btn.className = 'btn sm danger';
            status.textContent = 'ALL SAFETY OFF — be careful!';
            status.style.color = '#f85149';
        }
        log(`Safety ${safetyEnabled ? 'enabled' : 'DISABLED'}`, safetyEnabled ? 'success' : 'error');
    }
}

async function quickTest(l, r) {
    testLeft.value = l;
    testRight.value = r;
    document.getElementById('test-left-val').textContent = l;
    document.getElementById('test-right-val').textContent = r;
    const res = await api(`/api/control?left=${l}&right=${r}`, { method: 'POST' });
    if (res && res.ok) {
        log(`Quick test: L=${l} R=${r}`, 'data');
    }
}

// ============================================================================
// RPM Chart (canvas-based, no dependencies)
// ============================================================================

function pushChart(lr, rr, tl, tr) {
    chartData.leftRpm.push(lr || 0);
    chartData.rightRpm.push(rr || 0);
    chartData.targetL.push(tl || 0);
    chartData.targetR.push(tr || 0);
    if (chartData.leftRpm.length > CHART_LEN) {
        chartData.leftRpm.shift();
        chartData.rightRpm.shift();
        chartData.targetL.shift();
        chartData.targetR.shift();
    }
}

function drawChart() {
    const canvas = document.getElementById('rpm-chart');
    const ctx = canvas.getContext('2d');
    const W = canvas.width = canvas.clientWidth * (window.devicePixelRatio || 1);
    const H = canvas.height = canvas.clientHeight * (window.devicePixelRatio || 1);
    ctx.scale(window.devicePixelRatio || 1, window.devicePixelRatio || 1);
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;

    ctx.clearRect(0, 0, w, h);

    // Find Y range
    const allVals = [
        ...chartData.leftRpm, ...chartData.rightRpm,
        ...chartData.targetL, ...chartData.targetR
    ];
    let maxAbs = Math.max(10, ...allVals.map(Math.abs));
    maxAbs = Math.ceil(maxAbs / 10) * 10;  // Round up to nearest 10

    const pad = { top: 10, bottom: 20, left: 40, right: 10 };
    const cw = w - pad.left - pad.right;
    const ch = h - pad.top - pad.bottom;
    const zeroY = pad.top + ch / 2;

    function yPos(val) {
        return zeroY - (val / maxAbs) * (ch / 2);
    }

    // Grid lines
    ctx.strokeStyle = '#21262d';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(pad.left, zeroY);
    ctx.lineTo(w - pad.right, zeroY);
    ctx.stroke();

    // Y axis labels
    ctx.fillStyle = '#484f58';
    ctx.font = '10px monospace';
    ctx.textAlign = 'right';
    ctx.fillText(`${maxAbs}`, pad.left - 4, pad.top + 10);
    ctx.fillText('0', pad.left - 4, zeroY + 4);
    ctx.fillText(`-${maxAbs}`, pad.left - 4, h - pad.bottom);

    // Draw lines
    function drawLine(data, color, dashed) {
        ctx.strokeStyle = color;
        ctx.lineWidth = dashed ? 1 : 1.5;
        ctx.setLineDash(dashed ? [4, 4] : []);
        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = pad.left + (i / (CHART_LEN - 1)) * cw;
            const y = yPos(data[i]);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.setLineDash([]);
    }

    drawLine(chartData.targetL, 'rgba(76,201,240,0.35)', true);
    drawLine(chartData.targetR, 'rgba(247,37,133,0.35)', true);
    drawLine(chartData.leftRpm, '#4cc9f0', false);
    drawLine(chartData.rightRpm, '#f72585', false);
}

// ============================================================================
// Arduino Pin States
// ============================================================================

const PIN_LABELS = {
    0: 'RX', 1: 'TX', 2: 'ENC-A1', 3: 'ENC-A2',
    4: 'ENC-B1', 5: 'ENC-B2', 6: 'ENA', 7: 'IN1',
    8: 'IN2', 9: 'ENB', 10: 'IN3', 11: 'IN4',
    12: '', 13: 'LED'
};

async function refreshPins() {
    const data = await api('/api/debug/pins');
    if (!data || !data.pins) return;

    const grid = document.getElementById('pin-grid');
    grid.innerHTML = '';

    data.pins.forEach(pin => {
        const div = document.createElement('div');
        const modeClass = pin.m === 'PWM' ? 'mode-pwm' : pin.m === 'OUT' ? 'mode-out' : 'mode-in';
        const valClass = pin.v ? 'val-high' : 'val-low';
        div.className = `pin-box ${modeClass} ${valClass}`;

        let valText = pin.v ? 'HIGH' : 'LOW';
        let valColor = pin.v ? '#3fb950' : '#d29922';
        if (pin.pwm >= 0) { valText = `${pin.pwm}`; valColor = '#d29922'; }

        div.innerHTML = `<div class="pin-num">D${pin.p}</div>`
            + `<div class="pin-mode ${modeClass}">${pin.m}</div>`
            + `<div class="pin-val" style="color:${valColor}">${valText}</div>`
            + `<div class="pin-label">${PIN_LABELS[pin.p] || ''}</div>`;
        grid.appendChild(div);
    });

    log(`Pins refreshed: ${data.pins.length} pins`, 'data');
}

// ============================================================================
// Serial Bridge (avrdude passthrough)
// ============================================================================

async function startBridge() {
    const res = await api('/api/bridge/start');
    if (res && res.ok) {
        document.getElementById('bridge-status').textContent = 'ACTIVE (60s timeout)';
        document.getElementById('bridge-status').style.color = '#3fb950';
        log('Serial bridge started — run avrdude now!', 'success');
    }
}

async function stopBridge() {
    const res = await api('/api/bridge/stop');
    if (res && res.ok) {
        document.getElementById('bridge-status').textContent = 'Inactive';
        document.getElementById('bridge-status').style.color = '#8b949e';
        log('Serial bridge stopped', 'info');
    }
}

// ============================================================================
// Firmware Upload
// ============================================================================

function initFirmwareUpload() {
    setupUpload('esp32-file', 'esp32-upload-btn', 'esp32-progress', '/api/ota/esp32');
    setupUpload('avr-file', 'avr-upload-btn', 'avr-progress', '/api/ota/avr');
}

function setupUpload(fileId, btnId, progressId, endpoint) {
    const btn = document.getElementById(btnId);
    const progress = document.getElementById(progressId);

    btn.addEventListener('click', () => {
        const fileInput = document.getElementById(fileId);
        const file = fileInput.files[0];
        if (!file) {
            progress.textContent = 'No file selected';
            progress.className = 'fw-progress error';
            return;
        }

        btn.disabled = true;
        progress.textContent = 'Uploading...';
        progress.className = 'fw-progress';
        log(`Uploading ${file.name} (${file.size} bytes) to ${endpoint}...`);

        const formData = new FormData();
        formData.append('firmware', file);

        const xhr = new XMLHttpRequest();
        xhr.open('POST', `${API}${endpoint}`);

        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const pct = Math.round((e.loaded / e.total) * 100);
                progress.textContent = `Uploading... ${pct}%`;
            }
        };

        xhr.onload = () => {
            try {
                const res = JSON.parse(xhr.responseText);
                if (res.ok) {
                    progress.textContent = res.msg || 'Success!';
                    progress.className = 'fw-progress';
                    log(`Upload OK: ${res.msg || 'done'}`, 'success');
                } else {
                    progress.textContent = `Error: ${res.error}`;
                    progress.className = 'fw-progress error';
                    log(`Upload error: ${res.error}`, 'error');
                }
            } catch {
                progress.textContent = xhr.status === 200 ? 'Done!' : `HTTP ${xhr.status}`;
                progress.className = xhr.status === 200 ? 'fw-progress' : 'fw-progress error';
            }
            btn.disabled = false;
        };

        xhr.onerror = () => {
            if (endpoint.includes('esp32')) {
                progress.textContent = 'Rebooting ESP32... reload page in 10s';
                progress.className = 'fw-progress';
                log('ESP32 rebooting after OTA...', 'warn');
            } else {
                progress.textContent = 'Upload failed (connection error)';
                progress.className = 'fw-progress error';
                log('Upload failed', 'error');
            }
            btn.disabled = false;
        };

        xhr.send(formData);
    });
}

// ============================================================================
// Init
// ============================================================================

// Coast mode toggle (persisted via localStorage, read by control page)
const coastCheckbox = document.getElementById('coast-checkbox');
coastCheckbox.checked = localStorage.getItem('coastMode') === 'true';
coastCheckbox.addEventListener('change', (e) => {
    localStorage.setItem('coastMode', e.target.checked ? 'true' : 'false');
    log(`Coast mode ${e.target.checked ? 'enabled' : 'disabled'}`, 'success');
});

log('Admin panel initialized', 'info');
log(`Connecting to ${API}...`);
startPolling();
loadPid();
initFirmwareUpload();
