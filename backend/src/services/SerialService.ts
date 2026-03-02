import { SerialPort } from 'serialport';
import { ReadlineParser } from '@serialport/parser-readline';
import { ConfigService } from './ConfigService';

export interface SerialResponse {
    id: string;
    ok: boolean;
    result: any;
    error: any;
}

export class SerialService {
    private port: SerialPort | null = null;
    private parser: ReadlineParser | null = null;
    private static instance: SerialService;
    private pendingRequests: Map<string, (response: SerialResponse) => void> = new Map();

    private constructor() { }

    public static getInstance(): SerialService {
        if (!SerialService.instance) {
            SerialService.instance = new SerialService();
        }
        return SerialService.instance;
    }

    public async init(): Promise<void> {
        return new Promise((resolve, reject) => {
            try {
                this.port = new SerialPort({
                    path: ConfigService.SERIAL_PORT,
                    baudRate: ConfigService.BAUD_RATE,
                    autoOpen: false,
                });

                this.parser = this.port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

                this.port.open((err) => {
                    if (err) {
                        console.error(`Failed to open serial port: ${err.message}`);
                        return reject(err);
                    }
                    console.log(`Connected to T-Embed on ${ConfigService.SERIAL_PORT}`);
                    resolve();
                });

                this.parser.on('data', (data: string) => {
                    if (ConfigService.DEBUG) {
                        console.log(`[SERIAL RAW] ${data}`);
                    }
                    try {
                        const response: SerialResponse = JSON.parse(data);
                        if (response.id && this.pendingRequests.has(response.id)) {
                            const callback = this.pendingRequests.get(response.id);
                            if (callback) {
                                callback(response);
                                this.pendingRequests.delete(response.id);
                            }
                        }
                    } catch (e) {
                        // Not a JSON or invalid format, ignore or log
                        if (ConfigService.DEBUG) {
                            console.warn(`[SERIAL ERR] Failed to parse JSON: ${data}`);
                        }
                    }
                });

            } catch (error) {
                reject(error);
            }
        });
    }

    public async sendCommand(tool: string, id: string, args: any): Promise<SerialResponse> {
        if (!this.port || !this.port.isOpen) {
            throw new Error('Serial port is not open');
        }

        const command = JSON.stringify({ tool, id, args }) + '\n';

        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                if (this.pendingRequests.has(id)) {
                    this.pendingRequests.delete(id);
                    reject(new Error('Device timeout (no ACK)'));
                }
            }, 5000); // 5s default timeout for ACK

            this.pendingRequests.set(id, (response) => {
                clearTimeout(timeout);
                resolve(response);
            });

            this.port?.write(command, (err) => {
                if (err) {
                    clearTimeout(timeout);
                    this.pendingRequests.delete(id);
                    reject(err);
                }
            });
        });
    }

    public isOpen(): boolean {
        return this.port?.isOpen || false;
    }
}
