import './style.css';

const MAX_POINTS = 200;

// Local speed history (sampled from /api/stats since backend has no speed time-series)
const speedHistory = { labels: [], values: [] };

// ── Chart defaults ────────────────────────────────────────────────────────────
Chart.defaults.color = '#7ab8d4';
Chart.defaults.borderColor = 'rgba(0,212,255,0.08)';
Chart.defaults.font.family = "'Share Tech Mono', monospace";

const gridStyle = { color: 'rgba(0,212,255,0.07)', drawTicks: false };
const tickStyle = { color: '#4a7a94', maxTicksLimit: 6, padding: 8 };

// ── Acceleration chart ────────────────────────────────────────────────────────
const accelChart = new Chart(
  document.getElementById('accelChart').getContext('2d'),
  {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { label: 'Ax', data: [], borderColor: '#ff5577', backgroundColor: 'rgba(255,85,119,0.05)', borderWidth: 1.5, fill: true, tension: 0.3, pointRadius: 0 },
        { label: 'Ay', data: [], borderColor: '#00ff88', backgroundColor: 'rgba(0,255,136,0.05)', borderWidth: 1.5, fill: true, tension: 0.3, pointRadius: 0 },
        { label: 'Az', data: [], borderColor: '#4488ff', backgroundColor: 'rgba(68,136,255,0.05)', borderWidth: 1.5, fill: true, tension: 0.3, pointRadius: 0 },
      ],
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: true,
      aspectRatio: 2.5,
      scales: {
        x: { type: 'category', ticks: { ...tickStyle, maxRotation: 0, autoSkip: true, maxTicksLimit: 8 }, grid: gridStyle },
        y: { title: { display: true, text: 'g', color: '#4a7a94' }, ticks: tickStyle, grid: gridStyle },
      },
      plugins: { legend: { display: false } },
    },
  }
);

// ── Speed chart ───────────────────────────────────────────────────────────────
const speedChart = new Chart(
  document.getElementById('speedChart').getContext('2d'),
  {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Speed',
        data: [],
        borderColor: '#00d4ff',
        backgroundColor: 'rgba(0,212,255,0.06)',
        borderWidth: 2,
        fill: true,
        tension: 0.4,
        pointRadius: 0,
      }],
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: true,
      aspectRatio: 2.5,
      scales: {
        x: { type: 'category', ticks: { ...tickStyle, maxRotation: 0, autoSkip: true, maxTicksLimit: 8 }, grid: gridStyle },
        y: { min: 0, title: { display: true, text: 'mph', color: '#4a7a94' }, ticks: tickStyle, grid: gridStyle },
      },
      plugins: { legend: { display: false } },
    },
  }
);

// ── Clock ─────────────────────────────────────────────────────────────────────
function updateClock() {
  const now = new Date();
  document.getElementById('clock').textContent =
    now.toTimeString().slice(0, 8);
}
setInterval(updateClock, 1000);
updateClock();

// ── Fetch acceleration time series ───────────────────────────────────────────
async function fetchData() {
  try {
    const res = await fetch('/data');
    const buf = await res.json();

    const labels = buf.map(d => d.human_ts.slice(11, 19)).slice(-MAX_POINTS);
    accelChart.data.labels             = labels;
    accelChart.data.datasets[0].data   = buf.map(d => d.ax).slice(-MAX_POINTS);
    accelChart.data.datasets[1].data   = buf.map(d => d.ay).slice(-MAX_POINTS);
    accelChart.data.datasets[2].data   = buf.map(d => d.az).slice(-MAX_POINTS);
    accelChart.update('none');
  } catch (err) {
    console.error('fetchData:', err);
  }
}

// ── Fetch stats + update everything ──────────────────────────────────────────
const MAX_SPEED = 80; // mph — top of speed bar scale
const MAX_ACCEL_G = 1.5;

async function updateStats() {
  try {
    const [statsRes, currentRes] = await Promise.all([
      fetch('/api/stats'),
      fetch('/api/current'),
    ]);
    const stats   = await statsRes.json();
    const current = await currentRes.json();

    // ── Primary tracker: speed ────────────────────────────────────────────
    const mph = stats.velocity_mph;
    document.getElementById('velocity').textContent = mph.toFixed(1);
    document.getElementById('speedBar').style.width =
      Math.min(100, (mph / MAX_SPEED) * 100).toFixed(1) + '%';

    // Speed history chart
    const now = new Date().toTimeString().slice(0, 8);
    speedHistory.labels.push(now);
    speedHistory.values.push(mph);
    if (speedHistory.labels.length > MAX_POINTS) {
      speedHistory.labels.shift();
      speedHistory.values.shift();
    }
    speedChart.data.labels           = speedHistory.labels;
    speedChart.data.datasets[0].data = speedHistory.values;
    speedChart.update('none');

    // ── Primary tracker: current accel ───────────────────────────────────
    const ax = current.ax;
    document.getElementById('currentAccel').textContent = ax.toFixed(2);
    document.getElementById('accelBar').style.width =
      Math.min(100, (Math.abs(ax) / MAX_ACCEL_G) * 100).toFixed(1) + '%';

    const accelCard = document.getElementById('currentAccelCard');
    accelCard.classList.toggle('highlight', stats.in_braking || stats.in_corner);

    // ── Secondary stats ───────────────────────────────────────────────────
    document.getElementById('maxAccel').innerHTML   = `${stats.max_accel.toFixed(2)}<span class="stat-unit">g</span>`;
    document.getElementById('maxBrake').innerHTML   = `${stats.max_brake.toFixed(2)}<span class="stat-unit">g</span>`;
    document.getElementById('maxLateral').innerHTML = `${stats.max_lateral.toFixed(2)}<span class="stat-unit">g</span>`;
    document.getElementById('totalCorners').textContent      = stats.total_corners;
    document.getElementById('totalBraking').textContent      = stats.total_braking_events;

  } catch (err) {
    console.error('updateStats:', err);
  }
}

// ── Status indicator ──────────────────────────────────────────────────────────
async function updateStatus() {
  try {
    const res  = await fetch('/api/status');
    const data = await res.json();
    const dot  = document.getElementById('statusDot');
    const txt  = document.getElementById('statusText');
    if (data.running) {
      dot.classList.add('online');
      txt.textContent = 'ONLINE';
    } else {
      dot.classList.remove('online');
      txt.textContent = 'OFFLINE';
    }
  } catch {
    document.getElementById('statusDot').classList.remove('online');
    document.getElementById('statusText').textContent = 'NO SIGNAL';
  }
}

// ── Poll ──────────────────────────────────────────────────────────────────────
setInterval(fetchData,     100);
setInterval(updateStats,   100);
setInterval(updateStatus, 1000);

// ── Controls ──────────────────────────────────────────────────────────────────
document.getElementById('btnStart').addEventListener('click', async () => {
  await fetch('/api/start', { method: 'POST' });
  updateStatus();
});

document.getElementById('btnStop').addEventListener('click', async () => {
  await fetch('/api/stop', { method: 'POST' });
  updateStatus();
});

document.getElementById('btnNewLog').addEventListener('click', async () => {
  try {
    const res  = await fetch('/api/start_log', { method: 'POST' });
    const data = await res.json();
    document.getElementById('currentFile').textContent = 'LOG: ' + data.file;
    alert('New log started: ' + data.file);
  } catch (err) { console.error(err); }
});

document.getElementById('btnStopLog').addEventListener('click', async () => {
  try {
    await fetch('/api/stop_log', { method: 'POST' });
    document.getElementById('currentFile').textContent = 'LOG: none';
    alert('Logging stopped.');
  } catch (err) { console.error(err); }
});

document.getElementById('btnResetStats').addEventListener('click', async () => {
  if (!confirm('Reset all performance statistics?')) return;
  speedHistory.labels.length = 0;
  speedHistory.values.length = 0;
  try {
    await fetch('/api/reset_stats', { method: 'POST' });
  } catch (err) { console.error(err); }
});

document.getElementById('btnDeleteLogs').addEventListener('click', async () => {
  if (!confirm('Delete all CSV logs?')) return;
  try {
    const res  = await fetch('/api/delete_logs', { method: 'POST' });
    const data = await res.json();
    document.getElementById('currentFile').textContent = 'LOG: none';
    alert(`Purged ${data.files.length} log files.`);
    updateStatus();
  } catch (err) { console.error(err); }
});
