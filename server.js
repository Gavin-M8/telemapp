const express = require("express");
const http = require("http");
const { Server } = require("socket.io");
const bodyParser = require("body-parser");

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(bodyParser.json());
app.use(express.static("public"));

// Receive accelerometer data from ESP32
app.post("/data", (req, res) => {
  const { x, y, z } = req.body;
  io.emit("accelData", { x, y, z }); // send to all connected clients
  res.sendStatus(200);
});

// Temporary fake data generator (remove when using ESP32)
setInterval(() => {
  const fake = {
    x: (Math.random() * 2 - 1).toFixed(3),
    y: (Math.random() * 2 - 1).toFixed(3),
    z: (Math.random() * 2 - 1).toFixed(3),
  };

  io.emit("accelData", fake);
}, 200); // every 200ms


server.listen(3000, () => {
  console.log("Server running at http://localhost:3000");
});
