"""
Output module for dumping selected guardrails to a file or stdout.

Módulo de saída para gravar guardrails selecionados em arquivo ou stdout.
"""

from typing import List, Tuple

Guardrail = Tuple[str, int, List[str]]

def output_guardrails(
    guardrails: List[Guardrail], out_file: str = None
) -> None:
    """
    Output the selected guardrails to stdout or to a specified file.

    Grava os guardrails selecionados no stdout ou em um arquivo especificado.
    """
    lines = []
    for path, lineno, context_lines in guardrails:
        header = f"# {path}:{lineno}\n"
        lines.append(header)
        lines.extend(context_lines)
        lines.append("\n")
    content = "".join(lines)
    if out_file:
        with open(out_file, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Guardrails written to {out_file}")
    else:
        print(content)
