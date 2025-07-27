"""
Downloader module for Gemma source code.

Módulo de download para o código-fonte do Gemma.
"""

import os
import subprocess
from typing import Optional

def clone_gemma(repo_url: str, dest_dir: str, branch: Optional[str] = None) -> None:
    """
    Clone the Gemma repository from the given URL into dest_dir.
    If dest_dir exists, it will be updated via git pull.

    Clona o repositório Gemma da URL fornecida em dest_dir.
    Se dest_dir existir, será atualizado via git pull.
    """
    if os.path.isdir(dest_dir):
        print(f"Updating existing Gemma repository at {dest_dir}")
        subprocess.check_call(["git", "-C", dest_dir, "pull"])
    else:
        print(f"Cloning Gemma repository from {repo_url} into {dest_dir}")
        cmd = ["git", "clone", repo_url, dest_dir]
        if branch:
            cmd.extend(["--branch", branch])
        subprocess.check_call(cmd)
