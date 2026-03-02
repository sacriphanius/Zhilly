import Ajv from 'ajv';
import { toolSchemas } from '../schemas/toolSchemas';

const ajv = new Ajv();

export class Validator {
    public static validate(tool: string, args: any): { valid: boolean; errors?: any } {
        const schema = toolSchemas[tool];
        if (!schema) {
            return { valid: false, errors: `Unknown tool: ${tool}` };
        }

        const validateFn = ajv.compile(schema);
        const valid = validateFn(args);

        if (!valid) {
            return { valid: false, errors: validateFn.errors };
        }

        return { valid: true };
    }
}
