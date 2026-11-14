# Monitor de alturas ZenithAircraft

Ventana local de Python/Tkinter para la conexión USB CDC de la STM32F407VET6.
Muestra altura relativa al cero capturado y altitud barométrica estimada bajo
atmósfera estándar, ambas en metros y suavizadas por el firmware con tau=0,5 s.
La segunda cifra no usa QNH meteorológico ni representa una elevación validada.

## Abrir

Ejecutar `Iniciar_monitor.cmd` en Windows, o desde esta carpeta:

```powershell
python monitor.py --auto-connect
```

Requiere Python 3.10+, Tkinter y pyserial 3.5. En la PC del usuario ya estaban
disponibles Tkinter y pyserial; no se instaló ni cambió Python global. La lista
de dependencias queda en `requirements.txt`.

La conexión automática solo selecciona dispositivos con VID/PID de desarrollo
`CAFE:4001`. En la prueba de esta placa Windows asignó **COM11**; el número puede
cambiar. Si hay más de una placa con ese VID/PID, seleccionar manualmente el
puerto correcto. Un puerto solo puede estar abierto en una aplicación a la vez.

## Uso

1. Alimentar la placa por su USB y conectar el cable de datos a la PC.
2. Abrir el monitor. El inicio del firmware espera 10 s de adquisición limpia y
   captura otros 3 s de presión para la referencia relativa. La altitud estimada
   puede mostrarse antes de terminar esa captura.
3. Mantener la placa quieta durante la captura. Las cifras sin dato válido se
   muestran como `— m`; no se presenta un cero ficticio como medición.
4. Para cambiar el cero relativo, pulsar **Capturar cero relativo** y mantener la
   placa quieta durante tres segundos. La placa confirma la orden. Esta acción
   no reinicia el MCU ni cambia el origen de la altitud estimada.
5. Las gráficas conservan los últimos 30 s. Si pasan más de 0,5 s sin tramas,
   se ocultan las cifras y se indica que no hay dato reciente. Los cortes de
   validez dejan huecos en las gráficas. Los buffers y la memoria están limitados.

`--demo` permite revisar la interfaz sin hardware. El título y el estado indican
**DEMOSTRACIÓN** permanentemente; no se mezcla ese modo con datos reales.

## Protocolo

USB CDC nativo, líneas JSON ASCII con versión `v:1`, terminadas en LF, hasta
511 bytes. El firmware intenta enviar una actualización cada 50 ms nominales,
fuera de las interrupciones y sin esperar a la PC. Si no cabe una trama completa
en el buffer TX, la omite y aumenta `drops`. USB usa paquetes de 64 bytes; una
línea puede llegar fragmentada y el lector la reconstruye.

Campos: `seq` secuencia de telemetría, `ms` tick TIM2, `p` Pa, `t` grados Celsius,
`rel` altura relativa filtrada en m, `asl` altitud estimada filtrada en m,
`rel_ok`/`asl_ok` validez independiente, `state` estado del cero, `elapsed` ms de
la etapa, `ref` presión de referencia en Pa, `n` muestras, `std` desviación en Pa,
`hz` tasa de adquisición observada con TIM5, `drops` tramas USB omitidas,
`err` errores BMP388, `overruns` avisos excedentes y `zero` órdenes aceptadas.
Los timestamps son relojes de la placa, no UTC ni una referencia externa.

La única orden aceptada es `ZERO\n`. No hay órdenes de reset, programación ni
gestión de archivos. El decodificador rechaza versiones incompatibles, campos
incoherentes/no finitos y líneas excesivas, y recupera sincronía en el siguiente LF.
La velocidad configurada al abrir el COM es 115200 por compatibilidad con la API
serial de la PC; no configura un UART físico ni limita USB a esa tasa.

## Comprobaciones

```powershell
python -m unittest test_telemetry.py -v
```

Pasaron ocho pruebas de unidades, validez independiente, fragmentación, valores
inválidos, límites de memoria, resincronización y contadores. La ventana se
revisó visualmente en Windows. Desde la carpeta del firmware, la prueba física
reproducible es `python tests/check_usb_live.py --duration 20 --zero`; debe
cerrarse el monitor antes de usarla para liberar el puerto.

La prueba real inicial recibió 393 tramas en 20 s, sin errores de decodificación
ni huecos de secuencia, con ambas alturas válidas y confirmación de una orden
de cero. Los contadores mostrados de error BMP388, avisos excedentes y tramas USB
omitidas permanecieron en cero. No es una validación de precisión, de pérdida
cero en cualquier condición, de operación prolongada ni de vuelo.
