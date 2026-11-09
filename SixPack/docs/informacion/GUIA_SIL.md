# Six Pack SIL

Primer prototipo de los seis instrumentos de aviación. Las variables se introducen manualmente y el panel las representa en el navegador.

## Abrir el proyecto

1. Abre la carpeta `SIL-SixPack` de este repositorio en Visual Studio Code.
2. Abre `index.html` en un navegador moderno. También puedes usar la opción de VS Code para abrirlo en el navegador.
3. Cambia los campos de la columna «Condición de vuelo». El panel lee las entradas cada 100 ms y anima las indicaciones hacia los nuevos valores.

## Funciones de prueba

- Los botones «Vuelo nivelado», «Ascenso», «Viraje» y «Descenso» cargan condiciones de ejemplo. Son instantáneas independientes, no una simulación física continua. Después puedes modificar cualquier valor a mano.
- En «Secuencia dinámica» hay cinco perfiles: nivelado, ascenso, viraje, descenso y ascenso con viraje. «Iniciar» reproduce el perfil, «Pausar» congela el tiempo, «Continuar» reanuda y «Reiniciar» vuelve al comienzo. Iniciar desde cero o reiniciar restablece las señales; durante la reproducción puedes cortar una señal para probar la falla. Si editas cualquier campo manual, la secuencia se detiene y quedan los valores actuales en el formulario.
- La línea de tiempo permite saltar directamente a un instante y deja el recorrido en pausa. Puedes continuar desde ahí. Las velocidades 0.5×, 1× y 2× modifican el ritmo de reproducción, no las tasas físicas indicadas: 600 ft/min siguen siendo 600 ft/min en tiempo simulado. Cambiar de velocidad conserva el instante actual. Al ocultar la pestaña, el recorrido se pausa para no saltarse parte de la demostración.
- En las secuencias, la altitud se obtiene integrando la velocidad vertical y el rumbo integrando la velocidad de viraje. Los demás datos siguen trayectorias programadas. Esto permite observar el panel, pero no representa la dinámica completa de una aeronave.
- El botón «Señal OK» de cada variable corta o restablece su señal. Una señal perdida deja sin indicación a los instrumentos que dependen de esa variable; el valor escrito se conserva para cuando se restablezca.
- «Fallas por sistema» permite cortar la disponibilidad del pitot (ASI) o de la presión estática (ASI, ALT y VSI). Estas pruebas simulan pérdida de datos, no los errores físicos de un tubo bloqueado. Las fallas se superponen: restablecer pitot no recupera ASI si todavía está activa la falla estática. Los botones individuales muestran «Sistema» mientras una falla de sistema los afecte. «Restablecer todas las señales» recupera todos los canales.
- «Registrar prueba» añade una fila con la entrada, la indicación visible en ese instante y el estado. También puede usarse durante una secuencia dinámica; la fila incluye el perfil y el tiempo transcurrido. Las filas se conservan en el almacenamiento local del navegador cuando está disponible. «Exportar CSV» descarga la tabla para presentarla fuera del navegador. No se borran pruebas al restablecer el formulario.
- El ajuste «reducir movimiento» del sistema operativo desactiva las transiciones suaves.
- «Ejecutar comprobaciones» verifica ocho casos con resultado esperado: ascenso, descenso, viraje cruzando el norte, un perfil completo, fallas de pitot/estática, fallas superpuestas y una entrada fuera de rango. La tabla muestra PASÓ/NO PASÓ y agrega la evidencia al registro y al CSV. Usa datos independientes, por lo que conserva tu vuelo actual.
- «Presentación» amplía el six-pack y oculta controles y tablas. Puedes pausar o continuar la secuencia desde la barra superior. «Volver a controles» o Escape sale del modo. El botón adicional «Pantalla completa» utiliza esa función del navegador cuando está disponible.

No requiere instalar Python, Node ni paquetes de JavaScript.

## Carpetas y archivos

- `index.html`: estructura del panel y controles.
- `styles.css`: presentación adaptable a distintos tamaños de pantalla.
- `src/flight-state.js`: variables, límites y validación de entradas.
- `src/dynamic-scenarios.js`: perfiles temporales y cálculo de altitud/rumbo durante las secuencias.
- `src/playback.js`: reloj de reproducción, pausa, velocidad y selección de instante.
- `src/instruments.js`: dibujo de ASI, AI, ALT, TC, HI y VSI.
- `src/verification.js`: comprobaciones automáticas con valores esperados independientes.
- `src/app.js`: lectura de entradas y actualización de la interfaz.
- `tests/run-tests.cjs`: pruebas de regresión de lógica, controles, registro y dibujo con un DOM/Canvas simulado.
- `jsconfig.json`: configuración del proyecto JavaScript para Visual Studio Code.

## Comprobaciones manuales sugeridas

- Cambia la velocidad a 30 kt y el cabeceo a 20°. ASI y AI deben actualizarse sin reiniciar la página.
- Cambia el rumbo de 359° a 0° y comprueba que la rosa gira sin perder la indicación.
- Deja vacío un campo o introduce un valor fuera del rango mostrado. El instrumento correspondiente debe indicar «SIN DATOS».
- Carga «Viraje», corta la señal de velocidad de viraje y comprueba que sólo el coordinador de viraje quede «SIN SEÑAL».
- Reproduce «Ascenso con viraje»: el altímetro debe subir, el rumbo cambiar y ASI, AI, TC y VSI deben responder. Pausa la secuencia y comprueba que el tiempo deja de avanzar; reanúdala y después edita una entrada para detenerla.
- Registra una prueba, recarga la página y verifica que siga en la tabla; exporta el CSV. Si el navegador no permite almacenamiento local, exporta antes de cerrar.
- Usa «Restablecer valores» para regresar a la condición inicial.

Las escalas gráficas son preliminares. Este prototipo representa las variables ingresadas; todavía no calcula variables a partir de sensores.

## Revisión técnica y futuras extensiones

El panel conserva escalas genéricas; no usa límites de operación de una aeronave específica. `SixPackState.createSnapshot` concentra la validación y el formato común de datos. La entrada manual, las secuencias y las comprobaciones utilizan ese contrato. Las fallas de disponibilidad se resuelven antes de dibujar; el reloj y el generador de trayectorias están separados de las carátulas. Esto permite añadir más fuentes después sin duplicar el dibujo ni la validación.

Para ejecutar las pruebas de desarrollo, con Node 18 o posterior instalado, abre una terminal en esta carpeta y ejecuta `node tests/run-tests.cjs`. No requiere paquetes. Estas pruebas cubren tiempos de reproducción, salto y pausa, fallas superpuestas, captura inmediata, resultados esperados, CSV, recarga del registro, control manual, modo presentación y almacenamiento no disponible. El uso normal del SIL no requiere Node.

La revisión visual final debe hacerse en el navegador, incluyendo pantalla completa y tamaños de ventana pequeños. El registro anterior se conserva: las comprobaciones agregan filas y no se descartan datos si el almacenamiento local falla. Si aparece el aviso de almacenamiento, exporta el CSV antes de cerrar.
