## Checking it out
```sh
# ui
pnpm install
pnpm dev

# wasm build
em++ main.cpp -o main.js   -s EXPORTED_FUNCTIONS='["_init","_player1_key_event", "_player2_key_event"]'   -s EXPORTED_RUNTIME_METHODS='["cwrap"]' -s ASYNCIFY -s MODULARIZE=1  -s EXPORT_ES6=1 -O2
```

## todo checklist

### performance / tech
- shared memory buffer between cpp and js - reduce heap copying
- debug routes for info on js dashboard
- partial updation of the canvas heap memory (cpp)
- benchmarking and perf testing

### features
- curved entities and bouncing
- score counter
- bounce takes random angles and the ball speeds up (do playtesting)
- site hosting
