## Plan: Pipeline de Assets con Modelos de Difusion

Ajustar el pipeline de generacion para usar el inventario real de modelos disponibles en Lemonade, separando claramente generacion base, refinado y upscale, y cerrando la conversion hacia el formato de spritesheet que hoy usa Pakal.

**Steps**
1. Phase 1: Contrato de entrada y perfiles por tipo de asset
2. Definir un manifiesto de job por asset con campos: tipo, dimensiones objetivo, estilo, paleta, semilla, modelo preferido, fallback, y reglas de postproceso.
3. Crear perfiles de tipo para: personaje, enemigo, proyectil, tile, fondo panoramico, y variacion de decoracion.
4. Incluir reglas de validacion por perfil (tamano exacto, transparencia, borde seguro, cantidad de frames, nomenclatura).

5. Phase 2: Enrutamiento de modelos segun capacidad
6. Generacion base de calidad (concepto y primera version): SDXL-Base-1.0, Qwen-Image-2512-GGUF, Qwen-Image-GGUF, Flux-2-Klein-9B-GGUF, Flux-2-Klein-4B.
7. Generacion rapida para exploracion y lotes de prueba: SDXL-Turbo, SD-Turbo, Z-Image-Turbo.
8. Upscale y restauracion solamente cuando aplique: RealESRGAN-x4plus y RealESRGAN-x4plus-anime.
9. Regla de uso para pixel-art gameplay: evitar upscale automatico en sprites de gameplay final; usar upscale solo en fondos o en etapa intermedia si luego se hace cuantizacion y limpieza.
10. Fallback por disponibilidad y latencia: 9B a 4B, luego SDXL-Base, luego Turbo para no bloquear pipeline.

11. Phase 3: Flujo de procesamiento por etapas
12. Stage A Generacion: producir N variantes por asset con semillas controladas y prompt versionado.
13. Stage B Seleccion automatica: filtrar por metrica tecnica (alpha valida, dimensiones, nitidez de contorno, ruido).
14. Stage C Postproceso: cuantizacion de paleta, ajuste de bordes transparentes, normalizacion de pivote visual, opcional ESRGAN segun perfil.
15. Stage D Aprobacion humana: gate corto para elegir variante final por asset critico (player, enemy, bullets, fondo principal).

16. Phase 4: Empaque y compatibilidad con engine
17. Empaquetar atlas con TexturePacker en modo CLI usando proyecto tps versionado por categoria de asset.
18. Formato objetivo de entrega para este scope (ContraGame): Custom JSON (PakalSpriteSheet v1.0) + PNG atlas.
19. Si se usa TexturePacker o Aseprite como intermedio, convertir en el mismo job hacia PakalSpriteSheet v1.0 para entregar salida lista sin intervencion manual.

20. Phase 5: Integracion con repo y CI
21. Publicar salida final en Assets del modulo ContraGame con estructura estable por categoria y version.
22. Ejecutar validadores de consistencia antes de build: schema de metadata, tamanos, nombres, transparencia y conteo de frames.
23. Correr build y smoke test del juego para validar carga real de recursos y ausencia de regresiones.
24. Guardar manifiesto de lote con hashes de entradas y salidas para rollback reproducible.

**Formato objetivo para el agente externo (sin acceso al codigo)**
- Elegir y usar como salida final para ContraGame: Custom JSON PakalSpriteSheet v1.0 + atlas PNG.
- Entregar por cada atlas:
1. `atlas.png`
2. `atlas.pakal_spritesheet.json`

**Schema logico PakalSpriteSheet v1**
1. Root:
- `format`: string fijo `PakalSpriteSheet`
- `version`: string fijo `1.0`
- `meta`: objeto
- `animations`: array de animaciones
2. `meta`:
- `image`: nombre o ruta relativa del atlas PNG
- `size`: `{ "w": int, "h": int }`
- `scale`: string en formato `pixels:meters` (ejemplo `100:1`)
- `default_animation`: nombre de animacion por defecto
3. Cada `animation`:
- `name`: string
- `looped`: bool
- `duration_ms`: int (duracion total de la animacion)
- `frames`: array ordenado de frames
4. Cada `frame`:
- `x`, `y`, `w`, `h`: int (rectangulo en pixeles dentro del atlas)
- `oX`, `oY`: float o int (offset visual)
- `pX`, `pY`: float o int (pivot)

**Ejemplo de salida esperada**
```json
{
	"format": "PakalSpriteSheet",
	"version": "1.0",
	"meta": {
		"image": "knight_spritesheet.png",
		"size": { "w": 2560, "h": 4800 },
		"scale": "100:1",
		"default_animation": "knight_idle"
	},
	"animations": [
		{
			"name": "knight_idle",
			"looped": true,
			"duration_ms": 200,
			"frames": [
				{ "x": 0, "y": 0, "w": 512, "h": 512, "oX": 0, "oY": 0, "pX": 0, "pY": 0 },
				{ "x": 512, "y": 0, "w": 512, "h": 512, "oX": 0, "oY": 0, "pX": 0, "pY": 0 }
			]
		},
		{
			"name": "knight_run",
			"looped": true,
			"duration_ms": 120,
			"frames": [
				{ "x": 0, "y": 512, "w": 512, "h": 512, "oX": 0, "oY": 0, "pX": 0, "pY": 0 },
				{ "x": 512, "y": 512, "w": 512, "h": 512, "oX": 0, "oY": 0, "pX": 0, "pY": 0 }
			]
		}
	]
}
```

**Reglas de conversion desde herramientas comunes**
1. TexturePacker/Aseprite pueden usarse como formato intermedio, pero la entrega final debe ser PakalSpriteSheet v1.
2. Agrupar frames por prefijo de nombre (ejemplo: `knight_idle_01`, `knight_idle_02` -> `knight_idle`).
3. Calcular duracion total por animacion: `duration_ms = round(1000 * frame_count / fps_perfil)`.
4. Si no hay pivot/offset explicitos, usar `0` en `oX`, `oY`, `pX`, `pY`.

**Nota de evolucion (fuera de scope actual)**
- Para futuros generos tipo beat em all, se puede extender a PakalSpriteSheet v1.1 con metadata de combate por frame (hurtbox/hitbox/pushbox/eventos).
- En este plan de ContraGame no se requiere esa extension; v1.0 es suficiente.

**Naming obligatorio**
1. Convencion de frame: `<asset>_<anim>_<index>` con `index` numerico de 2-4 digitos.
2. Animaciones permitidas por perfil deben definirse en el manifiesto del job (ejemplo: idle, run, jump, shoot, hit, die).
3. No mezclar diferentes assets en una sola serie de nombres.

**Verification**
1. Validar que cada job selecciona modelo segun tabla de enrutamiento y activa fallback cuando falte capacidad.
2. Verificar que los outputs cumplan dimensiones, alpha y naming por perfil.
3. Confirmar que atlas y metadata PakalSpriteSheet v1 se generan sin errores (schema, campos obligatorios, animacion default valida).
4. Ejecutar build debug y smoke runtime para asegurar carga de assets en juego.
5. Revisar visualmente legibilidad de sprites de gameplay y calidad de fondos.
6. Validar que no se exijan campos de combate por frame en este scope (sin hitboxes en metadata v1.0).

**Decisions**
- Se asume que todos los modelos listados son de difusion y se usan con sd-cpp bajo Lemonade.
- RealESRGAN se trata como etapa de upscale y no como generacion base.
- Para gameplay 2D tipo Contra se prioriza consistencia de silueta y legibilidad sobre detalle fino.
- Para este entregable de ContraGame, se estandariza PakalSpriteSheet v1.0 sin hitboxes por frame.
- La extension v1.1 para beat em all queda explicitamente fuera de scope actual.
- Scope incluido: pipeline de generacion, seleccion, empaque y conversion.
- Scope excluido: entrenamiento de modelos y fine-tuning propio en esta fase.

**Further Considerations**
1. Recomendacion: comenzar con una matriz pequena de benchmark por tipo de asset para fijar el modelo default por categoria antes de escalar a produccion.
2. Recomendacion: separar pipeline de personajes y pipeline de fondos, porque tienen criterios de calidad diferentes.
3. Recomendacion: agregar umbrales de tiempo por job para evitar que modelos pesados bloqueen entregas de arte diarias.
