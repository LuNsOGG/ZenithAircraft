# USB CDC para la placa STM32F407VET6

TinyUSB 0.18.0, CMSIS Core 5.9.0 y cabeceras ST cmsis-device-f4 v2.6.10 se
conservan con sus licencias bajo `ThirdParty`. `usb_dependencies.json` registra
versiones, URLs y SHA256 de los ZIP originales, guardados en `build/dependencies`.
`tools/fetch_usb_dependencies.py` reproduce la descarga y no sobrescribe fuentes
existentes que difieran. Solo se compilan dispositivo CDC, núcleo USB y DWC2 FS;
no hay HAL, RTOS, host USB, almacenamiento masivo ni DFU.

Se conservan los siete pares de módulos de aplicación. `serial_output.c/.h`
contiene descriptores, enlace CDC y telemetría; `main.c` prepara relojes y GPIO.

- CPU/AHB/APB1/APB2 siguen con HSI a 16 MHz nominales. TIM2, TIM5 y SPI1 conservan
  sus divisores; no se cambia el cristal de referencia de esas mediciones.
- USB usa el HSE de 8 MHz indicado por Y2 en el esquema de referencia, con
  PLLM=8, PLLN=192, PLLQ=4: 48 MHz nominales. La salida PLLP no se usa como CPU.
- PA11/PA12 usan AF10. Se preservan SPI PA4..PA7, PB0 y SWD PA13/PA14.
- IRQ OTG_FS tiene prioridad 10, por debajo de EXTI0 (8) y por encima de TIM2 (15).
  El controlador atiende transferencias USB; formato JSON y comandos se procesan
  en main. `tud_task_ext(0, false)` no espera al host. No se usa DMA.
- El esquema no lleva VBUS a PA9: se desactiva su detección y se opera con la
  placa alimentada por USB. No se declara soporte de desconexión VBUS en una
  configuración alimentada por otra fuente.
- Buffers CDC: RX 128 bytes, TX 1024 bytes; paquete 64 bytes. No se almacenan
  muestras históricas cuando la PC no lee. Telemetría nominal de 20 Hz, sin
  cambiar la adquisición. Pila mínima reservada: 4096 bytes.
- VID/PID `CAFE:4001` de desarrollo privado. No constituye una identidad USB de
  producto asignada para distribución. Cadena de serie: UID de 96 bits del MCU.

Variables de diagnóstico: `g_usb_ready`, `g_usb_error`, `g_usb_mounted`,
`g_usb_connected`, `g_usb_irq_count`, `g_usb_frame_count`, `g_usb_drop_count`,
`g_usb_zero_count`, `g_usb_command_error_count`.
Errores: 0 ninguno, 1 timeout HSE, 2 timeout al detener PLL, 3 timeout al arrancar
PLL, 4 fallo al inicializar TinyUSB. Un fallo inicial de reloj USB deja continuar
la adquisición BMP388; no cambia silenciosamente a HSI para el USB.

## Alimentación y carga verificadas en esta sesión

El usuario confirmó alimentación de la placa por USB-C, y únicamente SWDIO,
SWCLK y GND entre su ST-Link V2 y la STM32. Las salidas 3V3/5V del programador
no se conectaron a la placa; GND es común. Esta descripción no se debe aplicar
a ciegas a un pin VTref/VAPP de un programador diferente: en un ST-Link oficial
esa referencia de tensión no es una salida de alimentación.

Se guardó la imagen previa completa, 524288 bytes, en
`build/flash_backups/before_usb_20261114T021524Z.bin`, con SHA256
`58786cd8092354cb3c7039dc0abe1728566e9efeacc02937ea7534b199e5d1b4`.
El primer intento de programación en Hot Plug falló al iniciar el borrado.
La lectura de opciones confirmó RDP=0xAA y ninguna protección WRP activa.
La carga autorizada de Release funcionó en modo Normal, núcleo detenido y reset
por software, sin modificar protecciones ni hacer borrado masivo. CubeProgrammer
programó los sectores de la aplicación (0..3), verificó la descarga y ejecutó
reset de sistema. No se borraron archivos del proyecto.

Windows enumeró **COM11**, VID/PID `CAFE:4001`, serie
`004C00483138511136353436`. La prueba real de 20 s recibió 393 tramas y confirmó
ambas alturas y una orden ZERO, sin errores de decodificación ni saltos de
secuencia. Al final: presión 84151,43 Pa, temperatura 25,67 °C, relativa 0,042 m,
altitud estimada 1538,333 m, adquisición reportada 196,8 Hz, cero errores BMP388,
avisos excedentes y omisiones USB. Son cifras de esa prueba, no exactitud medida.
Las tramas y resumen están en `build/checks/usb_live_20261114T022250417834Z.*`.

Construcciones verificadas: Debug 71052 bytes de Flash, Release 55540 bytes;
RAM informada 9960 bytes en ambas, incluyendo reservas de heap/pila. La copia de
respaldo se conserva; no se hizo commit ni push ni se modificaron SixPack o EasyEdaPro.
