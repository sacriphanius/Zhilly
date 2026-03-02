import { Request, Response, NextFunction } from 'express';
import { ConfigService } from '../services/ConfigService';

export const authMiddleware = (req: Request, res: Response, next: NextFunction) => {
    const apiKey = req.headers['x-api-key'];

    if (!apiKey || apiKey !== ConfigService.API_KEY) {
        return res.status(401).json({
            id: req.body.id || "unknown",
            ok: false,
            result: null,
            error: { message: "Unauthorized: Invalid API Key" }
        });
    }

    next();
};
