# Framework para alteração compilacao e quantizacao de  comportamental de modelos
# Fornece recursos para alteracao de politicas guardrails, etc.
# 
# Roger Luft 27/07/2025


# Gemma Guardrails Manager

CLI entry point for Gemma guardrails manager.
Ponto de entrada CLI para gerenciador de guardrails do Gemma.

## Installation / Instalação

Clone the repository and run with Python 3.
Clone o repositório e execute com Python 3.

```sh
git clone https://github.com/openai/gemma.git
cd gemma
python3 cli.py --help
```

## Usage / Uso

Scan the repository for guardrails and select which ones to keep via an interactive TUI.
Analise o repositório em busca de guardrails e selecione quais manter através de uma TUI interativa.

```sh
# Launch full interactive TUI (scan, inference, queries...)
python3 cli.py scan
# or
python3 cli.py tui
```

Controls / Controles:
- UP/DOWN or k/j: navigate / navegar
- SPACE: select/deselect / selecionar/desmarcar
- ENTER: confirm selection and exit / confirmar seleção e sair
- Q: quit without selecting / sair sem selecionar

> **Note / Nota:** If you encounter a terminal initialization error (e.g., "could not find terminal"),
> ensure you're running in a real terminal with a valid TERM environment variable set,
> e.g., `export TERM=xterm-256color`.

## Interactive TUI Interface / Interface TUI Interativa

The TUI presents a list of guardrails on the left and the code context on the right.
A TUI apresenta a lista de guardrails à esquerda e o contexto de código à direita.

## Framework Commands / Comandos do Framework

Além do scan de guardrails, há subcomandos para orquestração de inferência, consultas externas, tools, quantização e gerenciamento de pesos.

### Inference / Inferência

```sh
python3 cli.py infer \
  --model llama_model \
  --prompt "What is the capital of France?" \
  --quant fp16 \
  --weights custom_weights.bin \
  --external-query "weather(api)" \
  --tool preprocess_tool
```

### External Query / Consulta Externa

```sh
python3 cli.py query \
  --endpoint https://api.example.com/data \
  --query "select * from table"
```

### Tool Invocation / Invocação de Tool

```sh
python3 cli.py tool \
  --name summarize_tool \
  --input "Long document text..."
```

### Quantization / Quantização

```sh
python3 cli.py quantize \
  --model base_model.bin \
  --to int8 \
  --out-model model_int8.bin
```


### Weight Management / Gerenciamento de Pesos

```sh
# adicionar peso
python3 cli.py weights --action add --file new_weights.bin

# remover peso
python3 cli.py weights --action remove --file old_weights.bin

## Weight Management / Gerenciamento de Pesos

```sh
# adicionar peso
python3 cli.py weights --action add --file new_weights.bin

# remover peso
python3 cli.py weights --action remove --file old_weights.bin

# listar pesos conhecidos localmente ou recomendados
python3 cli.py tui  # e depois escolha 'Manage Weights' na TUI

### Configure / Configuração do Ambiente

```sh
# instala dependências do sistema (apt, Bazel, kernel headers)
python3 cli.py configure

# mostra estado atual da configuração (sem instalar)
python3 cli.py configure --status
```
```
```

### Build Gemma C++ Inference Engine / Compilação do motor de inferência Gemma C++

Para compilar o binário `gemma` (C++) com CMake, instale um compilador C++17 e o CMake.
Em sistemas Unix-like:
```sh
# na raiz do projeto
mkdir -p gemma_src/build && cd gemma_src/build
cmake ..
cmake --build . -- -j$(nproc)
# executável em gemma_src/build/gemma
```

Em Windows (PowerShell):
```powershell
cd gemma_src; mkdir build; cd build
cmake -A x64 ..
cmake --build . --config Release
# executável em gemma_src\build\Release\gemma.exe
```

Exemplo de uso:
```sh
./gemma --model ../models/2b-it-sfp.sbs \
      --tokenizer ../models/tokenizer.spm \
      --prompt "Hello, world!"
```

### Building Shared Library and Kernel Module / Biblioteca Compartilhada e Módulo de Kernel

Além do binário, você pode compilar a biblioteca compartilhada `libgemma` ou o módulo de kernel `gemma_kmod.ko`.

#### Biblioteca Compartilhada (libgemma)
```sh
cd gemma_src/build
cmake --preset make
cmake --build . --preset make --target libgemma
# saída: gemma_src/build/libgemma.so
```

#### Módulo de Kernel (gemma_kmod.ko)
Requer cabeçalhos do kernel Linux instalados.
```sh
mkdir -p gemma_src/build-kmod && cd gemma_src/build-kmod
cmake -DBUILD_KMOD=ON -DCMAKE_BUILD_TYPE=Release ../
cmake --build .
# módulo em gemma_src/build-kmod/gemma_kmod.ko
sudo insmod gemma_kmod.ko
sudo rmmod gemma_kmod
```

## Documentation Translation / Tradução da Documentação

This README includes both English and Brazilian Portuguese sections.
Este README inclui seções em Inglês e Português Brasileiro.

## License / Licença

This project (script, interactive TUI, and start-manager) is licensed under
Creative Commons Attribution 4.0 International (CC BY 4.0).
© 2025 Roger Luft

Este projeto (scripts, interface TUI e start-manager) está licenciado sob
Creative Commons Attribution 4.0 International (CC BY 4.0).
© 2025 Roger Luft
