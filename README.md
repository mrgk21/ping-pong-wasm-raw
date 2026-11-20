## Prerequisites
- `clang` is required to build wasm
- `pnpm` or `npm` for the node modules and svelte.

### Using PNPM:
```sh
pnpm install
```
### Using NPM:
```sh
npm install
```

## Testing the game
```sh
# wasm build
build.sh

# start dev server
npm run dev

# or with pnpm
pnpm dev
```

## For windows
```bat
# build wasm module
build.bat

# start dev server
npm run dev

# or with pnpm
pnpm dev
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
