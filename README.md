# ZenithAircraft

Proyecto de aviónica que reúne **SixPack**, el software de visualización y análisis de vuelo, y **Zenith**, la computadora de vuelo y su firmware. Ambos se desarrollan en este mismo repositorio.

## Organización

| Carpeta | Propósito |
| --- | --- |
| [SixPack/](SixPack/README.md) | SIL y futura aplicación de escritorio |
| [Zenith/](Zenith/README.md) | Firmware para STM32F407VET6, prototipos STM32F411 Black Pill y hardware |
| [docs/](docs/README.md) | Documentos e información generales del proyecto |
| [protocols/](protocols/README.md) | Contrato de datos compartido entre firmware y aplicaciones |
| [data/examples/](data/examples/README.md) | Futuros registros de ejemplo para desarrollo y pruebas |

Cada área tiene `docs/documentos/` para reportes y archivos de consulta y `docs/informacion/` para guías, decisiones y notas técnicas. Los documentos específicos se guardan en SixPack o Zenith; los que afectan a ambas partes se guardan en `docs/` en la raíz.

## Ejecutar el SIL actual

Abre [SixPack/sil/index.html](SixPack/sil/index.html) en un navegador moderno, o sirve esa carpeta con Live Server. Su uso no requiere Python, Node.js ni instalar paquetes.

Incluye ASI, horizonte artificial, altímetro, coordinador de viraje, rumbo y VSI; entradas manuales; cinco secuencias dinámicas; pausa, salto y velocidad de reproducción; validación y fallas de disponibilidad; registro local y CSV; ocho comprobaciones automáticas y modo presentación.

Con Node.js 18 o posterior, ejecuta desde la raíz:

```sh
node SixPack/sil/tests/run-tests.cjs
```

La suite no requiere dependencias externas y utiliza un DOM/Canvas simulado. La apariencia y la pantalla completa deben revisarse también en un navegador real. Consulta la [guía del SIL](SixPack/docs/informacion/GUIA_SIL.md).

## Estado y alcance

- **Disponible:** SIL web con datos manuales y secuencias programadas.
- **Por desarrollar:** aplicación de escritorio, integración serial y firmware para ambas placas. Las carpetas reservadas no contienen todavía implementaciones de esas funciones.
- **Hardware principal previsto:** STM32F407VET6. **Prototipado:** STM32F411 Black Pill.
- Las escalas del SIL son genéricas. Las secuencias no simulan la dinámica completa de una aeronave y las fallas representan pérdida de datos, no bloqueos físicos del sistema pitot-estático.
- La tecnología de la aplicación de escritorio y el protocolo de comunicación siguen pendientes de definición.

La carpeta local del proyecto es `F:\VisualStudioCode\ZenithAircraft`.

Consulta la [guía de organización y conservación](docs/informacion/organizacion.md) para ubicar los archivos anteriores, incluido el reporte del primer parcial y las referencias de `ZenithV3`.
