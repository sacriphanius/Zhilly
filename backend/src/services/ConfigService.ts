import dotenv from 'dotenv';
import path from 'path';

dotenv.config();

export class ConfigService {
    public static get PORT(): number {
        return parseInt(process.env.PORT || '3000', 10);
    }

    public static get SERIAL_PORT(): string {
        return process.env.SERIAL_PORT || '/dev/ttyACM0';
    }

    public static get BAUD_RATE(): number {
        return parseInt(process.env.BAUD_RATE || '115200', 10);
    }

    public static get API_KEY(): string {
        return process.env.API_KEY || 'tembed_secret_key_2024';
    }

    public static get RATE_LIMIT_WINDOW_MS(): number {
        return parseInt(process.env.RATE_LIMIT_WINDOW_MS || '60000', 10);
    }

    public static get RATE_LIMIT_MAX_REQUESTS(): number {
        return parseInt(process.env.RATE_LIMIT_MAX_REQUESTS || '100', 10);
    }

    public static get DEBUG(): boolean {
        return process.env.DEBUG === 'true';
    }
}
