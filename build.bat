@echo off

clang -Os -fno-builtin -Wall -Wextra -Wswitch-enum --target=wasm32 --no-standard-libraries -Wl,--export=game_init -Wl,--export=game_render -Wl,--export=game_update -Wl,--export=game_keydown -Wl,--export=game_keyup -Wl,--no-entry -Wl,--allow-undefined -o src\wasm\game.wasm src\wasm\game.c

if %errorlevel% neq 0 (
    echo Build failed! Exit Code %errorlevel%.
    pause
    exit /b %errorlevel%
)
