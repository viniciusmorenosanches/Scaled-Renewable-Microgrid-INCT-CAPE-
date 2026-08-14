@echo off
title Interface Web - Microrrede INCT CAPE
echo =================================================================
echo        INICIANDO INTERFACE WEB DA MICRORREDE INCT CAPE
echo =================================================================
echo.

:: Verifica se o Python esta instalado
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERRO] Python nao encontrado! Por favor, instale o Python 3.x
    echo e certifique-se de marcar a opcao "Add Python to PATH".
    echo.
    pause
    exit /b
)

:: Verifica/Instala dependencias necessarias
echo [INFO] Verificando dependencias necessarias (fastapi, uvicorn, pyserial, websockets)...
python -c "import fastapi, uvicorn, serial, websockets" >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Instalando pacotes necessarios via pip...
    python -m pip install --upgrade pip
    python -m pip install fastapi uvicorn pyserial websockets
    if %errorlevel% neq 0 (
        echo [ERRO] Falha ao instalar dependencias! Verifique sua conexao de internet.
        pause
        exit /b
    )
)

:: Abre o navegador padrao no endereco do site
echo [INFO] Abrindo o navegador em http://127.0.0.1:8000 ...
start http://127.0.0.1:8000

:: Inicia o servidor uvicorn
echo [INFO] Iniciando o servidor FastAPI...
echo Pressione Ctrl+C para encerrar o servidor.
echo.
cd web_app
python -m uvicorn main:app --host 127.0.0.1 --port 8000

pause
