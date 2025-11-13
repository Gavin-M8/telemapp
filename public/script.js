// Connect to the server
const socket = io();

// HTML elements
const xEl = document.getElementById("x-val");
const yEl = document.getElementById("y-val");
const zEl = document.getElementById("z-val");

// Receive accelerometer data from server
socket.on("accelData", (data) => {
  const { x, y, z } = data;

  // Update display
  xEl.textContent = x.toFixed(3);
  yEl.textContent = y.toFixed(3);
  zEl.textContent = z.toFixed(3);
});
