import express from 'express';
import cors from 'cors';
import helmet from 'helmet';
import { ConfigService } from './services/ConfigService';
import { SerialService } from './services/SerialService';
import toolRoutes from './routes/toolRoutes';

const app = express();

// Security & Middleware
app.use(helmet());
app.use(cors());
app.use(express.json());

// Routes
app.use('/api', toolRoutes);

// Root health check
app.get('/', (req, res) => {
    res.json({ status: "online", device: SerialService.getInstance().isOpen() ? "connected" : "disconnected" });
});

async function start() {
    try {
        const serialService = SerialService.getInstance();
        await serialService.init();

        app.listen(ConfigService.PORT, () => {
            console.log(`Backend server running on http://localhost:${ConfigService.PORT}`);
            console.log(`API Key enabled: ${ConfigService.API_KEY.substring(0, 3)}...`);
        });
    } catch (error) {
        console.error('Failed to start server:', error);
        process.exit(1);
    }
}

start();
