# EC Choir

Armonizador vocal estilo coro gospel (AU, VST3, Standalone) para macOS.

- Entrada en vivo (efecto de inserción en una pista de voz) o archivo de audio (arrastrar y soltar / LOAD AUDIO + PLAY).
- Detecta la nota cantada y genera hasta 6 voces del coro, ajustadas a la tonalidad y escala elegidas.
- Cada voz: ON, intervalo (en grados de la escala), nivel y paneo.
- Humanize (desfase de tiempo y afinación entre voces), Space (reverb), Gate (umbral de detección), Lead y Choir (niveles).
- Presets: Gospel Triad, Full Choir, Tight Duo.
- Latencia fija (~20 ms) informada al host.

Funciona mejor con una voz sola (monofónica) y limpia.

Instalar: `EC Choir.component` -> `~/Library/Audio/Plug-Ins/Components/`,
`EC Choir.vst3` -> `~/Library/Audio/Plug-Ins/VST3/`. Si macOS lo bloquea: `xattr -cr <archivo>`.
Validar AU: `auval -v aufx Ecch Ecaa`
