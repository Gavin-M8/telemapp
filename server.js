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

server.listen(3000, () => {
  console.log("Server running at http://localhost:3000");
});
