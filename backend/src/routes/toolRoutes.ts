import { Router } from 'express';
import { ToolController } from '../controllers/ToolController';
import { authMiddleware } from '../middleware/auth';
import { limiter } from '../middleware/rateLimiter';

const router = Router();

router.post('/call', limiter, authMiddleware, ToolController.handleToolCall);

export default router;
