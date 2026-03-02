# T-Embed CC1101 MCP Backend

This is a production-ready Node.js backend for controlling a T-Embed CC1101 device via Model Context Protocol (MCP) tool calling, designed for integration with Xiaozhi.

## Features

- **TypeScript** core for type safety.
- **SerialPort** communication with JSON framing.
- **AJV** strict schema validation for all RF parameters.
- **API Key** authentication and **Rate Limiting**.
- **PM2** and **Docker** ready.

## Setup

1. **Install dependencies:**
   ```bash
   cd backend
   npm install
   ```

2. **Configure environment:**
   Edit `.env` (copied from `.env.example`):
   ```env
   PORT=3000
   SERIAL_PORT=/dev/ttyACM0
   BAUD_RATE=115200
   API_KEY=your_secret_key
   ```

3. **Build and Run:**
   ```bash
   npm run build
   npm start
   ```

## Xiaozhi Console Configuration

In your Xiaozhi console screenshot (MCP Settings -> Custom Services):

1. Click **"Get MCP Endpoint"** (or Add).
2. **Endpoint URL:** `http://<YOUR_SERVER_IP>:3000/api/call`
3. **Authentication:** Add a header `x-api-key` with the value you set in `.env`.
4. **Tools Mapping:** The backend response follows the strict format Xiaozhi expects.

## Implemented Tools

- `system.get_status`
- `cc1101.set_frequency(mhz: number)` (300-928 MHz)
- `cc1101.set_modulation(type: "ASK" | "FSK" | "GFSK" | "OOK")`
- `cc1101.rx_start(timeout_ms: number)` (Max 60,000ms)
- `cc1101.rx_stop()`
- `cc1101.read_rssi()`

## Example Requests

### Get Status
```bash
curl -X POST http://localhost:3000/api/call \
  -H "Content-Type: application/json" \
  -H "x-api-key: tembed_secret_key_2024" \
  -d '{
    "tool": "system.get_status",
    "id": "req_001",
    "args": {}
  }'
```

### Set Frequency
```bash
curl -X POST http://localhost:3000/api/call \
  -H "Content-Type: application/json" \
  -H "x-api-key: tembed_secret_key_2024" \
  -d '{
    "tool": "cc1101.set_frequency",
    "id": "req_002",
    "args": { "mhz": 433.92 }
  }'
```

## Running with PM2
```bash
pm2 start pm2.config.js
```

## Running with Docker
```bash
docker build -t tembed-backend .
docker run -d --device /dev/ttyACM0 -p 3000:3000 tembed-backend
```
