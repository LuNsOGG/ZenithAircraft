# Firmware principal: STM32F407VET6

Proyecto base en C para la aeronave RC ZenithAircraft, preparado con la extensión oficial STM32CubeIDE for Visual Studio Code. El arranque del microcontrolador usa el archivo ensamblador suministrado por ST.

El entorno usa GCC, CMake y Ninja gestionados por la extensión. El destino es **STM32F407VET6**: 512 KB de Flash, 128 KB de RAM principal y 64 KB de CCM RAM, según el script de enlace generado para esta variante.

## Compilar desde VS Code

1. Abrir esta carpeta `stm32f407` como proyecto en VS Code.
2. Pulsar **Ctrl+Shift+B** para ejecutar `STM32: Compilar Debug`. La tarea configura primero CMake y usa el ejecutable `cube.exe` de la extensión. La versión actual de `tasks.json` conserva la ruta absoluta que el usuario corrigió y probó en VS Code; habrá que revisarla si cambia la carpeta o versión de la extensión.
3. También se pueden usar `CMake: Select Configure Preset` (`Debug` o `Release`) y `CMake: Build` cuando CMake Tools haya detectado el proyecto.

Las versiones de herramientas del proyecto quedan registradas en `.settings/bundles.store.json` y `.settings/bundles-lock.store.json`. La extensión resuelve los ejecutables mediante `cube-cmake`; no se necesitan rutas de instalación escritas en los archivos de CMake.

La compilación genera `build/Debug/zenith_aircraft.elf`, `.hex`, `.bin` y `.map` al seleccionar `Debug`. Compilar no carga firmware ni modifica la placa. Los productos de construcción están excluidos de Git mediante el `.gitignore` local.

## Estructura y alcance actual

- `Core/Inc`: cabeceras de los módulos previstos.
- `Core/Src`: plataforma, identificación y adquisición del BMP388, coordinación de la prueba, arranque, soporte de la biblioteca C y módulos todavía vacíos.
- `cmake`: configuración de construcción y del microcontrolador.
- `.settings` y `.vscode`: integración con la extensión STM32 y CMake Tools.

La plataforma está concentrada en `main.c` y `main.h`, conservando la organización existente. `main.c` proporciona SPI1, los temporizadores y EXTI0/PB0; `bmp388.c/.h` identifica/configura el sensor y compensa sus datos, y `application.c/.h` coordina el arranque y la adquisición y publica los resultados para Watch. Se usa un subconjunto privado de registros del STM32F407, sin dependencias adicionales de HAL ni CMSIS. Las direcciones, campos y secuencia se basan en [RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf), [PM0214](https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf) y las [definiciones oficiales del dispositivo](https://github.com/STMicroelectronics/cmsis-device-f4/blob/master/Include/stm32f407xx.h).

- `SystemInit`, llamado antes de inicializar `.data` y `.bss`, habilita la FPU y sitúa la tabla de vectores en la dirección del símbolo `g_pfnVectors` del arranque de ST. No depende de variables inicializadas de C.
- Desde `main` se selecciona explícitamente HSI interno de **16 MHz nominales**, con AHB, APB1 y APB2 sin división. Las esperas de reloj tienen un límite de iteraciones y un código de error; ese límite no representa un tiempo calibrado. No se activa el cristal externo ni el PLL.
- TIM2 recibe 16 MHz nominales y genera interrupciones de actualización a **1 kHz nominal**, periodo de **1 ms**: `16000000 / (PSC + 1) / (ARR + 1)`. Se conserva el ajuste actual del contador interno de 2 MHz (`PSC = 7`, `ARR = 1999`). La bandera del evento usado para cargar los divisores se limpia antes de habilitar la interrupción.
- `TIM2_IRQHandler` limpia la bandera y aumenta un contador. El bucle principal espera con `WFI`, aumenta otro contador al continuar y atiende la aplicación. Las transferencias SPI y la compensación se ejecutan en el bucle principal, nunca en las interrupciones. No se usa SysTick.
- TIM5 proporciona un contador libre de 1 MHz nominal para marcas de tiempo, latencia y ventanas de frecuencia; no genera interrupciones. Su resta modular admite el desbordamiento de 32 bits (cada 71,6 minutos aproximadamente).
- EXTI0 se asigna a PB0, flanco ascendente y prioridad 8. Su manejador limpia el pendiente y publica solamente la hora de TIM5 y un número de evento. TIM2 conserva prioridad 15.
- TIM2 y TIM5 se congelan cuando el procesador se detiene en depuración; se mantiene el acceso de depuración durante el sueño normal de `WFI`. El BMP388 externo sigue midiendo durante esas pausas.
- Un HardFault detiene la prueba con un código observable. Es un diagnóstico mínimo, sin captura de contexto ni recuperación automática.

BMI160, cálculo de altura, registro y salida serial permanecen como archivos vacíos. La aplicación actual es una prueba secuencial de adquisición con estados de arranque, espera de reset, ejecución y reintento, sin tareas ni colas. FreeRTOS aún no está integrado; las vistas RTOS de la extensión no implican que su kernel esté en el firmware.

## Hardware y verificación

El usuario ya cargó la prueba anterior de plataforma, alcanzó `main` y observó por SWD que `g_platform_ready = 1`, `g_platform_error = 0` y que ambos contadores avanzaban al continuar la ejecución. Eso comprueba el arranque básico y las interrupciones, sin medir externamente la precisión temporal.

El usuario también cargó la etapa posterior de identificación SPI: obtuvo `CHIP_ID = 80` (`0x50`), seis consultas correctas de seis, cero errores y `STATUS = 16` (`0x10`). Eso comprueba comunicación e identificación, sin demostrar mediciones de presión/temperatura.

La nueva etapa de adquisición se verifica por compilación Debug/Release, análisis estático, enlace de vectores y pruebas de lógica ARM con un sensor simulado. Estas comprobaciones no validan físicamente INT, los valores compensados ni la frecuencia efectiva. Esta preparación no ha cargado el nuevo firmware ni iniciado una sesión con el ST-Link.

La programación y depuración previstas serán mediante SWD y el HiLetgo ST-Link V2 del usuario. Se instalaron y verificaron en el almacén de Windows los controladores oficiales ST-Link Debug, Bridge y VCP (2.2.0.0), tras completar el asistente con autorización de administrador. Esto no añade una interfaz UART al programador ST-Link V2. El usuario comprobó la comunicación SWD mediante STM32CubeProgrammer 2.23.0 en Hot Plug: ID `0x413`, Cortex-M4, familia STM32F405/F407/F415/F417 y 512 KB de Flash. La frecuencia solicitada fue 1000 kHz y la utilizada, 950 kHz. Esa identificación es consistente con el destino previsto, sin identificar por sí sola el encapsulado o la revisión física de la placa.

La configuración `STM32: Depurar con ST-Link` en `.vscode/launch.json` usa el adaptador de la extensión oficial, solicita 1000 kHz y selecciona `Software system reset`. El usuario ya la utilizó para la prueba de plataforma; el log mostró un reset de sistema y la fase de programación de CubeProgrammer en Under Reset/Hardware reset. No se ha utilizado para cargar esta nueva prueba SPI. Se detiene al llegar a `main`; iniciarla programa la imagen Debug y reinicia el microcontrolador, por lo que no es una conexión Hot Plug de solo identificación.

## Carga y observación de la plataforma por SWD

1. Abrir **esta carpeta `stm32f407`** en VS Code, con la placa alimentada y el ST-Link conectado como en la comprobación SWD. Cerrar otras sesiones que estén utilizando el programador.
2. Pulsar **Ctrl+Shift+B** y confirmar que `STM32: Compilar Debug` termina correctamente. Este paso solo compila.
3. Abrir **Ejecutar y depurar** (`Ctrl+Shift+D`) y seleccionar **STM32: Depurar con ST-Link**.
4. **Cuando se decida cargar el firmware**, pulsar **F5**. Esta acción compila, programa la imagen `build/Debug/zenith_aircraft.elf` y reinicia el MCU. La programación reemplaza el contenido de las regiones de Flash necesarias para la imagen. No ejecutar borrado masivo ni cambios de protección. Si hay errores de conexión o una solicitud de actualización del ST-Link, detenerse y revisar el mensaje.
5. Al detenerse en la entrada de `main`, la inicialización de reloj y TIM2 dentro de `main` todavía no se ha ejecutado: `g_platform_ready` debe seguir en 0. Añadir a **Inspección / Watch** las expresiones de la tabla siguiente.
6. Pulsar **F5 / Continuar**, esperar unos segundos y pulsar **Pausar**. Comprobar `g_platform_ready = 1`, `g_platform_error = 0` y valores crecientes de ambos contadores. Continuar y pausar otra vez para comparar. Watch normalmente se actualiza con el procesador detenido; no se necesita lectura en vivo.
7. Para confirmar la interrupción, poner un breakpoint en `++g_tim2_irq_count`, continuar y comprobar que se alcanza `TIM2_IRQHandler`. Retirar ese breakpoint al terminar: detenerse en cada interrupción impide observar ejecución continua. Los breakpoints y pausas alteran la prueba temporal.
8. Tras detener la sesión, reiniciar físicamente la placa y volver a iniciar la depuración si se quiere repetir la prueba desde cero. Una nueva sesión F5 puede volver a programar la imagen y se detendrá de nuevo en `main`; no conserva el estado de los contadores del arranque anterior.

| Expresión | Resultado esperado después de continuar y pausar |
| --- | --- |
| `SystemCoreClock` | `16000000`, valor nominal configurado; no es una medición de frecuencia. |
| `g_platform_ready` | `1` una vez terminada la inicialización. |
| `g_platform_error` | `0`: sin error detectado; `1`: espera HSI agotada; `2`: espera de cambio de reloj agotada; `3`: HardFault. |
| `g_tim2_irq_count` | Aumenta nominalmente unas 1000 veces por segundo de ejecución libre. Cuenta interrupciones atendidas, no garantiza medir cada periodo si se bloquean interrupciones. |
| `g_main_wake_count` | Aumenta al retornar de `WFI` al bucle principal. Puede diferir del contador de interrupciones, por ejemplo por la actividad del depurador. |

Los contadores son `volatile uint32_t`: empiezan en cero en cada reset y vuelven a cero por desbordamiento después de `2^32` incrementos (aproximadamente 49,7 días para el contador de TIM2 a 1 kHz). TIM2 no acumula tiempo mientras el núcleo está detenido por el depurador. La precisión del HSI y el periodo real aún requieren medición externa; `g_platform_ready = 1` no sustituye la comprobación de que las interrupciones avanzan.

## Etapa anterior: identificación del BMP388 por SPI (comprobada por el usuario)

Esta sección conserva el alcance y la interpretación de la prueba anterior. El firmware actual añade adquisición y escritura de configuración, descritas en la sección siguiente; para probar la nueva imagen, seguir esa sección.

El usuario informó que conectó un módulo CJMCU-388 con este cableado. No se ha comprobado eléctricamente en esta preparación:

| CJMCU-388 | STM32F407VET6 | Uso actual |
| --- | --- | --- |
| VIN | Alimentación de 3,3 V según el cableado informado | Alimentación; confirmar el rango de la revisión real del módulo. |
| GND | GND común | Referencia eléctrica. |
| SCK | PA5 / SPI1_SCK, AF5 | Reloj de 1 MHz nominal. |
| SDI | PA7 / SPI1_MOSI, AF5 | Comandos y relojes para leer. |
| SDO | PA6 / SPI1_MISO, AF5 | Datos recibidos del sensor. |
| CS | PA4 / GPIO | Activo en bajo; alto antes de configurar la salida y entre transacciones. |
| INT | PB0 / entrada GPIO | Reservado, sin EXTI y sin interrupción habilitada en el BMP388. |
| 3Vo | Sin conectar | No se asume que sea entrada de alimentación. |

SPI1 usa modo 0 (CPOL = CPHA = 0), cuatro señales, 8 bits, MSB primero y NSS por software. PCLK2 se mantiene a 16 MHz nominales, dividido entre 16 para SCK de 1 MHz nominal. Los cambios GPIO se limitan a PA4..PA7 y PB0; se conservan los pines SWD PA13/PA14. MISO tiene una resistencia interna débil de bajada para definir el nivel cuando no hay una salida conectada.

Después de al menos 10 ticks de TIM2 desde la inicialización (10 ms nominales), la aplicación consulta el sensor y repite una vez por cada 1000 ticks. Se leen `CHIP_ID` (`0x00`, esperado `0x50`), `ERR_REG` (`0x02`) y `STATUS` (`0x03`), según la [hoja de datos de Bosch](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp388-ds001.pdf), secciones 4.3.1..4.3.3 y 5.3.2. Cada lectura SPI envía dirección con el bit 7 en uno, un byte de descarte y después los datos, manteniendo CS bajo hasta finalizar. Las esperas TXE/RXNE/BSY son acotadas por iteraciones; en error se libera CS y se reinicializa solamente SPI1 para el siguiente intento.

No se escriben registros del BMP388, no se envía reset del sensor y no se configura adquisición a 200 Hz. Leer `ERR_REG` sí tiene el efecto documentado por Bosch de limpiar `cmd_err` y `conf_err`; por eso se conserva también una acumulación de los bits de error observados. La identificación correcta no valida presión, temperatura, altura, calibración, INT o frecuencia de muestreo.

Para cargar y probar manualmente:

1. Con la sesión anterior detenida (`Shift+F5`), revisar el cableado con la alimentación desconectada. Alimentar placa y módulo como en el montaje informado, con GND común y el ST-Link conectado.
2. Abrir esta carpeta `stm32f407`, guardar los archivos y ejecutar `STM32: Compilar Debug`. La ruta absoluta corregida de `cube.exe` en `tasks.json` se conserva. Si VS Code muestra otra vez el selector de terminar tareas en lugar de compilar, recargar la ventana como en la prueba anterior; no continuar con F5 si la compilación falla.
3. Cuando se decida programar, seleccionar `STM32: Depurar con ST-Link` y pulsar F5. Este paso reemplaza las regiones necesarias de Flash por la nueva imagen y reinicia el MCU. Esta preparación no ha ejecutado ese paso.
4. En la parada inicial de `main` añadir las expresiones de la tabla siguiente a Watch mediante su botón `+`. Todavía deben estar a cero y `g_bmp388_last_result = 0`, porque no se ejecutó la prueba.
5. Continuar con F5, esperar unos tres segundos y pausar. Leer los resultados preferiblemente detenido en el bucle de `main`, fuera de `Application_Poll` y de las transferencias SPI: la publicación de las variables ocurre por pasos, no como una instantánea atómica.
6. Continuar y pausar otra vez tras unos segundos. Deben aumentar los intentos y los éxitos, y mantenerse en cero los errores si las respuestas son correctas. Los contadores de TIM2 y del bucle principal también deben seguir avanzando.
7. Si falla, conservar identificación, resultado, estado, registro de errores y contadores para diagnosticar. No cambiar conexiones con alimentación ni asumir que una lectura `0x00`/`0xFF` demuestra un modelo concreto.

| Watch | Resultado esperado tras completar una consulta |
| --- | --- |
| `g_bmp388_chip_id` | `80` decimal = `0x50`. Otro valor se publica sin aceptarlo como BMP388; `0x60` tampoco se acepta automáticamente. |
| `g_bmp388_probe_ok` | `1` si la última consulta completa fue correcta; `0` si falló o aún no se completó. |
| `g_bmp388_last_result` | `1` si fue correcta; códigos detallados debajo. |
| `g_bmp388_probe_count` | Intentos terminados desde el reset; aumenta aproximadamente uno por segundo de ejecución libre. |
| `g_bmp388_success_count` | Consultas con ID correcto, sin errores del sensor y con `cmd_rdy = 1`. Debe aumentar. |
| `g_bmp388_error_count` | Intentos fallidos acumulados; idealmente `0`. No se borra al recuperarse una lectura. |
| `g_bmp388_error_reg` | Último `ERR_REG` leído, esperado `0`. |
| `g_bmp388_error_flags_seen` | OR acumulado de bits 0..2 de errores leídos, esperado `0`. |
| `g_bmp388_status` | Último `STATUS` leído; bit 4 (`0x10`) indica `cmd_rdy`. Habitualmente `16` tras encendido sin adquisición. |

`g_bmp388_last_result`: `0` = aún no probado; `1` = correcto; `2` = espera SPI agotada; `3` = error SPI del STM32 (MODF/OVR/FRE); `4` = identificación distinta de `0x50`; `5` = bits de error del BMP388; `6` = decodificador de comandos aún no listo; `7` = argumento interno inválido. Un sensor desconectado puede producir bytes `0x00`/`0xFF` y código `4` sin que el controlador SPI genere un timeout: SPI no tiene confirmación de presencia del esclavo.

Al fallar una transferencia, los campos no obtenidos se publican en cero para no reutilizar respuestas de intentos anteriores. Por tanto, `error_reg = 0` o `status = 0` no son evidencia de una lectura correcta si `last_result` informa un fallo. Los contadores vuelven a cero con cada reset y se desbordan de forma modular a `2^32`; no son contadores de muestras de presión ni una medición de 200 Hz. Las pausas del depurador congelan TIM2 y alteran el intervalo entre consultas.

Las referencias existentes incluyen nombres STM32F407VGT6 y no constituyen una validación física del encapsulado VET6 ni de la revisión de conectores de la placa. El cableado de esta prueba es el informado por el usuario.

El código reutilizable podrá ubicarse en [../shared/](../shared/README.md).

## Presión y temperatura: 200 Hz nominales con INT en PB0 (sin cargar)

Se conserva el cableado y SPI1 a 1 MHz nominal. PB0 ahora usa EXTI0 por flanco ascendente, con una resistencia interna débil de bajada. El BMP388 se configura con salida INT push-pull, activa en alto y no enclavada. No se usa FIFO ni se cambia el bus a SPI de tres señales.

El arranque espera 10 ticks de TIM2 (10 ms nominales), identifica el sensor, envía un reset por software **solo al BMP388**, espera otros 10 ticks y lee los 21 bytes de calibración de `0x31..0x45`. Los coeficientes se decodifican con su signo y escala binaria. Bloques de calibración de todos ceros/todos unos o sin coeficiente T2 se rechazan. Este control detecta algunos fallos de comunicación; no prueba la exactitud de la calibración física.

Después se escriben y verifican mediante lectura los siguientes registros:

| Registro / expresión Watch | Valor esperado | Configuración |
| --- | --- | --- |
| `g_bmp388_registers.pwr_ctrl` | `51` / `0x33` | Presión y temperatura habilitadas, modo normal. |
| `g_bmp388_registers.osr` | `0` | Sobremuestreo x1 en ambos canales. |
| `g_bmp388_registers.odr` | `0` | ODR seleccionada de 200 Hz nominales, periodo nominal de 5 ms. |
| `g_bmp388_registers.config` | `0` | IIR desactivado para esta primera prueba. |
| `g_bmp388_registers.int_ctrl` | `66` / `0x42` | DRDY habilitado, push-pull, activo alto, no enclavado. |
| `g_bmp388_registers.if_conf` | `0` | SPI de cuatro señales. |

La configuración y las ecuaciones de compensación se basan en la [hoja de datos de Bosch, revisión 1.7](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp388-ds001.pdf), apartados 3.7.5.3, 3.9, 4.3 y 9; se contrastaron con la [API oficial BMP3](https://github.com/boschsensortec/BMP3_SensorAPI). La implementación local de las ecuaciones usa `double`, calcula primero temperatura y luego presión con los coeficientes de la misma unidad. Las salidas son Pa y grados Celsius. No se recortan valores fuera de rango para hacerlos parecer válidos.

Cada aviso de PB0 publica un número de evento y la marca temporal de atención de su interrupción. El bucle principal comprueba DRDY en `INT_STATUS` y los dos bits de dato nuevo en `STATUS`, lee los seis bytes de presión/temperatura en una sola ráfaga y compensa. Despertar por TIM2, una lectura repetida o un evento sin dato nuevo no incrementan el contador de muestras válidas. El contador de eventos funciona como una notificación de última muestra, no como una cola de muestras: si se acumulan varios eventos se lee una sola trama y se contabilizan las notificaciones excedentes.

Las variables originales de identificación se conservan para consultas de salud aproximadamente cada segundo; **`g_bmp388_success_count` cuenta consultas de identificación, no muestras**. Los contadores de adquisición se describen abajo. Al fallar arranque, configuración o la consulta de salud, se invalida la adquisición y se reintenta el arranque después de un segundo nominal. No recibir tramas durante 100 ms genera un diagnóstico de datos ausentes, sin sustituir silenciosamente PB0 por sondeo de adquisición.

### Prueba manual de esta imagen

1. Detener la sesión anterior con Shift+F5. Mantener el cableado acordado, especialmente `INT -> PB0`; cambiar conexiones únicamente sin alimentación.
2. Compilar Debug. Cuando se decida cargar, seleccionar `STM32: Depurar con ST-Link` y pulsar F5. **F5 programa la nueva imagen y reinicia el MCU**; esta preparación no ha realizado esa operación.
3. En la primera parada de `main`, las variables de adquisición todavía están en cero. Añadir a Watch las expresiones de la tabla siguiente; `g_bmp388_registers` se puede añadir como estructura y desplegar.
4. Continuar sin breakpoints en interrupciones ni en cada lectura. Dejar ejecutar entre 5 y 10 segundos y pausar preferiblemente en el bucle principal, fuera de SPI y de la publicación de variables. Las variables, especialmente las de 64 bits, no constituyen una instantánea atómica si se detiene el procesador en medio de su actualización.
5. Confirmar estado 2, configuración/calibración válidas, muestras e interrupciones crecientes, presión/temperatura razonables y cero errores. La presión local depende de la elevación y del clima; no debe exigirse que sea 101325 Pa. La temperatura es la del sensor, influida por la placa, y no equivale necesariamente a la temperatura del aire.
6. Confirmar que `g_bmp388_rate_valid = 1` y comparar las tasas de IRQ y muestras. El objetivo es estar cerca de 200 Hz y de 5000 us entre avisos, no demostrar exactamente 200 Hz por la mera selección del registro. La tolerancia del reloj del sensor y los tiempos de conversión pueden cambiar el resultado.
7. Tras una pausa, el BMP388 sigue funcionando aunque TIM2/TIM5 estén congelados. Las tramas producidas durante la pausa no se conservan en FIFO ni se cuentan exhaustivamente. Continuar al menos cinco segundos antes de evaluar otra ventana de frecuencia; no usar el primer evento tras reanudar para medir el periodo.

| Watch | Interpretación |
| --- | --- |
| `g_bmp388_state` | `0` espera inicial, `1` espera tras reset, **`2` adquisición**, `3` espera para reintentar. |
| `g_bmp388_calibration_valid` / `g_bmp388_configured` | Deben ser **1** durante adquisición. Configurado no demuestra que PB0 esté funcionando. |
| `g_bmp388_pressure_pa` | Presión compensada en Pa; solo interpretar como válida con `sample_valid = 1`. |
| `g_bmp388_temperature_c` | Temperatura compensada en grados Celsius. |
| `g_bmp388_sample_valid` | **1** si la última lectura aportó una muestra finita dentro de 30000..125000 Pa y -40..85 grados Celsius, sin diagnóstico de obsolescencia. Es un control de plausibilidad, no calibración externa. |
| `g_bmp388_sample_count` | Secuencia/contador de muestras compensadas aceptadas desde reset. |
| `g_bmp388_frame_count` | Tramas nuevas leídas, incluidas las que dieron valores fuera de rango. |
| `g_bmp388_irq_count` | Avisos atendidos por EXTI0, no una garantía de contar todas las conversiones. |
| `g_bmp388_sample_time_us` | Hora TIM5 del aviso asociado a la última trama; no es el instante exacto de conversión del ADC. Resta modular de 32 bits. |
| `g_bmp388_period_us` | Intervalo entre los últimos avisos atendidos por el bucle principal; unos 5000 us en condiciones nominales sin acumulaciones. |
| `g_bmp388_service_latency_us` | Tiempo desde la marca del aviso hasta comenzar a leer. |
| `g_bmp388_read_duration_us` | Tiempo de servicio que incluye SPI, decodificación y compensación. |
| `g_bmp388_irq_rate_hz` / `g_bmp388_sample_rate_hz` | Tasas calculadas en ventanas de al menos un segundo de TIM5. No son una medición con referencia externa: el HSI del STM32 también tiene tolerancia. |
| `g_bmp388_rate_valid` | 1 después de completar una ventana; 0 al arrancar o reintentar. |
| `g_bmp388_event_overrun_count` | Notificaciones acumuladas que no se pueden atender individualmente (`pendientes - 1`). No cuenta todas las muestras perdidas posibles. |
| `g_bmp388_irq_gap_count` | Intervalos observados mayores que 10000 us; no estima un número exacto de conversiones ausentes. |
| `g_bmp388_no_data_count` | Avisos atendidos sin los indicadores de muestra nueva; no se duplica la muestra anterior. |
| `g_bmp388_data_timeout_count` | Diagnósticos periódicos de 100 ms sin tramas. Si configuración es válida e IRQ sigue en cero, revisar INT/PB0 y la salida del módulo. |
| `g_bmp388_invalid_sample_count` | Tramas cuya compensación no fue finita o quedó fuera del rango de operación. |
| `g_bmp388_sample_flags` | Bits: `1` valor válido, `2` asociación temporal incierta, `4` eventos acumulados, `8` periodo largo, `16` dato obsoleto, `32` valor fuera de rango. Valor habitual sin incidencias: **1**. Validez numérica y certeza temporal son distintas. |
| `g_bmp388_raw_pressure` / `g_bmp388_raw_temperature` | ADC de 24 bits de la última trama; no están en unidades físicas. |
| `g_bmp388_int_status` | Registro de interrupciones leído antes de los datos; bit 3 (`0x08`) indica DRDY. Se limpia al leerlo. |

Se mantienen los códigos anteriores de `g_bmp388_last_result` y se añaden: **8** calibración inválida; **9** lectura de configuración distinta de lo solicitado; **10** no hay datos nuevos; **11** compensación fuera de rango/no finita; **12** ausencia de tramas durante 100 ms. El valor normal al completar una muestra es **1**. Durante la espera inicial tras reset puede mostrarse transitoriamente **6**.

Sin FIFO y sin un contador de conversiones del sensor, los diagnósticos permiten detectar algunas pérdidas/acumulaciones, pero no demostrar pérdida cero. No se implementan altura barométrica, SD, serial, BMI160 ni FreeRTOS en esta etapa.

### Comprobaciones de software reproducibles

`tests/check_bmp388.c` se compila para ARM y se ejecuta con un transporte de registros simulado mediante `tests/run_bmp388_checks.py`. Los vectores de compensación se generan independientemente con aritmética decimal de 60 dígitos y ejercitan los 14 coeficientes, incluidos valores con signo. También se verifican datos nuevos, configuración, errores, notificaciones acumuladas, eventos durante una lectura, frecuencia simulada de 200 Hz, timeouts, valores fuera de rango, recuperación y desbordamiento de los relojes. La prueba actual pasa **103 aserciones**.

Requiere Python y `unicorn==2.1.4`. La dependencia de estas pruebas está aislada en `build/checks/python_deps`; no se modificó Python global. Para repetir desde esta carpeta con un Python que tenga esa dependencia disponible:

```powershell
python tests/run_bmp388_checks.py
```

El script localiza GCC ARM en los bundles de STM32, admite `--toolchain-bin` y solo genera archivos de prueba bajo `build/checks/acquisition`. Compila el mismo controlador y aplicación para un núcleo ARM sin FPU, con operaciones de doble precisión por software; el firmware real se compila aparte para Cortex-M4F. La emulación no ejecuta los registros de plataforma ni demuestra frecuencia, conexiones eléctricas o comportamiento de vuelo. No ejecuta herramientas de programación.
