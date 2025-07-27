"""
CLI entry point for Gemma guardrails manager.

Ponto de entrada CLI para o gerenciador de guardrails do Gemma.
"""

import argparse
from interactive import run_tui
from framework import (
    run_inference,
    external_query,
    use_tool,
    manage_quantization,
    manage_weights,
)

def main():
    parser = argparse.ArgumentParser(
    description=(
        "Gemma Orchestrator / Orquestrador Gemma: scan guardrails, inference, queries, tools, quant, weights."
    )
    )
    subparsers = parser.add_subparsers(dest="command")

    scan_p = subparsers.add_parser(
        "scan",
        help="Launch interactive TUI (scan, inference, queries, tools...). Inicia interface TUI completa."
    )
    scan_p.add_argument("--repo", default="https://github.com/openai/gemma.git", help="Gemma repository URL. URL do repositório Gemma.")
    scan_p.add_argument("--branch", default=None, help="Branch or tag to checkout. Branch ou tag para checkout.")
    scan_p.add_argument("--dest", default="gemma_src", help="Destination directory for Gemma code. Diretório de destino para o código Gemma.")
    scan_p.add_argument("--context", type=int, default=3, help="Context lines around matches. Linhas de contexto em torno das ocorrências.")
    scan_p.add_argument("--out", default=None, help="Output file for selected guardrails. Arquivo de saída para guardrails selecionados.")

    infer_p = subparsers.add_parser(
        "infer",
        help="Run model inference with options. Executa inferência do modelo com opções."
    )
    infer_p.add_argument("--model", required=True, help="Model identifier or path. Identificador ou caminho do modelo.")
    infer_p.add_argument("--prompt", required=True, help="Prompt for inference. Prompt para inferência.")
    infer_p.add_argument("--quant", choices=["int8", "fp16", "fp32"], help="Quantization type. Tipo de quantização.")
    infer_p.add_argument("--weights", nargs="*", help="List of weight files. Lista de arquivos de peso.")
    infer_p.add_argument("--external-query", help="External data query. Consulta externa de dados.")
    infer_p.add_argument("--tool", help="Tool to invoke pre-inference. Tool a ser invocada antes da inferência.")

    query_p = subparsers.add_parser(
        "query",
        help="External data query. Consulta externa de dados."
    )
    query_p.add_argument("--endpoint", required=True, help="Query endpoint. Endpoint da consulta.")
    query_p.add_argument("--query", required=True, help="Query string or params. String de consulta ou parâmetros.")

    tool_p = subparsers.add_parser(
        "tool",
        help="Invoke a named tool. Invoca uma tool."
    )
    tool_p.add_argument("--name", required=True, help="Tool name. Nome da tool.")
    tool_p.add_argument("--input", help="Input data for tool. Dados de entrada para a tool.")

    quant_p = subparsers.add_parser(
        "quantize",
        help="Convert model quantization. Converte quantização do modelo."
    )
    quant_p.add_argument("--model", required=True, help="Source model path. Caminho do modelo fonte.")
    quant_p.add_argument("--to", required=True, help="Target quantization. Quantização alvo.")
    quant_p.add_argument("--out-model", required=True, help="Output model path. Caminho do modelo de saída.")

    wt_p = subparsers.add_parser(
        "weights",
        help="Manage weight files. Gerencia arquivos de peso."
    )
    wt_p.add_argument(
        "--action",
        choices=["add", "remove", "list"],
        required=True,
        help="Action add/remove/list. Ação adicionar/remover/listar."
    )
    wt_p.add_argument(
        "--file",
        required=False,
        help="Weight file path (not needed for list). Caminho do arquivo de peso (não necessário para list)."
    )

    cfg_p = subparsers.add_parser(
        "configure",
        help="Run configure script to install system dependencies or show status."
    )
    cfg_p.add_argument(
        "--status",
        action="store_true",
        help="Show current system configuration without installing."
    )

    tui_p = subparsers.add_parser(
        "tui",
        help="Launch full interactive TUI. Inicia a interface TUI completa."
    )

    args = parser.parse_args()

    if args.command == "scan" or args.command is None:
        # Launch full TUI for operations (scan guardrails, inference, etc.)
        run_tui()
    elif args.command == "infer":
        run_inference(
            args.model,
            args.prompt,
            quant=args.quant,
            weights=args.weights,
            external_query=args.external_query,
            tool=args.tool,
        )
    elif args.command == "query":
        external_query(args.query, args.endpoint)
    elif args.command == "tool":
        use_tool(args.name, args.input)
    elif args.command == "quantize":
        manage_quantization(args.model, args.to, args.out_model)
    elif args.command == "weights":
        manage_weights(args.action, args.file)
    elif args.command == "tui":
        from interactive import run_tui
        run_tui()
    elif args.command == "configure":
        import subprocess, os, sys
        script = os.path.join(os.getcwd(), "gemma_src", "configure")
        cmd = ["bash", script]
        if args.status:
            cmd.append("--status")
        subprocess.run(cmd, check=True)

if __name__ == "__main__":
    main()
