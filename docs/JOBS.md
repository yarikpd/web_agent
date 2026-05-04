# Job File Documentation

Job files are JSON files that describe a task to be executed by the agent. Each file defines the command to run, its parameters, and post-execution behavior.

Job files are stored in the directory specified by `JOBS_DIR` (default: `jobs/`). The file name is used as the task identifier (e.g., `TASK.json` → task `TASK`).

## File Structure

```json
{
  "name": "my_job",
  "description": "Task description",
  "command": {
    "linux": "command for Linux",
    "macos": "command for macOS",
    "windows": "command for Windows"
  },
  "parameters": {
    "param_name": {
      "description": "Parameter description",
      "required": true,
      "type": "string"
    }
  },
  "return_files": false,
  "restart_after": false,
  "return": {
    "description": "Return value description",
    "type": "string",
    "source": "command_output"
  }
}
```

## Fields

### `name` (string, required)
Unique task name. Must match the file name (without `.json`).

### `description` (string, optional)
Human-readable description of the task.

### `command` (object, required)
An object containing commands for different operating systems. Keys: `linux`, `macos`, `windows`. The value is a shell command string.

The command may contain placeholders in the form `{param_name}`, which are replaced with the actual parameter values at runtime. If a parameter is optional and not provided, the placeholder is replaced with an empty string.

All three OS keys are supported (`linux`, `macos`, `windows`). The agent selects the command matching the current platform.

### `parameters` (object, required)
Defines the parameters the task accepts. Keys are parameter names (must match placeholders in the command). Values are objects with the following fields:

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `description` | string | no | Parameter description |
| `required` | bool | no | Whether the parameter is required (default: `false`) |
| `type` | string | yes | Parameter type (see below) |

**Supported parameter types:**
- `string` — any string
- `int` / `integer` — integer number
- `float` / `double` / `number` — floating-point number
- `bool` / `boolean` — boolean value (`true`/`false` or `1`/`0`)

Passing an argument not declared in `parameters` results in an error. The same applies if a required parameter is missing.

### `return_files` (bool, optional, default: `false`)
If `true`, the agent scans the passed arguments for file paths, waits for them to appear/stabilize on disk (up to 3 seconds), and uploads them to the server along with the execution result.

Useful for tasks that create or modify files.

### `restart_after` (bool, optional, default: `false`)
If `true`, after successful task execution and result delivery, the agent restarts itself. Used for tasks that modify the agent's configuration (e.g., `CONF`, `TIMEOUT`) so the new settings take effect.

### `return` (object, optional)
Describes the type of value the task returns.

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | Return type: `string`, `bool`, `int`, `float` |
| `source` | string | Value source: `command_output` (stdout) or `command_success` (exit status) |
| `description` | string | Return value description |

Default type is `string`.

## Examples

### Simple task (directory.json)
```json
{
  "name": "directory",
  "description": "Get files in a directory",
  "command": {
    "linux": "ls -l {directory}",
    "macos": "ls -l {directory}",
    "windows": "dir {directory}"
  },
  "parameters": {
    "directory": {
      "description": "The directory to list",
      "required": true,
      "type": "string"
    }
  },
  "return": {
    "type": "string",
    "source": "command_output"
  }
}
```

Call: `f("directory", {{"directory", "/home/user"}})` — runs `ls -l /home/user`.

### Task with file return (TASK.json)
```json
{
  "name": "TASK",
  "description": "Run a program with optional arguments and return its output",
  "command": {
    "linux": "{program} {args}",
    "macos": "{program} {args}",
    "windows": "start /wait \"\" \"{program}\" {args}"
  },
  "return_files": true,
  "parameters": {
    "program": {
      "description": "Program or command to run",
      "required": true,
      "type": "string"
    },
    "args": {
      "description": "Optional command-line arguments",
      "required": false,
      "type": "string"
    }
  },
  "return": {
    "type": "string",
    "source": "command_output"
  }
}
```

Call: `f("TASK", {{"program", "gcc"}, {"args", "-o prog main.c"}})` — runs `gcc -o prog main.c`.

### Task with restart (TIMEOUT.json)
```json
{
  "name": "TIMEOUT",
  "description": "Update polling interval in .env",
  "command": {
    "linux": "sed -i 's/^POLL_INTERVAL_SECONDS=.*/POLL_INTERVAL_SECONDS={timeout}/' .env",
    "macos": "sed -i.bak 's/^POLL_INTERVAL_SECONDS=.*/POLL_INTERVAL_SECONDS={timeout}/' .env && rm -f .env.bak",
    "windows": "powershell -Command \"...\""
  },
  "return_files": false,
  "restart_after": true,
  "parameters": {
    "timeout": {
      "description": "Polling interval in seconds",
      "required": true,
      "type": "int"
    }
  },
  "return": {
    "type": "bool",
    "source": "command_success"
  }
}
```

Call: `f("TIMEOUT", {{"timeout", "10"}})` — updates the polling interval and restarts the agent.
