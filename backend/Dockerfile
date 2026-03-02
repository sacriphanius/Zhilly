FROM node:18-slim AS builder

WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
RUN npm run build

FROM node:18-slim

WORKDIR /app
COPY package*.json ./
RUN npm install --production
COPY --from=builder /app/dist ./dist
COPY .env.example .env

EXPOSE 3000

# Note: You need to run this container with --device /dev/ttyACM0
CMD ["node", "dist/server.ts"]
