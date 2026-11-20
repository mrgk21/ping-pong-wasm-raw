<script lang="ts">
    import type { Action } from 'svelte/action';
    import { onMount } from 'svelte';

    let loading = $state(true)
    let error = $state('')
    let workerRef = $state<Worker | null>()
    let frameRef = $state<HTMLCanvasElement | null>()
    let pressedKeys = $state(new Map<number, boolean>())

    const width = 800;
    const height = 600;

    onMount(() => {
      try {
        setInterval(() => {}, 200)

        const worker = new Worker(
            new URL("../lib/workers/looper", import.meta.url),
            { type: "module" }
        );

        if (!worker) {
            console.log('failed to load game worker', );
            error = 'failed to load game worker'
            return;
        }

        worker.addEventListener('error', (event) => {
          console.error('Worker failed:', event.message);
        })

        worker.addEventListener('message', ({data: {action, data}}) => {
          console.log('Worker Sent Message: ', data);
        })

        workerRef = worker
        loading = false
      } catch (e) {
        console.log('error', e);
      }
    })

	const onKeyDown = (evt) => {
		workerRef?.postMessage({
			action: 'keydown',
			data: {
				key_code: evt.keyCode
			}
		});
	}

	const onKeyUp = (evt) => {
		workerRef?.postMessage({
			action: 'keyup',
			data: {
				key_code: evt.keyCode
			}
		});
	}

    const handleGameStart = async () => {
        console.log('do something', workerRef);
        try {
			const offscreencanvas = frameRef.transferControlToOffscreen();
			workerRef?.postMessage({
			  action:'init',
			  data : {
				  canvas: offscreencanvas,
				  width,
				  height
			  }
			}, [offscreencanvas]);
        } catch (e) {
          console.log('error', e);
        }
    }

</script>

<svelte:window onkeydown={onKeyDown} onkeyup={onKeyUp} />
<section class="p-2 bg-gray-400">
    {#if loading}
        <span>loading module</span>
  {:else if error}
      <span>error {error}</span>
    {:else}
        <section class="flex flex-col gap-1" >
            <button type='button' class="w-50 bg-purple-600 text-white p-1 rounded justify-start cursor-pointer" onclick={handleGameStart}>start game</button>
        </section>
        <canvas id="drawBox" class="m-5" height={height} width={width} bind:this={frameRef}></canvas>
    {/if}
</section>

<style>
    #drawBox {
        image-rendering: pixelated;
		margin: auto;
    }
</style>
