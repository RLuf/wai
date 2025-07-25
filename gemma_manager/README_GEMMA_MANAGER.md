# Gemma Guardrails Manager

This tool allows downloading the Gemma source code, scanning for guardrails,
blocks, and directives, and interactively selecting which ones to keep or remove.

## Architecture

- **gemma_manager/downloader.py**: Clone or update the Gemma repository.
- **gemma_manager/parser.py**: Parse source files for configurable patterns.
- **gemma_manager/interactive.py**: Interactive CLI for user selections.
- **gemma_manager/output.py**: Output the selected contexts to file or stdout.
- **gemma_manager/cli.py**: Main entry point gluing all modules.
- **manage_gemma.py**: Executable script wrapper for convenience.

## Usage

```bash
./manage_gemma.py [--repo URL] [--branch BRANCH] [--dest DIR]
                 [--context N] [--out OUTPUT_FILE]
```

By default, it clones `https://github.com/openai/gemma.git` into `./gemma_src`,
uses a context window of 3 lines, and prints results to stdout.

## Extensibility

- Add new match patterns in `parser.py` under `DEFAULT_PATTERNS`.
- Extend or customize the interactive flow in `interactive.py`.
- Modify output formatting or file targets in `output.py`.
- Enhance command-line options in `cli.py`.

## Integration with Fazai

The selected guardrails can be used as the new model-driven engine for Fazai.
For example, update `/opt/fazai/lib/main.js` to load the contexts:

```js
const fs = require('fs');
const contexts = fs.readFileSync('/path/to/output', 'utf-8');
// TODO: feed `contexts` into the model adapter logic
```

Then adjust the launcher `/opt/fazai/bin/fazai` to invoke your model engine.

---
