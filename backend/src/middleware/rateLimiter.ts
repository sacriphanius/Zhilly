import rateLimit from 'express-rate-limit';
import { ConfigService } from '../services/ConfigService';

export const limiter = rateLimit({
    windowMs: ConfigService.RATE_LIMIT_WINDOW_MS,
    max: ConfigService.RATE_LIMIT_MAX_REQUESTS,
    message: {
        ok: false,
        error: { message: "Too many requests, please try again later." }
    },
    standardHeaders: true,
    legacyHeaders: false,
});
