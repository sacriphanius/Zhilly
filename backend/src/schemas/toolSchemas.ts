import { JSONSchemaType } from 'ajv';

export interface SetFrequencyArgs {
    mhz: number;
}

export interface SetModulationArgs {
    type: "ASK" | "FSK" | "GFSK" | "OOK";
}

export interface RxStartArgs {
    timeout_ms: number;
}

export const toolSchemas: Record<string, any> = {
    "system.get_status": {
        type: "object",
        properties: {},
        additionalProperties: false
    },
    "cc1101.set_frequency": {
        type: "object",
        properties: {
            mhz: { type: "number", minimum: 300, maximum: 928 }
        },
        required: ["mhz"],
        additionalProperties: false
    },
    "cc1101.set_modulation": {
        type: "object",
        properties: {
            type: { type: "string", enum: ["ASK", "FSK", "GFSK", "OOK"] }
        },
        required: ["type"],
        additionalProperties: false
    },
    "cc1101.rx_start": {
        type: "object",
        properties: {
            timeout_ms: { type: "number", minimum: 1, maximum: 60000 }
        },
        required: ["timeout_ms"],
        additionalProperties: false
    },
    "cc1101.rx_stop": {
        type: "object",
        properties: {},
        additionalProperties: false
    },
    "cc1101.read_rssi": {
        type: "object",
        properties: {},
        additionalProperties: false
    }
};
