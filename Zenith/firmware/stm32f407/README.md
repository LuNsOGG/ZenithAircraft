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

La plataforma está concentrada en `main.c` y `main.h`, conservando la organización existente. `main.c` proporciona SPI1, los temporizadores, EXTI0/PB0 y la preparación de reloj/GPIO USB; `bmp388.c/.h` identifica/configura el sensor y compensa sus datos, y `application.c/.h` coordina el arranque y la adquisición y publica los resultados para Watch. `barometric_altitude.c/.h` captura el cero y calcula/filtra las alturas relativa y barométrica estimada. `serial_output.c/.h` transmite por USB CDC y recibe la orden de cero desde la PC. La adquisición conserva su subconjunto privado de registros; USB incorpora TinyUSB y cabeceras CMSIS oficiales bajo `ThirdParty`, sin HAL ni RTOS. Las direcciones, campos y secuencia se basan en [RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf), [PM0214](https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf) y las [definiciones oficiales del dispositivo](https://github.com/STMicroelectronics/cmsis-device-f4/blob/master/Include/stm32f407xx.h).

- `SystemInit`, llamado antes de inicializar `.data` y `.bss`, habilita la FPU y sitúa la tabla de vectores en la dirección del símbolo `g_pfnVectors` del arranque de ST. No depende de variables inicializadas de C.
- Desde `main` se selecciona explícitamente HSI interno de **16 MHz nominales**, con AHB, APB1 y APB2 sin división. Las esperas de reloj tienen un límite de iteraciones y un código de error; ese límite no representa un tiempo calibrado. Posteriormente USB habilita HSE de 8 MHz y PLLQ de 48 MHz, conservando HSI para CPU, buses y temporizadores.
- TIM2 recibe 16 MHz nominales y genera interrupciones de actualización a **1 kHz nominal**, periodo de **1 ms**: `16000000 / (PSC + 1) / (ARR + 1)`. Se conserva el ajuste actual del contador interno de 2 MHz (`PSC = 7`, `ARR = 1999`). La bandera del evento usado para cargar los divisores se limpia antes de habilitar la interrupción.
- `TIM2_IRQHandler` limpia la bandera y aumenta un contador. El bucle principal espera con `WFI`, aumenta otro contador al continuar y atiende la aplicación. Las transferencias SPI y la compensación se ejecutan en el bucle principal, nunca en las interrupciones. No se usa SysTick.
- TIM5 proporciona un contador libre de 1 MHz nominal para marcas de tiempo, latencia y ventanas de frecuencia; no genera interrupciones. Su resta modular admite el desbordamiento de 32 bits (cada 71,6 minutos aproximadamente).
- EXTI0 se asigna a PB0, flanco ascendente y prioridad 8. Su manejador limpia el pendiente y publica solamente la hora de TIM5 y un número de evento. TIM2 conserva prioridad 15.
- TIM2 y TIM5 se congelan cuando el procesador se detiene en depuración; se mantiene el acceso de depuración durante el sueño normal de `WFI`. El BMP388 externo sigue midiendo durante esas pausas.
- Un HardFault detiene la prueba con un código observable. Es un diagnóstico mínimo, sin captura de contexto ni recuperación automática.

BMI160 y registro SD permanecen como archivos vacíos. La aplicación actual es una prueba secuencial de adquisición, dos alturas y USB CDC con estados de arranque, espera de reset, ejecución y reintento, sin tareas ni colas RTOS. FreeRTOS, UART e integración con SixPack siguen pendientes; las vistas RTOS de la extensión no implican que su kernel esté en el firmware.

La ventana se encuentra en [../../tools/altitude_monitor/](../../tools/altitude_monitor/README.md). Ejecutar `Iniciar_monitor.cmd` o `python monitor.py --auto-connect` desde esa carpeta. En la prueba real, la placa enumeró como COM11. Relojes, dependencias, alimentación, copia de respaldo y resultados de la carga autorizada se detallan en [usb/README.md](usb/README.md).

## Hardware y verificación

El usuario ya cargó la prueba anterior de plataforma, alcanzó `main` y observó por SWD que `g_platform_ready = 1`, `g_platform_error = 0` y que ambos contadores avanzaban al continuar la ejecución. Eso comprueba el arranque básico y las interrupciones, sin medir externamente la precisión temporal.

El usuario también cargó la etapa posterior de identificación SPI: obtuvo `CHIP_ID = 80` (`0x50`), seis consultas correctas de seis, cero errores y `STATUS = 16` (`0x10`). Eso comprueba comunicación e identificación, sin demostrar mediciones de presión/temperatura.

La adquisición y las alturas se verifican por compilación Debug/Release, análisis estático y pruebas de lógica ARM con un sensor simulado. Después, con autorización expresa del usuario, se cargó y verificó Release mediante ST-Link y se comprobaron 20 s de telemetría USB real: 393 tramas, ambas alturas válidas y una orden de cero confirmada, sin errores de decodificación ni huecos de secuencia. La ventana también se comprobó conectada a COM11 y se probó su botón de cero. Estas verificaciones no miden exactitud, pérdida cero universal ni comportamiento de vuelo.

La programación y depuración previstas serán mediante SWD y el HiLetgo ST-Link V2 del usuario. Se instalaron y verificaron en el almacén de Windows los controladores oficiales ST-Link Debug, Bridge y VCP (2.2.0.0), tras completar el asistente con autorización de administrador. Esto no añade una interfaz UART al programador ST-Link V2. El usuario comprobó la comunicación SWD mediante STM32CubeProgrammer 2.23.0 en Hot Plug: ID `0x413`, Cortex-M4, familia STM32F405/F407/F415/F417 y 512 KB de Flash. La frecuencia solicitada fue 1000 kHz y la utilizada, 950 kHz. Esa identificación es consistente con el destino previsto, sin identificar por sí sola el encapsulado o la revisión física de la placa.

La configuración `STM32: Depurar con ST-Link` en `.vscode/launch.json` usa el adaptador de la extensión oficial, solicita 1000 kHz y selecciona `Software system reset`. El usuario ya la utilizó para pruebas anteriores; el log mostró un reset de sistema y la fase de programación de CubeProgrammer en Under Reset/Hardware reset. La carga USB de esta sesión se hizo con CLI, modo Normal, núcleo detenido y reset por software. La configuración de VS Code se conserva: se detiene al llegar a `main`; iniciarla programa Debug y reinicia el microcontrolador, por lo que no es una conexión Hot Plug de solo identificación.

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

## Presión y temperatura: 200 Hz nominales con INT en PB0

Esta etapa fue cargada y probada por el usuario antes de añadir altura: informó aproximadamente 196 Hz efectivos, presión de 83994 Pa, temperatura de 26,10 grados Celsius y cero incidencias en los contadores mostrados. Esa prueba no valida la nueva etapa de altura, la exactitud con instrumentos externos, pérdida cero ni vuelo. Los pasos de programación siguientes son manuales; el agente no programa la placa.

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

Sin FIFO y sin un contador de conversiones del sensor, los diagnósticos permiten detectar algunas pérdidas/acumulaciones, pero no demostrar pérdida cero. La altura añadida después de esta adquisición se describe abajo. La salida USB CDC ya está implementada; SD, UART, BMI160 y FreeRTOS siguen pendientes.

## Alturas barométricas relativa y estimada (cargadas y observadas por USB)

Se conservan SPI1, configuración del BMP388, interrupciones y los siete pares de módulos. La aplicación entrega una muestra a `barometric_altitude` una sola vez después de adquirirla, fuera de las interrupciones, y solamente si `g_bmp388_sample_flags == 1`: nueva, numéricamente válida, sin acumulación ni incertidumbre temporal ni periodo largo. Un despertar sin muestra no suma al promedio ni actualiza el filtro. Esto es más estricto que comprobar solamente `g_bmp388_sample_valid`.

### Modelo, unidades y filtro

Se usa la aproximación de [altura de presión de NOAA/NWS](https://www.weather.gov/media/epz/wxcalc/pressureAltitude.pdf), convertida de pies/hPa a metros/Pa. Sus constantes redondeadas se mantienen juntas en el módulo:

```text
H(P) = 44307.69396 m * [1 - (P / 101325 Pa)^0.190284]
h_rel = H(P_actual) - H(P_referencia)
```

`101325 Pa` es una constante del modelo atmosférico, no el cero local impuesto. La referencia es la media realmente adquirida en el punto inicial. Presiones iguales producen altura cero; menor presión da altura positiva y mayor presión da altura negativa. Se trata de una altura barométrica calculada bajo atmósfera estándar, no de una distancia directa al suelo ni de una altitud absoluta validada. Se admite el mismo intervalo numérico de presión que el controlador, 30000..125000 Pa. No se emplea la temperatura del chip como temperatura del aire y no se recortan presiones para aceptarlas.

La salida sin filtrar se conserva. La suavizada usa un paso bajo de primer orden, discretizado por Euler hacia atrás, con constante de tiempo `tau = 0,5 s`:

```text
dt = resta modular entre timestamps de muestras limpias, en microsegundos
alpha = dt / (500000 + dt)
filtrada += alpha * (h_rel - filtrada)
```

El filtro usa el intervalo observado, sin asumir exactamente 200 Hz. Su primera salida es la primera altura calculada, sin rampa artificial desde cero. También se reinicializa con la primera muestra limpia después de un fallo, conservando la referencia establecida. A 5 ms por muestra, un escalón alcanza aproximadamente el 63 % en 0,5 s y el 95 % en 1,5 s. Estos son resultados del modelo del filtro; su comportamiento con la placa está pendiente. `g_bmp388_read_duration_us` sigue midiendo SPI y compensación; no incluye el cálculo posterior de altura.

### Captura y renovación de la referencia

1. Después del arranque se esperan **10 segundos de muestras limpias y continuas**, desde la primera muestra aceptada. Si falla esa continuidad durante la espera, los diez segundos empiezan de nuevo. Esta espera no garantiza equilibrio térmico.
2. A continuación se promedian **3 segundos completos**, con la placa quieta. Se usa la media y desviación estándar muestral de Welford, en `double`, sin guardar una lista ni reservar memoria dinámica. La ventana se mide por tiempo, no por un número fijo: unas 590 muestras a la cadencia observada y 601 en la simulación de 200 Hz que incluye ambos extremos.
3. Una muestra inválida, un aviso sin dato nuevo, incertidumbre temporal, eventos acumulados o un intervalo entre muestras superior a 10000 us descartan la ventana incompleta y empiezan otra. No se repiten los diez segundos si ya se completaron. Timestamps repetidos tampoco añaden muestras. Si no llegan muestras limpias durante 100 ms, se invalida la salida y cualquier captura incompleta.
4. Al completar la ventana se publican presión media, cantidad y desviación estándar en Pa. La desviación es un diagnóstico de dispersión, no una prueba de precisión o inmovilidad. No se impone un umbral de quietud sin validar; el operador debe mantener la placa quieta.
5. La referencia queda fija durante el movimiento y ante fallos/reintentos del sensor. Los fallos invalidan la altura, sin redefinir automáticamente el cero. Las últimas alturas pueden seguir visibles con `g_baro_valid = 0`; no deben interpretarse como actuales. Un reset del MCU inicia una referencia nueva.
6. Escribir **`g_baro_zero_request = 1`** solicita otro cero. El bucle principal lo consume y lo vuelve a `0`; no reinicia el MCU ni el BMP388. Invalida y pone a cero las publicaciones de la referencia/altura anterior mientras captura tres segundos nuevos. Si los diez segundos iniciales aún no se habían completado, primero vuelve a cumplir esa espera. La orden solo puede ejecutarse cuando se reanuda el núcleo.

### Watch y prueba física manual pendiente

Esta implementación pasó pruebas con un sensor simulado y después se cargó y verificó mediante SWD con autorización del usuario. Se observaron ambas alturas por USB y se comprobó la captura manual del cero desde la ventana de PC; falta contrastar la altura con desplazamientos medidos. Watch se conserva como diagnóstico alternativo. La configuración existente **STM32: Depurar con ST-Link** sigue disponible: F5 programa Flash y reinicia el MCU; no confundirlo con compilar.

| Watch | Qué comprobar |
| --- | --- |
| `g_baro_state` | `0` espera muestras, `1` espera inicial de 10 s, `2` captura de 3 s, `3` referencia lista. El estado 3 no sustituye a `g_baro_valid`. |
| `g_baro_state_elapsed_ms` | Tiempo entre muestras aceptadas de la etapa de espera/captura; al terminar queda cercano a 3000 ms. |
| `g_baro_reference_ready` | `1` solo cuando se ha completado el cero. |
| `g_baro_reference_pa` | Media local en Pa; debe permanecer fija después de la captura y durante el movimiento. |
| `g_baro_reference_samples` | Muestras de la ventana actual; tras finalizar, cantidad usada en la referencia. No incluye las de espera inicial. |
| `g_baro_reference_std_pa` | Desviación estándar muestral de la ventana completada, en Pa. |
| `g_baro_reference_restart_count` | Ventanas incompletas interrumpidas desde el reset; una orden manual no cuenta como fallo. |
| `g_baro_altitude_relative_m` | Altura calculada sin filtrar en metros. |
| `g_baro_altitude_filtered_m` | Altura suavizada en metros, con tau de 0,5 s. |
| `g_baro_valid` | `1` para una altura vigente obtenida de muestras limpias con referencia completa. Leer las alturas solo con este valor y `reference_ready` en 1. |
| `g_baro_zero_request` | Única variable de esta tabla que se debe editar: escribir `1` para solicitar otro cero. |

Procedimiento después de una carga manual autorizada por el usuario:

1. En la parada inicial de `main`, añadir la tabla a Watch, además de `g_bmp388_state`, `g_bmp388_sample_flags`, `g_bmp388_sample_count`, ambas tasas y los contadores de errores, acumulaciones y timeout. Mantener la placa quieta y evitar soplar sobre el sensor o corrientes de aire.
2. Continuar y esperar **al menos 15 segundos sin pausas ni breakpoints**. Pausar preferiblemente en el bucle principal, fuera de la publicación de variables. Esperar estado BMP388 `2`, estado barométrico `3`, `reference_ready = 1`, `valid = 1`, muestras y tasas coherentes y referencia cercana a la presión local. La altura estará alrededor de cero con ruido; no debe ser exactamente cero.
3. Reanudar, subir la placa aproximadamente **1–2 m**, mantenerla quieta al menos cinco segundos y pausar para leer: la altura filtrada debe cambiar hacia valores positivos y la referencia debe conservarse. Comparar también la salida sin filtrar. Repetir al regresar al punto inicial y al bajar por debajo de él: cerca de cero y signo negativo, respectivamente. El ruido y la deriva pueden ocultar movimientos pequeños; esto no prueba exactitud métrica.
4. Para renovar el cero, detenerse con la placa quieta en el punto elegido, escribir `g_baro_zero_request = 1`, reanudar al menos cinco segundos y volver a pausar. Deben verse orden consumida `0`, nueva referencia lista y altura otra vez alrededor de cero. La nueva referencia solo debe cambiar por esta orden o por reset del MCU.
5. Si no termina la referencia, comprobar estados, avance del contador y `reference_restart_count`, además de las banderas BMP388. Si `g_baro_valid = 0`, no interpretar las alturas antiguas ni suponer que una cifra cero representa una medición válida.

Durante una pausa SWD el BMP388 sigue midiendo y TIM5 se congela. El firmware no puede detectar exhaustivamente ese tiempo físico ni garantizar que una captura pausada sea continua. Si se pausa durante la captura, dejar ejecutar más de diez segundos para completar la espera si fuera necesario y luego pedir un nuevo cero con la placa quieta, dejándolo ejecutar sin pausas. Para comparar frecuencias tras cualquier pausa, dejar al menos cinco segundos de ejecución libre. Los `double` y el conjunto de publicaciones de Watch no son atómicos.

El filtro reduce fluctuaciones y añade retraso; promediar la referencia no elimina el ruido de cada lectura posterior. Los cambios meteorológicos y térmicos pueden desplazar la altura estimada. No se declara precisión centimétrica, pérdida cero ni validación de vuelo.

La segunda salida usa directamente `H(P_actual)` con presión estándar de 101325 Pa, sin referencia meteorológica QNH. `g_baro_altitude_estimated_m` conserva el resultado sin filtrar y `g_baro_altitude_estimated_filtered_m` aplica el mismo filtro de 0,5 s. `g_baro_estimated_valid` es independiente de la captura de referencia relativa: la altitud estimada puede ser válida durante la espera/captura. Una orden de cero relativo no reinicia ni desplaza la segunda salida; una pérdida de muestras sí invalida ambas. La ventana recibe las dos alturas filtradas, ambas en metros.

### Comprobaciones de software reproducibles

`tests/check_bmp388.c` se compila para ARM y se ejecuta con un transporte de registros simulado mediante `tests/run_bmp388_checks.py`. Los vectores de compensación se generan independientemente con aritmética decimal de 60 dígitos y ejercitan los 14 coeficientes, incluidos valores con signo. También se verifican datos nuevos, configuración, errores, notificaciones acumuladas, eventos durante una lectura, frecuencia simulada de 200 Hz, timeouts, valores fuera de rango, recuperación y desbordamiento de los relojes. Se conservan las comprobaciones anteriores y se añaden captura de referencia, media/desviación, rechazo de duplicados y ventanas incompletas, signos y vectores independientes de altura, filtro con intervalos variables, cadencia cercana a 196 Hz, cero manual, conservación de la referencia ante fallos e independencia/obsolescencia de la altitud estimada. La prueba actual pasa **183 aserciones, cero fallos**. Los vectores de altura y las estadísticas esperadas también se calculan con Decimal de 60 dígitos, sin usar las funciones matemáticas C del módulo. El monitor añade ocho pruebas del decodificador, que también pasaron.

Requiere Python y `unicorn==2.1.4`. La dependencia de estas pruebas está aislada en `build/checks/python_deps`; no se modificó Python global. Para repetir desde esta carpeta con un Python que tenga esa dependencia disponible:

```powershell
python tests/run_bmp388_checks.py
```

El script localiza GCC ARM en los bundles de STM32, admite `--toolchain-bin` y solo genera archivos de prueba bajo `build/checks/acquisition`. Compila el mismo controlador, aplicación y módulo de altura para un núcleo ARM sin FPU, con operaciones de doble precisión por software; el firmware real se compila aparte para Cortex-M4F. La emulación no ejecuta los registros de plataforma ni demuestra frecuencia, conexiones eléctricas o comportamiento de vuelo. No ejecuta herramientas de programación.

Las compilaciones de esta etapa terminaron correctamente en Debug y Release, enlazando la biblioteca matemática después de los objetos. Tamaños informados por el enlazador (la RAM incluye las reservas configuradas de heap/stack, no una medición de uso máximo en ejecución):

| Configuración | Flash | RAM principal | CCM |
| --- | --- | --- | --- |
| Debug | 71052 bytes | 9960 bytes | 0 bytes |
| Release | 55540 bytes | 9960 bytes | 0 bytes |

También pasaron el análisis estático GCC con avisos estrictos de `main`, `bmp388`, `application`, `barometric_altitude` y `serial_output`, y `git diff --check`. Estas verificaciones de software se complementaron con la prueba física USB descrita en [usb/README.md](usb/README.md). En la sesión del agente, el alias Python y los ejecutables ARM requirieron ejecución autorizada fuera del entorno restringido de Windows. Se conservaron las tareas de VS Code. La carga posterior fue autorizada expresamente, guardó respaldo y reemplazó/verificó los sectores necesarios de Flash; no se borraron archivos. La invocación de `cube.exe cmake` no resolvió el subcomando en esa sesión, de modo que se usó CMake 4.4.0+st.1 directamente desde los bundles, con GCC 14.3.1+st.2 en el PATH del proceso.
