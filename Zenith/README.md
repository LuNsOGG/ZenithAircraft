# Zenith

Computadora de vuelo de ZenithAircraft: firmware, hardware y documentación técnica.

| Carpeta | Propósito |
| --- | --- |
| [firmware/stm32f407/](firmware/stm32f407/README.md) | Proyecto principal para STM32F407VET6 |
| [firmware/stm32f411/](firmware/stm32f411/README.md) | Prototipos con STM32F411 Black Pill |
| [firmware/shared/](firmware/shared/README.md) | Código que pueda compartirse entre placas |
| [hardware/](hardware/README.md) | Esquemáticos, PCB, conexiones y referencias |
| [docs/](docs/README.md) | Documentos e información específicos de Zenith |

Las carpetas de firmware están preparadas para desarrollo futuro; todavía no contienen proyectos de STM32CubeIDE, configuraciones de pines ni controladores implementados. La separación de carpetas no valida la compatibilidad de las placas o sensores.

El código común se incorporará a `shared/` cuando se identifique qué partes son reutilizables. La configuración de cada microcontrolador permanecerá en su proyecto correspondiente.

La comunicación con SixPack y el formato de los registros se definirán en [protocols/](../protocols/README.md).
