#!/bin/bash

# Cores para a saída do terminal
GREEN=$(tput setaf 2)
BLUE=$(tput setaf 4)
YELLOW=$(tput setaf 3)
RED=$(tput setaf 1)
RESET=$(tput sgr0)
BOLD=$(tput bold)

# Variáveis globais
KERNEL_VERSION=""
KERNEL_SOURCE_PATH="${HOME}/src/kernel/WSL2-Linux-Kernel" # Caminho padrão
MODULE_NAME="hello-wsl2-module"
MODULE_CODE=""
MODULE_MAKEFILE=""
DKMS_CONF=""

# Função para exibir mensagens de cabeçalho
display_header() {
    echo ""
    echo "${BOLD}${BLUE}=====================================================${RESET}"
    echo "${BOLD}${BLUE}  Assistente de Módulo de Kernel WSL2 (Bash Script)  ${RESET}"
    echo "${BOLD}${BLUE}=====================================================${RESET}"
    echo ""
}

# Função para exibir um bloco de código/comando e permitir cópia
display_code_block() {
    local code="$1"
    echo "${YELLOW}Comando/Código:${RESET}"
    echo "${BOLD}${GREEN}-----------------------------------------------------${RESET}"
    echo "${code}"
    echo "${BOLD}${GREEN}-----------------------------------------------------${RESET}"
    echo ""
}

# Função para solicitar confirmação do usuário
confirm_action() {
    local prompt="$1"
    while true; do
        read -p "${BOLD}${YELLOW}${prompt} (s/n): ${RESET}" yn
        case $yn in
            [Ss]* ) return 0;;
            [Nn]* ) return 1;;
            * ) echo "${RED}Por favor, responda 's' ou 'n'.${RESET}";;
        esac
    done
}

# Função para verificar a compatibilidade do SO
check_os_compatibility() {
    echo "${BOLD}${BLUE}Passo 0: Verificando Compatibilidade do SO${RESET}"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        if [[ "$ID" == "ubuntu" || "$ID" == "debian" ]]; then
            echo "${GREEN}Sistema Operacional detectado: ${NAME} ${VERSION_ID}. Compatível.${RESET}"
        else
            echo "${RED}Sistema Operacional não suportado: ${NAME}. Este script é para Ubuntu/Debian.${RESET}"
            exit 1
        fi
    else
        echo "${RED}Não foi possível detectar o Sistema Operacional. /etc/os-release não encontrado.${RESET}"
        exit 1
    fi
    echo ""
}

# Função para instalar pré-requisitos
install_prerequisites() {
    echo "${BOLD}${BLUE}Passo 1: Instalando Pré-requisitos${RESET}"
    echo "Certifique-se de que todas as ferramentas de construção necessárias estejam instaladas."
    echo "Este passo requer privilégios de sudo."
    echo ""

    local commands="sudo apt update && sudo apt install -y build-essential flex bison dwarves libssl-dev libelf-dev libncurses-dev git bc cpio qemu-utils dkms"
    display_code_block "$commands"

    if confirm_action "Deseja executar os comandos de instalação de pré-requisitos agora?"; then
        if eval "$commands"; then
            echo "${GREEN}Pré-requisitos instalados com sucesso.${RESET}"
        else
            echo "${RED}Falha ao instalar pré-requisitos. Por favor, verifique os erros acima.${RESET}"
            error_exit
        fi
    else
        echo "${YELLOW}Instalação de pré-requisitos ignorada. Por favor, instale-os manualmente para prosseguir.${RESET}"
        error_exit
    fi
    echo ""
}

# Função para preparar o código-fonte do kernel
prepare_kernel_source() {
    echo "${BOLD}${BLUE}Passo 2: Configuração do Código-Fonte do Kernel${RESET}"
    echo "Vamos obter e preparar o código-fonte do kernel WSL2 que corresponde à sua versão atual."
    echo ""

    echo "Executando 'uname -r' para obter a versão do seu kernel..."
    CURRENT_KERNEL_VERSION=$(uname -r)
    echo "${GREEN}Versão do Kernel detectada: ${CURRENT_KERNEL_VERSION}${RESET}"
    KERNEL_VERSION="$CURRENT_KERNEL_VERSION"

    # Extrai a parte numérica da versão do kernel (ex: 6.6.87.2 de 6.6.87.2-microsoft-standard-WSL2)
    KERNEL_TAG_BASE=$(echo "$KERNEL_VERSION" | cut -d'-' -f1)
    KERNEL_TAG="linux-msft-wsl-${KERNEL_TAG_BASE}"

    echo ""
    echo "Agora, vamos clonar o repositório do kernel e prepará-lo."
    echo "A tag do git a ser usada é: ${BOLD}${YELLOW}${KERNEL_TAG}${RESET}"
    echo "O código-fonte será clonado em: ${BOLD}${YELLOW}${KERNEL_SOURCE_PATH}${RESET}"
    echo ""

    local commands="mkdir -p ${KERNEL_SOURCE_PATH%/*} && \
cd ${KERNEL_SOURCE_PATH%/*} && \
git clone https://github.com/microsoft/WSL2-Linux-Kernel.git ${KERNEL_SOURCE_PATH##*/} && \
cd ${KERNEL_SOURCE_PATH} && \
git checkout ${KERNEL_TAG} && \
zcat /proc/config.gz > .config && \
make prepare modules_prepare -j\$(nproc)"
    display_code_block "$commands"

    if confirm_action "Deseja executar os comandos para clonar e preparar o código-fonte do kernel agora?"; then
        if eval "$commands"; then
            echo "${GREEN}Código-fonte do kernel clonado e preparado com sucesso.${RESET}"
        else
            echo "${RED}Falha ao clonar ou preparar o código-fonte do kernel. Por favor, verifique os erros acima.${RESET}"
            error_exit
        fi
    else
        echo "${YELLOW}Clonagem e preparação do código-fonte do kernel ignoradas. Por favor, prepare-o manualmente para prosseguir.${RESET}"
        error_exit
    fi

    echo ""
    read -p "${BOLD}${YELLOW}Confirme o caminho para o diretório do código-fonte do kernel clonado (padrão: ${KERNEL_SOURCE_PATH}): ${RESET}" user_path
    if [ -n "$user_path" ]; then
        KERNEL_SOURCE_PATH="$user_path"
    fi
    echo "${GREEN}Caminho do código-fonte do kernel definido para: ${KERNEL_SOURCE_PATH}${RESET}"
    echo ""
}

# Função para criar arquivos do módulo
create_module_files() {
    echo "${BOLD}${BLUE}Passo 3: Detalhes do Módulo${RESET}"
    echo "Defina o nome do seu módulo e revise o código-fonte e o Makefile."
    echo ""

    read -p "${BOLD}${YELLOW}Digite o nome do Módulo (padrão: ${MODULE_NAME}): ${RESET}" user_module_name
    if [ -n "$user_module_name" ]; then
        MODULE_NAME="$user_module_name"
    fi
    echo "${GREEN}Nome do Módulo definido para: ${MODULE_NAME}${RESET}"
    echo ""

    # Conteúdo padrão para a descrição e comentário (sem chamada LLM direta no Bash)
    local default_description="Um módulo hello world simples para WSL2."
    local default_comment="/*\n * ${MODULE_NAME}.c - Um módulo de kernel de exemplo para WSL2.\n */"

    echo "${YELLOW}Nota: A geração automática de descrição via LLM não é diretamente suportada em scripts Bash puros. Uma descrição padrão será usada.${RESET}"
    echo "Você pode editar o código do módulo manualmente para personalizar a descrição e o comentário."
    echo ""

    MODULE_CODE="
${default_comment}
#include <linux/module.h> /* Necessário para todos os módulos */
#include <linux/printk.h> /* Necessário para pr_info() */

int init_module(void)
{
    pr_info(\"Olá mundo do módulo de kernel WSL2: ${MODULE_NAME} carregado!\\n\");
    return 0;
}

void cleanup_module(void)
{
    pr_info(\"Adeus mundo do módulo de kernel WSL2: ${MODULE_NAME} descarregado.\\n\");
}

MODULE_LICENSE(\"GPL\");
MODULE_AUTHOR(\"Seu Nome\");
MODULE_DESCRIPTION(\"${default_description}\");
MODULE_VERSION(\"1.0\");
"

    MODULE_MAKEFILE="
# Defina o caminho absoluto para o seu código-fonte do Microsoft WSL2-Linux-Kernel clonado e preparado
# Certifique-se de que este caminho esteja correto para sua configuração.
KERNEL_SRC := ${KERNEL_SOURCE_PATH}

# Especifique o nome do seu módulo de kernel (sem a extensão .ko)
obj-m += ${MODULE_NAME}.o

# Alvo padrão para construir o módulo
all:
	\$(MAKE) -C \$(KERNEL_SRC) M=\$(PWD) modules

# Alvo para limpar os arquivos do módulo compilado
clean:
	\$(MAKE) -C \$(KERNEL_SRC) M=\$(PWD) clean
"

    echo "${BOLD}${BLUE}Código-Fonte do Módulo (${MODULE_NAME}.c):${RESET}"
    display_code_block "$MODULE_CODE"

    echo "${BOLD}${BLUE}Makefile do Módulo:${RESET}"
    display_code_block "$MODULE_MAKEFILE"

    local module_dir="${HOME}/${MODULE_NAME}_module"
    echo "Criando diretório para o módulo: ${module_dir}"
    mkdir -p "$module_dir" || error_exit "Falha ao criar diretório do módulo."

    echo "Salvando ${MODULE_NAME}.c em ${module_dir}..."
    echo -e "$MODULE_CODE" > "${module_dir}/${MODULE_NAME}.c" || error_exit "Falha ao salvar o arquivo .c."

    echo "Salvando Makefile em ${module_dir}..."
    echo -e "$MODULE_MAKEFILE" > "${module_dir}/Makefile" || error_exit "Falha ao salvar o Makefile."

    echo "${GREEN}Arquivos do módulo criados com sucesso em ${module_dir}${RESET}"
    echo ""
}

# Função para compilar e testar o módulo
compile_and_test_module() {
    echo "${BOLD}${BLUE}Passo 4: Compilação e Teste do Módulo${RESET}"
    echo "Agora vamos compilar seu módulo e testá-lo."
    echo ""

    local module_dir="${HOME}/${MODULE_NAME}_module"
    if [ ! -d "$module_dir" ]; then
        echo "${RED}Diretório do módulo não encontrado: ${module_dir}. Por favor, execute o Passo 3 primeiro.${RESET}"
        error_exit
    fi

    echo "Navegando para o diretório do módulo: ${module_dir}"
    cd "$module_dir" || error_exit "Falha ao navegar para o diretório do módulo."

    echo "${BOLD}${BLUE}Compilando o Módulo:${RESET}"
    display_code_block "make"

    if confirm_action "Deseja compilar o módulo agora?"; then
        if make; then
            echo "${GREEN}Módulo compilado com sucesso! Arquivo ${MODULE_NAME}.ko gerado.${RESET}"
        else
            echo "${RED}Falha ao compilar o módulo. Por favor, verifique os erros acima.${RESET}"
            error_exit
        fi
    else
        echo "${YELLOW}Compilação do módulo ignorada.${RESET}"
        error_exit
    fi
    echo ""

    echo "${BOLD}${BLUE}Testando o Módulo:${RESET}"
    local module_ko="${MODULE_NAME}.ko"
    local module_underscored=$(echo "$MODULE_NAME" | tr '-' '_')

    display_code_block "sudo insmod ./${module_ko}"
    if confirm_action "Deseja carregar o módulo agora (insmod)?"; then
        if sudo insmod "./${module_ko}"; then
            echo "${GREEN}Módulo carregado com sucesso.${RESET}"
            echo ""
            echo "Verificando se o módulo está carregado e vendo as mensagens de log:"
            display_code_block "lsmod | grep ${module_underscored}\ndmesg | tail"
            if confirm_action "Deseja verificar o módulo e os logs agora?"; then
                lsmod | grep "${module_underscored}"
                dmesg | tail
                echo "${GREEN}Verificação concluída.${RESET}"
            fi
        else
            echo "${RED}Falha ao carregar o módulo. Por favor, verifique os logs do kernel com 'dmesg | tail'.${RESET}"
            error_exit
        fi
    else
        echo "${YELLOW}Carregamento do módulo ignorado.${RESET}"
    fi
    echo ""

    display_code_block "sudo rmmod ${module_underscored}"
    if confirm_action "Deseja descarregar o módulo agora (rmmod)?"; then
        if sudo rmmod "${module_underscored}"; then
            echo "${GREEN}Módulo descarregado com sucesso.${RESET}"
        else
            echo "${RED}Falha ao descarregar o módulo. Ele pode não estar carregado ou há um erro.${RESET}"
        fi
    else
        echo "${YELLOW}Descarregamento do módulo ignorado.${RESET}"
    fi
    echo ""
}

# Função para integrar com DKMS
integrate_with_dkms() {
    echo "${BOLD}${BLUE}Passo 5: Integração com DKMS para Persistência${RESET}"
    echo "Para garantir que seu módulo seja recompilado e carregado automaticamente após as atualizações do kernel WSL2, vamos integrá-lo com o DKMS."
    echo ""

    local module_underscored=$(echo "$MODULE_NAME" | tr '-' '_')

    DKMS_CONF="
PACKAGE_NAME=\"${MODULE_NAME}\"
PACKAGE_VERSION=\"1.0\"
BUILT_MODULE_NAME=\"${MODULE_NAME}\"
BUILT_MODULE_LOCATION=\".\"
DEST_MODULE_LOCATION=\"/kernel/drivers/misc\" # Ou um caminho apropriado, e.g., /kernel/drivers/char
AUTOINSTALL=\"yes\"
# O caminho para o código-fonte do kernel clonado e preparado
MAKE=\"make -C ${KERNEL_SOURCE_PATH} M=\${PWD}\"
CLEAN=\"make -C ${KERNEL_SOURCE_PATH} M=\${PWD} clean\"
"
    echo "${BOLD}${BLUE}Conteúdo do dkms.conf:${RESET}"
    display_code_block "$DKMS_CONF"

    local module_dir="${HOME}/${MODULE_NAME}_module"
    echo "Salvando dkms.conf em ${module_dir}..."
    echo -e "$DKMS_CONF" > "${module_dir}/dkms.conf" || error_exit "Falha ao salvar dkms.conf."
    echo "${GREEN}dkms.conf criado com sucesso.${RESET}"
    echo ""

    echo "${BOLD}${BLUE}Comandos DKMS:${RESET}"
    local dkms_commands="sudo mkdir -p /usr/src/${MODULE_NAME}-1.0 && \
sudo cp -Rv ${module_dir}/. /usr/src/${MODULE_NAME}-1.0 && \
sudo dkms add -m ${MODULE_NAME} -v 1.0 && \
sudo dkms build -m ${MODULE_NAME} -v 1.0 && \
sudo dkms install -m ${MODULE_NAME} -v 1.0 && \
sudo modprobe ${module_underscored}"
    display_code_block "$dkms_commands"

    if confirm_action "Deseja executar os comandos DKMS para adicionar, construir e instalar o módulo?"; then
        if eval "$dkms_commands"; then
            echo "${GREEN}Módulo DKMS configurado e instalado com sucesso.${RESET}"
        else
            echo "${RED}Falha na configuração do DKMS. Por favor, verifique os erros acima.${RESET}"
            error_exit
        fi
    else
        echo "${YELLOW}Configuração do DKMS ignorada.${RESET}"
    fi
    echo ""

    echo "${BOLD}${BLUE}Configurando Carregamento Automático na Inicialização:${RESET}"
    local autoload_command="echo \"${module_underscored}\" | sudo tee -a /etc/modules-load.d/${module_underscored}.conf"
    display_code_block "$autoload_command"

    if confirm_action "Deseja configurar o módulo para carregar automaticamente na inicialização?"; then
        if eval "$autoload_command"; then
            echo "${GREEN}Módulo configurado para carregamento automático na inicialização.${RESET}"
        else
            echo "${RED}Falha ao configurar o carregamento automático.${RESET}"
        fi
    else
        echo "${YELLOW}Carregamento automático ignorado.${RESET}"
    fi
    echo ""
}

# Função para considerações finais
final_considerations() {
    echo "${BOLD}${BLUE}Passo 6: Considerações Finais e Compatibilidade${RESET}"
    echo "Algumas notas importantes sobre compatibilidade e integração com o WSL2:"
    echo ""

    echo "${BOLD}${BLUE}Compatibilidade com Docker Desktop e GPU:${RESET}"
    echo "${YELLOW}Aviso: O uso de kernels ou módulos personalizados com o Docker Desktop no WSL2 não é oficialmente suportado e pode causar problemas. Se você depende do Docker Desktop, proceda com cautela e esteja preparado para reverter as alterações se encontrar problemas. Módulos de kernel modificam o ambiente do kernel, o que pode afetar a estabilidade de outros recursos.${RESET}"
    echo "${YELLOW}Para compatibilidade com GPU, certifique-se de que seu módulo não interfira com os drivers ou a configuração de GPU existente do WSL2. Testes rigorosos são recomendados.${RESET}"
    echo ""

    echo "${BOLD}${BLUE}Ajustando Arquivos de Integração WSL (.wslconfig):${RESET}"
    echo "Se você precisar de configurações de kernel mais profundas (por exemplo, desabilitar a verificação de assinatura de módulo com 'module.sig_unenforce'), você precisará modificar o arquivo '.wslconfig' no Windows. Este arquivo geralmente está localizado em %UserProfile%\\.wslconfig."
    echo ""
    echo "${YELLOW}Exemplo de .wslconfig para adicionar argumentos à linha de comando do kernel:${RESET}"
    display_code_block "[wsl2]\nkernelCommandLine = module.sig_unenforce"
    echo "${YELLOW}Importante: Você deve editar este arquivo no Windows, não no WSL2. Após a modificação, reinicie o WSL com 'wsl --shutdown' no PowerShell/CMD e inicie-o novamente.${RESET}"
    echo ""
}

# Função de saída de erro
error_exit() {
    echo ""
    echo "${RED}Erro: $1${RESET}"
    echo "${RED}O script será encerrado.${RESET}"
    exit 1
}

# Loop principal do menu
main_menu() {
    while true; do
        clear
        display_header
        echo "${BOLD}${BLUE}Menu Principal:${RESET}"
        echo "1. Verificar Compatibilidade do SO"
        echo "2. Instalar Pré-requisitos"
        echo "3. Preparar Código-Fonte do Kernel"
        echo "4. Criar Arquivos do Módulo"
        echo "5. Compilar e Testar Módulo"
        echo "6. Integrar com DKMS para Persistência"
        echo "7. Considerações Finais e Compatibilidade"
        echo "8. Sair"
        echo ""
        read -p "${BOLD}${YELLOW}Escolha uma opção: ${RESET}" choice

        case $choice in
            1) check_os_compatibility ;;
            2) install_prerequisites ;;
            3) prepare_kernel_source ;;
            4) create_module_files ;;
            5) compile_and_test_module ;;
            6) integrate_with_dkms ;;
            7) final_considerations ;;
            8) echo "${GREEN}Saindo do assistente. Adeus!${RESET}"; exit 0 ;;
            * ) echo "${RED}Opção inválida. Por favor, tente novamente.${RESET}" ; sleep 2 ;;
        esac
        echo ""
        read -p "${BOLD}${YELLOW}Pressione Enter para continuar...${RESET}"
    done
}

# Iniciar o script
main_menu

