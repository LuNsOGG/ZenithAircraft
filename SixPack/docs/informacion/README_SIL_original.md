# Six Pack SIL

Prototipo académico de los seis instrumentos básicos de vuelo, desarrollado en HTML, CSS y JavaScript. Funciona en el navegador con entradas manuales, escenarios de demostración y comprobaciones automáticas.

## Ejecutar

1. Clona o descarga este repositorio.
2. Abre `SIL-SixPack/index.html` en un navegador moderno. También puedes servir la carpeta con Live Server desde Visual Studio Code.
3. Modifica las entradas o inicia una secuencia dinámica desde los controles.

El uso del panel no requiere Python, Node.js ni instalar paquetes.

## Funciones

- Six-pack: velocidad indicada (ASI), horizonte artificial (AI), altímetro (ALT), coordinador de viraje (TC), rumbo (HI) y velocidad vertical (VSI).
- Escenarios fijos y cinco secuencias dinámicas, con pausa, reinicio, selección de instante y velocidades de reproducción de 0.5×, 1× y 2×.
- Validación de entradas, pérdida individual de señales y fallas de disponibilidad de pitot y presión estática.
- Registro de entrada, indicación y estado, persistencia local en el navegador y exportación CSV.
- Ocho comprobaciones automáticas con valores esperados y modo presentación con pantalla completa.

## Pruebas de desarrollo

Con Node.js 18 o posterior, ejecuta desde la raíz del repositorio:

```sh
node SIL-SixPack/tests/run-tests.cjs
```

No requiere dependencias externas. La suite cubre lógica, reproducción, controles, fallas combinadas, registro, exportación y presentación con un DOM/Canvas simulado. La apariencia y la pantalla completa también deben revisarse en un navegador real.

## Estructura y documentación

- [`SIL-SixPack/GUIA_SIL.md`](SIL-SixPack/GUIA_SIL.md): uso, pruebas manuales, módulos y límites del prototipo.
- `SIL-SixPack/src/`: estado, perfiles dinámicos, reloj, instrumentos, comprobaciones e interfaz.
- `SIL-SixPack/tests/`: pruebas de regresión sin paquetes adicionales.
- [`Trabajo para el primer parcial.docx`](Trabajo%20para%20el%20primer%20parcial.docx): reporte académico de la primera fase; conserva el contexto original y no describe necesariamente todas las funciones actuales.

Las escalas son genéricas. Los perfiles son trayectorias programadas para demostración, y las fallas representan pérdida de datos. El prototipo todavía no recibe datos de sensores ni del puerto serial.
