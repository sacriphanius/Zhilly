module.exports = {
    apps: [{
        name: "tembed-mcp-backend",
        script: "./dist/server.js",
        env: {
            NODE_ENV: "production",
            PORT: 3000
        }
    }]
}
