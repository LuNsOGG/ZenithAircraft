# SixPack

Software de ZenithAircraft para visualizar instrumentos y analizar datos de vuelo.

| Carpeta | Contenido y estado |
| --- | --- |
| [sil/](sil/README.md) | Aplicación web actual, escenarios y pruebas SIL |
| [desktop/](desktop/README.md) | Espacio reservado para la aplicación de escritorio |
| [docs/](docs/README.md) | Documentos e información específicos de SixPack |

## SIL

Abre [sil/index.html](sil/index.html) en el navegador. No requiere paquetes. Desde la raíz del repositorio, ejecuta las pruebas con `node SixPack/sil/tests/run-tests.cjs`.

La [guía de uso](docs/informacion/GUIA_SIL.md) explica los controles, secuencias, fallas, registro CSV y comprobaciones. El [reporte del primer parcial](docs/documentos/Trabajo%20para%20el%20primer%20parcial.docx) conserva el contexto académico original.

## Aplicación de escritorio prevista

Permitirá graficar y consultar los datos registrados por la computadora de vuelo; también se contempla conectar los instrumentos a datos recibidos por puerto serial. La plataforma y la implementación se decidirán posteriormente. La organización actual no implica una migración a Python.

Las futuras entradas seriales, archivos y datos de prueba deben coordinarse con [protocols/](../protocols/README.md). El contrato todavía no está definido.
