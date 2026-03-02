import { Request, Response } from 'express';
import { Validator } from '../utils/validator';
import { SerialService } from '../services/SerialService';
import { ConfigService } from '../services/ConfigService';

export class ToolController {
    public static async handleToolCall(req: Request, res: Response) {
        const { tool, id, args } = req.body;

        // Validate request structure
        if (!tool || !id) {
            return res.status(400).json({
                id: id || "unknown",
                ok: false,
                result: null,
                error: { message: "Missing tool or id" }
            });
        }

        // Validate args
        const validation = Validator.validate(tool, args || {});
        if (!validation.valid) {
            return res.status(400).json({
                id,
                ok: false,
                result: null,
                error: { message: "Validation failed", details: validation.errors }
            });
        }

        try {
            const serialService = SerialService.getInstance();
            if (!serialService.isOpen()) {
                return res.status(503).json({
                    id,
                    ok: false,
                    result: null,
                    error: { message: "Serial port not connected" }
                });
            }

            const response = await serialService.sendCommand(tool, id, args || {});
            return res.status(200).json(response);

        } catch (error: any) {
            return res.status(500).json({
                id,
                ok: false,
                result: null,
                error: { message: error.message }
            });
        }
    }
}
