"""
Parser module for extracting guardrails, blocks, and directives from Gemma source code.

Módulo parser para extrair guardrails, blocos e diretivas do código-fonte do Gemma.
"""

import os
import re
from typing import List, Tuple

Guardrail = Tuple[str, int, List[str]]  # (file_path, line_number, context_lines)

DEFAULT_PATTERNS = {
    "guardrail": re.compile(r"\bguardrail\b", re.IGNORECASE),
    "block": re.compile(r"\bblock\b", re.IGNORECASE),
    "directive": re.compile(r"\bdirective\b", re.IGNORECASE),
}

def parse_guardrails(
    source_dir: str, context: int = 3, patterns: dict = DEFAULT_PATTERNS
) -> List[Guardrail]:
    """
    Walk through source_dir and find lines matching the given patterns.
    Returns a list of tuples with file path, line number, and context lines.

    Percorre source_dir e encontra linhas que batem com os padrões dados.
    Retorna uma lista de tuplas com caminho do arquivo, número da linha e linhas de contexto.
    """
    results: List[Guardrail] = []
    for root, _, files in os.walk(source_dir):
        for fname in files:
            if not fname.endswith((".py", ".js", ".ts", ".txt", ".md")):
                continue
            path = os.path.join(root, fname)
            try:
                with open(path, encoding="utf-8", errors="ignore") as f:
                    lines = f.readlines()
            except OSError:
                continue
            for idx, line in enumerate(lines):
                for key, pat in patterns.items():
                    if pat.search(line):
                        start = max(idx - context, 0)
                        end = min(idx + context + 1, len(lines))
                        results.append((path, idx + 1, lines[start:end]))
                        break
    return results
