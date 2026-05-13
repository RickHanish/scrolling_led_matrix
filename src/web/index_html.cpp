#include "web/index_html.h"

#include "app/app_state.h"
#include "colors/colot_utils.h"

static String escapeForJs(const String& input) {
	String out;
	out.reserve(input.length() + 8);

	for (size_t i = 0; i < input.length(); i++) {
		char ch = input[i];
		if (ch == '\\') {
			out += "\\\\";
		} else if (ch == '"') {
			out += "\\\"";
		} else if (ch == '\n') {
			out += "\\n";
		} else if (ch == '\r') {
			out += "\\r";
		} else {
			out += ch;
		}
	}

	return out;
}

String buildIndexHtml(const AppState& state, const char* apiKey) {
	String msgValue = state.customMessage ? state.message : String("");

	String html = R"HTML(
<!doctype html>
<html lang='en'>
<head>
	<meta charset='UTF-8'>
	<meta name='viewport' content='width=device-width, initial-scale=1'>
	<title>LED Matrix Control</title>
	<style>
		:root {
			--bg-0: #0e1a2b;
			--bg-1: #142845;
			--card: #f7fbff;
			--ink: #11213a;
			--muted: #5a6c88;
			--accent: #0f8cff;
			--accent-2: #23d6b5;
			--danger: #d64545;
		}
		* { box-sizing: border-box; }
		body {
			margin: 0;
			font-family: "Trebuchet MS", "Segoe UI", sans-serif;
			color: var(--ink);
			min-height: 100vh;
			background:
				radial-gradient(1200px 800px at -10% -20%, #3c6ea5 0%, rgba(60,110,165,0) 45%),
				radial-gradient(1200px 900px at 120% 10%, #3fd8c2 0%, rgba(63,216,194,0) 40%),
				linear-gradient(135deg, var(--bg-0), var(--bg-1));
			display: grid;
			place-items: center;
			padding: 18px;
		}
		.panel {
			width: min(980px, 100%);
			background: linear-gradient(180deg, #ffffff, #f3f9ff);
			border-radius: 18px;
			box-shadow: 0 20px 48px rgba(0, 0, 0, 0.25);
			padding: 18px;
			display: grid;
			gap: 14px;
			animation: fadeIn 300ms ease-out;
		}
		@keyframes fadeIn {
			from { opacity: 0; transform: translateY(8px); }
			to { opacity: 1; transform: translateY(0); }
		}
		h1 {
			margin: 2px 0 0;
			font-size: 1.35rem;
			letter-spacing: 0.3px;
		}
		.sub {
			margin: 0;
			color: var(--muted);
			font-size: 0.95rem;
		}
		.grid {
			display: grid;
			grid-template-columns: 1.3fr 1fr;
			gap: 12px;
		}
		.card {
			border: 1px solid #dce8f5;
			border-radius: 14px;
			padding: 12px;
			background: #ffffff;
		}
		label {
			font-weight: 600;
			font-size: 0.92rem;
			display: block;
			margin-bottom: 6px;
		}
		input[type='text'],
		input[type='number'],
		select {
			width: 100%;
			border: 1px solid #bfd2e6;
			border-radius: 10px;
			padding: 10px 12px;
			font-size: 0.95rem;
			color: var(--ink);
			background: #fff;
		}
		input[type='color'] {
			width: 100%;
			border: 1px solid #bfd2e6;
			border-radius: 10px;
			min-height: 44px;
			background: #fff;
			padding: 4px;
		}
		.row {
			display: grid;
			grid-template-columns: 1fr 1fr;
			gap: 10px;
		}
		.actions {
			display: flex;
			gap: 10px;
			flex-wrap: wrap;
		}
		button {
			border: 0;
			border-radius: 11px;
			padding: 10px 14px;
			font-size: 0.95rem;
			cursor: pointer;
			transition: transform 120ms ease, box-shadow 120ms ease;
		}
		button:hover {
			transform: translateY(-1px);
			box-shadow: 0 8px 18px rgba(9, 39, 78, 0.16);
		}
		.btn-primary {
			background: linear-gradient(90deg, var(--accent), #2a7bff);
			color: #fff;
			font-weight: 700;
		}
		.btn-secondary {
			background: linear-gradient(90deg, #f0f4fa, #e9f6ff);
			color: #1b3557;
			border: 1px solid #c8d9ec;
			font-weight: 700;
		}
		.preview {
			min-height: 66px;
			border-radius: 12px;
			border: 1px solid #21334a;
			background:
				linear-gradient(180deg, rgba(10, 16, 26, 0.98), rgba(20, 30, 46, 0.98)),
				repeating-linear-gradient(
					-45deg,
					rgba(255, 255, 255, 0.03),
					rgba(255, 255, 255, 0.03) 8px,
					rgba(255, 255, 255, 0.01) 8px,
					rgba(255, 255, 255, 0.01) 16px
				);
			color: #f4f8ff;
			display: flex;
			align-items: center;
			gap: 1px;
			font-size: 1.25rem;
			font-weight: 700;
			letter-spacing: 1px;
			padding: 10px;
			overflow-x: auto;
			white-space: nowrap;
		}
		.letter-grid {
			display: grid;
			grid-template-columns: repeat(auto-fill, minmax(68px, 1fr));
			gap: 8px;
			max-height: 240px;
			overflow-y: auto;
			padding-right: 2px;
		}
		.swatch {
			border: 1px solid #c7d8ea;
			border-radius: 10px;
			padding: 6px;
			background: #f9fcff;
			display: grid;
			gap: 5px;
		}
		.swatch code {
			font-size: 0.72rem;
			color: var(--muted);
			text-align: center;
		}
		.status {
			font-size: 0.9rem;
			color: var(--muted);
			min-height: 1.2em;
		}
		.status.ok { color: #14873a; }
		.status.err { color: var(--danger); }
		@media (max-width: 900px) {
			.grid { grid-template-columns: 1fr; }
		}
	</style>
</head>
<body>
	<main class='panel'>
		<div>
			<h1>LED Matrix Controller</h1>
			<p class='sub'>Send a custom message, switch to defaults, and design text colors with solid, gradient, or per-letter styles.</p>
		</div>

		<div class='grid'>
			<section class='card'>
				<label for='msg'>Custom Message</label>
				<input id='msg' type='text' maxlength='220' placeholder='Type your message'>

				<div style='height:10px'></div>

				<label for='mode'>Color Mode</label>
				<select id='mode'>
					<option value='solid'>Solid color</option>
					<option value='gradient'>Gradient</option>
					<option value='per_letter'>Define color for each letter</option>
				</select>

				<div style='height:10px'></div>

				<div id='solidWrap'>
					<label for='solidColor'>Solid Color</label>
					<input id='solidColor' type='color'>
				</div>

				<div id='gradientWrap' class='row' style='display:none'>
					<div>
						<label for='gradientStart'>Gradient Start</label>
						<input id='gradientStart' type='color'>
					</div>
					<div>
						<label for='gradientEnd'>Gradient End</label>
						<input id='gradientEnd' type='color'>
					</div>
				</div>

				<div style='height:10px'></div>

				<label for='brightness'>Brightness (%)</label>
				<input id='brightness' type='number' min='0' max='100'>

				<div style='height:12px'></div>
				<div class='actions'>
					<button id='sendCustom' class='btn-primary' type='button'>Submit</button>
					<button id='useDefault' class='btn-secondary' type='button'>Default</button>
				</div>
				<p id='status' class='status'></p>
			</section>

			<section class='card'>
				<label>Preview</label>
				<div id='preview' class='preview'>Preview</div>

				<div style='height:10px'></div>
				<div id='letterWrap' style='display:none'>
					<label>Per-Letter Colors</label>
					<div id='letterGrid' class='letter-grid'></div>
				</div>
			</section>
		</div>
	</main>

	<script>
		const state = {
			key: "__API_KEY__",
			message: "__MESSAGE__",
			mode: "__MODE__",
			solid: "__SOLID__",
			gradientStart: "__GRADIENT_START__",
			gradientEnd: "__GRADIENT_END__",
			brightness: __BRIGHTNESS__,
			letterColorsCsv: "__LETTER_COLORS__"
		};

		const msgInput = document.getElementById('msg');
		const modeInput = document.getElementById('mode');
		const solidColorInput = document.getElementById('solidColor');
		const gradientStartInput = document.getElementById('gradientStart');
		const gradientEndInput = document.getElementById('gradientEnd');
		const brightnessInput = document.getElementById('brightness');
		const preview = document.getElementById('preview');
		const letterGrid = document.getElementById('letterGrid');
		const letterWrap = document.getElementById('letterWrap');
		const solidWrap = document.getElementById('solidWrap');
		const gradientWrap = document.getElementById('gradientWrap');
		const statusEl = document.getElementById('status');

		let letterColors = (state.letterColorsCsv || '').split(',').map(v => v.trim()).filter(Boolean);

		function glyphsFromMessage(message) {
			return Array.from(message || '').filter(ch => ch !== '\uFE0F');
		}

		function clampColor(hex) {
			return /^#[0-9a-fA-F]{6}$/.test(hex) ? hex.toUpperCase() : state.solid;
		}

		function ensureLetterColors() {
			const glyphs = glyphsFromMessage(msgInput.value);
			const fallback = clampColor(solidColorInput.value || state.solid);
			const next = [];
			for (let i = 0; i < glyphs.length; i++) {
				next.push(clampColor(letterColors[i] || fallback));
			}
			letterColors = next;
		}

		function renderLetterPickers() {
			ensureLetterColors();
			const glyphs = glyphsFromMessage(msgInput.value);
			letterGrid.innerHTML = '';

			glyphs.forEach((glyph, i) => {
				const wrapper = document.createElement('div');
				wrapper.className = 'swatch';

				const code = document.createElement('code');
				const shown = glyph === ' ' ? 'space' : glyph;
				code.textContent = shown;

				const picker = document.createElement('input');
				picker.type = 'color';
				picker.value = letterColors[i];
				picker.addEventListener('input', () => {
					letterColors[i] = picker.value.toUpperCase();
					renderPreview();
				});

				wrapper.appendChild(code);
				wrapper.appendChild(picker);
				letterGrid.appendChild(wrapper);
			});
		}

		function hexToRgb(hex) {
			const h = (hex || '#FFFFFF').replace('#', '');
			return {
				r: parseInt(h.substring(0, 2), 16),
				g: parseInt(h.substring(2, 4), 16),
				b: parseInt(h.substring(4, 6), 16)
			};
		}

		function rgbToHex(c) {
			const two = (v) => v.toString(16).padStart(2, '0');
			return `#${two(c.r)}${two(c.g)}${two(c.b)}`.toUpperCase();
		}

		function gradientColor(startHex, endHex, index, count) {
			if (count <= 1) {
				return clampColor(startHex);
			}
			const a = hexToRgb(startHex);
			const b = hexToRgb(endHex);
			const t = index / (count - 1);
			return rgbToHex({
				r: Math.round(a.r + (b.r - a.r) * t),
				g: Math.round(a.g + (b.g - a.g) * t),
				b: Math.round(a.b + (b.b - a.b) * t)
			});
		}

		function colorForIndex(index, total) {
			if (modeInput.value === 'gradient') {
				return gradientColor(gradientStartInput.value, gradientEndInput.value, index, total);
			}
			if (modeInput.value === 'per_letter') {
				return clampColor(letterColors[index] || solidColorInput.value);
			}
			return clampColor(solidColorInput.value);
		}

		function renderPreview() {
			const glyphs = glyphsFromMessage(msgInput.value);
			preview.innerHTML = '';

			if (glyphs.length === 0) {
				preview.textContent = 'Preview';
				return;
			}

			glyphs.forEach((glyph, i) => {
				const span = document.createElement('span');
				span.textContent = glyph;
				span.style.color = colorForIndex(i, glyphs.length);
				preview.appendChild(span);
			});
		}

		function updateModeUI() {
			const mode = modeInput.value;
			solidWrap.style.display = mode === 'solid' ? 'block' : 'none';
			gradientWrap.style.display = mode === 'gradient' ? 'grid' : 'none';
			letterWrap.style.display = mode === 'per_letter' ? 'block' : 'none';
			if (mode === 'per_letter') {
				renderLetterPickers();
			}
			renderPreview();
		}

		function setStatus(text, kind) {
			statusEl.textContent = text;
			statusEl.className = 'status ' + (kind || '');
		}

		async function sendForm(action) {
			const brightness = Math.max(0, Math.min(100, Number(brightnessInput.value || 0)));
			const params = new URLSearchParams();
			params.set('key', state.key);
			params.set('action', action);
			params.set('brightness', String(brightness));
			params.set('mode', modeInput.value);
			params.set('solid_color', solidColorInput.value.toUpperCase());
			params.set('gradient_start', gradientStartInput.value.toUpperCase());
			params.set('gradient_end', gradientEndInput.value.toUpperCase());
			if (action === 'custom') {
				params.set('msg', msgInput.value);
				ensureLetterColors();
				params.set('letter_colors', letterColors.join(','));
			}

			try {
				setStatus('Sending...', '');
				const res = await fetch('/set', {
					method: 'POST',
					headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
					body: params.toString()
				});
				const text = await res.text();
				if (!res.ok) {
					throw new Error(text || ('Request failed: ' + res.status));
				}
				setStatus(text || 'Updated', 'ok');
			} catch (error) {
				setStatus(error.message || 'Failed to send request', 'err');
			}
		}

		msgInput.value = state.message;
		modeInput.value = state.mode;
		solidColorInput.value = clampColor(state.solid);
		gradientStartInput.value = clampColor(state.gradientStart);
		gradientEndInput.value = clampColor(state.gradientEnd);
		brightnessInput.value = String(state.brightness);

		msgInput.addEventListener('input', () => {
			if (modeInput.value === 'per_letter') {
				renderLetterPickers();
			}
			renderPreview();
		});
		msgInput.addEventListener('keypress', (e) => {
			if (e.key === 'Enter') {
				document.getElementById('sendCustom').click();
			}
		});
		modeInput.addEventListener('change', updateModeUI);
		solidColorInput.addEventListener('input', () => {
			if (modeInput.value === 'per_letter') {
				ensureLetterColors();
			}
			renderPreview();
		});
		gradientStartInput.addEventListener('input', renderPreview);
		gradientEndInput.addEventListener('input', renderPreview);

		document.getElementById('sendCustom').addEventListener('click', () => sendForm('custom'));
		document.getElementById('useDefault').addEventListener('click', () => sendForm('use_default'));

		updateModeUI();
	</script>
</body>
</html>
)HTML";

	html.replace("__API_KEY__", escapeForJs(String(apiKey)));
	html.replace("__MESSAGE__", escapeForJs(msgValue));
	html.replace("__MODE__", colorModeToString(state.colorMode));
	html.replace("__SOLID__", colorToHex(state.currentColor));
	html.replace("__GRADIENT_START__", colorToHex(state.gradientStartColor));
	html.replace("__GRADIENT_END__", colorToHex(state.gradientEndColor));
	html.replace("__BRIGHTNESS__", String(state.currentBrightnessPct));
	html.replace("__LETTER_COLORS__", escapeForJs(serializePerLetterColors(state)));

	return html;
}
