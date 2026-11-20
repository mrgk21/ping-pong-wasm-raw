
/*
const moduleFns = {
  game_init: (x: number, y: number) => { },
  game_keydown: (key_code: number) => { },
  game_keyup: (key_code: number) => { },
  game_update: (fixed_dt: number) => { },
  game_render: (dt: number, alpha: number) => { }
}
*/

let module = null
let offscreencanvas;
let ctx;

const color_hex = (color: number) => {
	const r = ((color>>(0*8))&0xFF).toString(16).padStart(2, '0');
	const g = ((color>>(1*8))&0xFF).toString(16).padStart(2, '0');
	const b = ((color>>(2*8))&0xFF).toString(16).padStart(2, '0');
	const a = ((color>>(3*8))&0xFF).toString(16).padStart(2, '0');
	return "#"+r+g+b+a;
}

const cstrlen = (mem, ptr) => {
	let len = 0;
	while (mem[ptr] != 0) {
		len++;
		ptr++;
	}
	return len;
}

const cstr_by_ptr = (mem_buffer, ptr) => {
	const mem = new Uint8Array(mem_buffer);
	const len = cstrlen(mem, ptr);
	const bytes = new Uint8Array(mem_buffer, ptr, len);
	return new TextDecoder().decode(bytes);
}

const platform_log = (message_ptr) => {
	const buffer = wasm.instance.exports.memory.buffer;
	const message = cstr_by_ptr(buffer, message_ptr);
	console.log(message);
}

const platform_stroke_line_t = (x1, y1, x2, y2, thickness, color) => {
	prev_lineWidth = ctx.lineWidth;
	ctx.lineWidth = thickness;
	ctx.strokeStyle = color_hex(color);
	ctx.moveTo(x1, y1);
	ctx.lineTo(x2, y2);
	ctx.stroke();
	ctx.lineWidth = prev_lineWidth;
}

const platform_stroke_line = (x1, y1, x2, y2, color) => {
	ctx.strokeStyle = color_hex(color);
	ctx.moveTo(x1, y1);
	ctx.lineTo(x2, y2);
	ctx.stroke();
}

const platform_stroke_rect = (x, y, w, h, color) => {
	ctx.strokeStyle = color_hex(color); 
	ctx.strokeRect(x, y, w, h);
}

const platform_fill_rect = (x, y, w, h, color) => {
	ctx.fillStyle = color_hex(color); 
	ctx.fillRect(x, y, w, h);
}

const platform_stroke_circle = (x, y, r, color) => {
	ctx.beginPath();
	ctx.arc(x, y, r, 0, 2 * Math.PI);
	ctx.strokeStyle = color_hex(color); 
	ctx.stroke();
}

const platform_fill_circle = (x, y, r, color) => {
	ctx.beginPath();
	ctx.arc(x, y, r, 0, 2 * Math.PI);
	ctx.fillStyle = color_hex(color); 
	ctx.fill();
}

const PHYSICS_RATE_HZ = 60;
const FIXED_DT = 1.0 / PHYSICS_RATE_HZ;

let prev_time;
let accumulator = 0;
let alpha = 0;
const loop = (timestamp) => {
	if (prev_time === undefined) {
		prev_time = timestamp;
	}

	const dt = 0.001 * (timestamp - prev_time);
	prev_time = timestamp;

	accumulator += dt;
	while(accumulator >= FIXED_DT) {
		module?.instance.exports.game_update(FIXED_DT);
		accumulator -= FIXED_DT;
	}
	
	alpha = accumulator / FIXED_DT;

	module?.instance.exports.game_render(dt, alpha);
	requestAnimationFrame(loop);
}


onmessage = async function (event: any) {
  const { action, data } = event.data;
  switch (action) {
    case 'init': {
      try {
        if (module) {
          console.log('game already initialised',);
          return;
        }

		offscreencanvas = data.canvas
		ctx = offscreencanvas.getContext("2d");
        ctx.imageSmoothingEnabled = true;

		module = await WebAssembly.instantiateStreaming(
			fetch(new URL("../../wasm/game.wasm", import.meta.url)),
			{
				env: {
					platform_stroke_line_t,
					platform_stroke_line,
					platform_stroke_rect,
					platform_fill_rect,
					platform_stroke_circle,
					platform_fill_circle,
					platform_log
				}
			}
		);

		module?.instance.exports.game_init(data.width, data.height);
        console.log('game initialized');
		requestAnimationFrame(loop);
      } catch (e) {
        console.log('error', e);
      }
      break;
    }
	case 'keydown': {
      try {
		module?.instance.exports.game_keydown(data.key_code);
      } catch (e) {
        console.log("Error while keydown");
      }
		break;
	}
	case 'keyup': {
      try {
		module?.instance.exports.game_keyup(data.key_code);
      } catch (e) {
        console.log("Error while keyup");
      }
		break;
	}
  }
}
