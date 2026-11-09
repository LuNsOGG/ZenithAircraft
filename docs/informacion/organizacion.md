# Organización y conservación

ZenithAircraft reúne SixPack y Zenith en un solo repositorio. La carpeta local continúa siendo `F:\VisualStudioCode\SixPack`; todas las operaciones de esta reorganización se realizan dentro de esa ruta.

## Ubicación del contenido anterior

| Ubicación anterior | Ubicación actual |
| --- | --- |
| `SIL-SixPack/index.html`, `styles.css`, `jsconfig.json`, `src/` y `tests/` | `SixPack/sil/`, con el mismo contenido |
| `SIL-SixPack/GUIA_SIL.md` | `SixPack/docs/informacion/GUIA_SIL.md` |
| `README.md` original de la raíz | `SixPack/docs/informacion/README_SIL_original.md` |
| `SIL-SixPack/README.md` original | `SixPack/docs/informacion/README_SIL_carpeta_original.md` |
| `Trabajo para el primer parcial.docx` | `SixPack/docs/documentos/Trabajo para el primer parcial.docx` |
| `ZenithV3/` | `Zenith/hardware/referencias/ZenithV3/` |

Los archivos originales se trasladaron sin eliminar contenido. Los README vigentes se añadieron por separado y los originales se conservaron como referencia histórica. Se mantuvo también la subcarpeta local vacía que estaba dentro de `ZenithV3/`.

El [inventario inicial](organizacion-inicial.json) registra las rutas original y actual, tamaño y hash SHA-256 de los 20 archivos que existían antes de reorganizar. Permite verificar su conservación byte por byte. Git conserva además el historial previo.

## Responsabilidades

- **SixPack:** instrumentos, escenarios SIL, pruebas y futura aplicación de escritorio para visualizar y analizar registros de vuelo.
- **Zenith:** proyectos STM32F407VET6 y STM32F411 Black Pill, código compartido y hardware.
- **Documentación:** `docs/documentos/` e `informacion/` en cada área; documentación común en `docs/` en la raíz.
- **Protocolos:** mensajes y formatos de archivos compartidos en `protocols/`, todavía por definir.
- **Datos:** futuros registros de ejemplo en `data/examples/`, con unidades y procedencia documentadas.

La aplicación de escritorio y el firmware todavía no están implementados. Esta reorganización conserva la lógica del SIL y no elige una tecnología para escritorio.

## Ejecutar y verificar

Abre `SixPack/sil/index.html` en el navegador. Desde la raíz ejecuta `node SixPack/sil/tests/run-tests.cjs` con Node.js 18 o posterior. La apariencia y la pantalla completa requieren revisión en navegador real.

Para localizar instrucciones anteriores, ten presente que `SIL-SixPack/` pasó a `SixPack/sil/`. Los README históricos conservan las rutas originales.
