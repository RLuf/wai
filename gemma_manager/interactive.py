"""
Interactive full-screen TUI for orchestrating commands: scan guardrails, inference,
external queries, tools, quantization, and weight management with logs and colors.

Interface TUI completa para orquestração: scan de guardrails, inferência,
consultas externas, tools, quantização e gerenciamento de pesos com logs e cores.
"""
import curses
import textwrap

from downloader import clone_gemma
from parser import parse_guardrails
from output import output_guardrails
from framework import (
    run_inference,
    external_query,
    use_tool,
    manage_quantization,
    manage_weights,
)

MENU_ITEMS = [
    "Scan Guardrails",
    "Inference",
    "External Query",
    "Tool",
    "Quantize Model",
    "Manage Weights",
    "Exit",
]

def run_tui():
    """
    Launch the full-screen TUI. Requires a valid terminal (TTY and TERM set).
    """
    import os
    import sys

    # Ensure running in a terminal environment
    if not sys.stdin.isatty() or 'TERM' not in os.environ:
        print(
            "Error: Unable to initialize TUI. Please run this command in a terminal "
            "with a valid TERM environment variable set.",
            file=sys.stderr,
        )
        sys.exit(1)
    try:
        curses.wrapper(_main)
    except curses.error as e:
        print(f"Error initializing terminal: {e}", file=sys.stderr)
        sys.exit(1)

def _main(stdscr):
    # Initialize colors
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)   # menu highlight
    curses.init_pair(2, curses.COLOR_YELLOW, -1)                # logs
    curses.init_pair(3, curses.COLOR_GREEN, -1)                 # success
    curses.init_pair(4, curses.COLOR_RED, -1)                   # error

    k = 0
    selected = 0
    logs = []
    guardrails = []

    while True:
        stdscr.erase()
        max_y, max_x = stdscr.getmaxyx()

        # Draw menu
        menu_win = stdscr.subwin(len(MENU_ITEMS) + 2, 30, 1, 1)
        menu_win.box()
        for idx, item in enumerate(MENU_ITEMS):
            if idx == selected:
                menu_win.attron(curses.color_pair(1))
                menu_win.addstr(idx + 1, 2, item)
                menu_win.attroff(curses.color_pair(1))
            else:
                menu_win.addstr(idx + 1, 2, item)

        # Draw log window
        log_h = 5
        log_win = stdscr.subwin(log_h, max_x - 2, max_y - log_h - 1, 1)
        log_win.box()
        for i, line in enumerate(logs[-(log_h - 2):]):
            log_win.attron(curses.color_pair(2))
            log_win.addnstr(i + 1, 2, line, max_x - 6)
            log_win.attroff(curses.color_pair(2))

        stdscr.refresh()

        key = stdscr.getch()
        if key in (curses.KEY_UP, ord('k')):
            selected = (selected - 1) % len(MENU_ITEMS)
        elif key in (curses.KEY_DOWN, ord('j')):
            selected = (selected + 1) % len(MENU_ITEMS)
        elif key in (10, 13):  # Enter
            choice = MENU_ITEMS[selected]
            if choice == "Exit":
                break
            _handle_choice(stdscr, choice, logs, guardrails)
        elif key in (ord('q'), ord('Q')):
            break

def _handle_choice(stdscr, choice, logs, guardrails):
    max_y, max_x = stdscr.getmaxyx()
    stdscr.erase()
    stdscr.addstr(1, 2, f"[ {choice} ]", curses.A_BOLD)

    def prompt(y, x, prompt_str):
        stdscr.addstr(y, x, prompt_str)
        stdscr.refresh()
        curses.echo()
        inp = stdscr.getstr(y, x + len(prompt_str),  max_x - x - len(prompt_str) - 2)
        curses.noecho()
        return inp.decode('utf-8').strip()

    try:
        if choice == "Scan Guardrails":
            repo = prompt(3, 4, "Repo URL [default https://github.com/openai/gemma.git]: ") or None
            dest = prompt(5, 4, "Dest dir [default gemma_src]: ") or None
            branch = prompt(7, 4, "Branch/tag [optional]: ") or None
            ctx = prompt(9, 4, "Context lines [default 3]: ") or None
            logs.append(f"Cloning repository...")
            clone_gemma(repo or "https://github.com/openai/gemma.git",
                        dest or "gemma_src", branch or None)
            logs.append(f"Parsing guardrails...")
            guardrails.clear()
            guardrails.extend(parse_guardrails(dest or "gemma_src", int(ctx or 3)))
            logs.append(f"Found {len(guardrails)} guardrails.")
            logs.append("Use 'Scan Guardrails' > 'Select...' for details.")
        elif choice == "Inference":
            model = prompt(3, 4, "Model id/path: ")
            prompt_txt = prompt(5, 4, "Prompt: ")
            quant = prompt(7, 4, "Quant [int8/fp16/fp32 or none]: ") or None
            weights = prompt(9, 4, "Weights (comma sep) or none: ") or None
            ext = prompt(11,4, "External query or none: ") or None
            tool = prompt(13,4, "Tool or none: ") or None
            logs.append(f"Running inference...")
            res = run_inference(model, prompt_txt,
                                quant=quant or None,
                                weights=weights.split(',') if weights else None,
                                external_query=ext or None,
                                tool=tool or None)
            for line in textwrap.wrap(res, max_x - 6):
                logs.append(line)
        elif choice == "External Query":
            endpoint = prompt(3,4, "Endpoint: ")
            query = prompt(5,4, "Query: ")
            logs.append(f"External query...")
            res = external_query(query, endpoint)
            for line in textwrap.wrap(res, max_x - 6):
                logs.append(line)
        elif choice == "Tool":
            name = prompt(3,4, "Tool name: ")
            inp = prompt(5,4, "Input data: ")
            logs.append(f"Invoking tool...")
            out = use_tool(name, inp)
            for line in textwrap.wrap(out, max_x - 6):
                logs.append(line)
        elif choice == "Quantize Model":
            model = prompt(3,4, "Source model: ")
            to = prompt(5,4, "To quantization: ")
            outm = prompt(7,4, "Output model path: ")
            logs.append(f"Quantizing model...")
            manage_quantization(model, to, outm)
            logs.append("Quantization completed.")
        elif choice == "Manage Weights":
            action = prompt(3,4, "Action (add/remove): ")
            wf = prompt(5,4, "Weight file path: ")
            logs.append(f"Managing weights...")
            manage_weights(action, wf)
            logs.append("Weights updated.")
    except Exception as e:
        logs.append(str(e))
    stdscr.addstr(max_y-2, 2, "Press any key to return...", curses.A_DIM)
    stdscr.getch()
