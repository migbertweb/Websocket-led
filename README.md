# WebSocket LED Control - ESP32-C3

Proyecto para ESP32-C3 que implementa un servidor WebSocket para controlar un LED y monitorear un sensor DHT11 mediante una interfaz web moderna y responsive.

## 📋 Descripción

Este proyecto permite controlar remotamente un LED conectado al ESP32-C3 a través de una interfaz web utilizando WebSockets. Además, integra un sensor DHT11 para monitorear temperatura y humedad, y una pantalla OLED para mostrar el estado del sistema.

### Características principales

- ✅ Control remoto de LED vía WebSocket
- 🌡️ Monitorización de temperatura y humedad con sensor DHT11
- 📱 Interfaz web moderna y responsive
- 📺 Pantalla OLED 72x40 para visualización local
- 🔌 Configuración WiFi STA (Station)
- 💾 Sistema de archivos SPIFFS para servir archivos web
- 🔄 Actualización automática de datos del sensor

## 🔧 Hardware Requerido

- **ESP32-C3** (microcontrolador)
- **LED** conectado al GPIO2
- **Sensor DHT11** conectado al GPIO4
- **Pantalla OLED SSD1306 72x40** conectada por I2C:
  - SCL: GPIO6
  - SDA: GPIO5
- **Resistencia pull-up** de 4.7kΩ para el sensor DHT11 (opcional, si el módulo no la incluye)

## 📦 Componentes del Proyecto

### Componentes personalizados

- **`dht11`**: Driver para el sensor DHT11 con comunicación bit-banging
- **`oled`**: Driver para pantalla OLED SSD1306 72x40 con primitivas de dibujo
- **`led_control`**: Control simple de LED en GPIO2
- **`websocket_server`**: Servidor HTTP/WebSocket para control remoto
- **`fonts`**: Fuente bitmap 5x7 para la pantalla OLED

### Archivos web (SPIFFS)

- **`index.html`**: Interfaz principal de usuario
- **`style.css`**: Estilos CSS modernos y responsive
- **`websocket.js`**: Cliente WebSocket con lógica de reconexión y auto-refresh

## 🚀 Instalación

### Prerequisitos

1. **ESP-IDF v5.0 o superior** instalado y configurado
2. **Python 3.7+** con pip
3. **Herramientas de desarrollo ESP-IDF** configuradas

### Pasos de instalación

1. **Clonar el repositorio**:
```bash
git clone https://github.com/migbertweb/Websocket-led.git
cd Websocket-led
```

2. **Configurar el proyecto**:
```bash
idf.py set-target esp32c3
idf.py menuconfig
```

3. **Configurar WiFi** (opcional, también se puede editar directamente en código):
   - Edita `components/websocket_server/websocket_server.c`
   - Modifica las credenciales WiFi en la función `wifi_init_sta()`:
   ```c
   .ssid = "TU_SSID",
   .password = "TU_PASSWORD",
   ```

4. **Montar archivos SPIFFS**:
   - Los archivos en `spiffs/` se montarán automáticamente durante la compilación

5. **Compilar y flashear**:
```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**Nota**: Reemplaza `/dev/ttyUSB0` con el puerto serie de tu sistema (ej: `COM3` en Windows, `/dev/tty.usbserial-*` en macOS).

## 🔌 Configuración de Pines

| Componente | GPIO | Notas |
|-----------|------|-------|
| LED | GPIO2 | Controlado por salida digital |
| DHT11 Data | GPIO4 | Comunicación bit-banging |
| OLED SCL | GPIO6 | I2C Clock |
| OLED SDA | GPIO5 | I2C Data |

**I2C Configuration**:
- Dirección OLED: `0x3C`
- Frecuencia: `400kHz`
- Modo: Master

## 📖 Uso

### Inicio del sistema

1. **Encender el ESP32-C3**: El sistema iniciará automáticamente
2. **Pantalla OLED**: Mostrará pantallas de inicio y luego el estado del sistema
3. **Conexión WiFi**: El ESP32 se conectará automáticamente a la red configurada
4. **Logs serie**: La IP asignada se mostrará en el monitor serie

### Interfaz Web

1. **Conectar**: Abre un navegador web y visita la IP mostrada en los logs o en la pantalla OLED
   - Ejemplo: `http://192.168.1.100`
2. **Control LED**: Usa los botones para encender, apagar o hacer toggle del LED
3. **Monitor DHT11**: 
   - Haz clic en "Actualizar" para solicitar datos manualmente
   - Activa "Auto" para actualización automática cada 5 segundos

### Comandos WebSocket

El servidor WebSocket acepta los siguientes comandos:

| Comando | Descripción |
|---------|-------------|
| `ON` | Enciende el LED |
| `OFF` | Apaga el LED |
| `TOGGLE` | Cambia el estado del LED |
| `STATUS` | Solicita el estado actual del LED |
| `GET_DHT` | Solicita datos del sensor DHT11 |

**Formato de respuesta**:
- LED: `LED:ENCENDIDO` o `LED:APAGADO`
- DHT11: `DHT:temperatura:humedad` (ej: `DHT:23.5:65.0`)

## 📁 Estructura del Proyecto

```
Websocket-led/
├── main/
│   └── main.c                 # Punto de entrada y tareas principales
├── components/
│   ├── dht11/
│   │   ├── dht11.c           # Implementación driver DHT11
│   │   └── include/
│   │       └── dht11.h       # Interfaz driver DHT11
│   ├── oled/
│   │   ├── oled.c            # Implementación driver OLED
│   │   └── include/
│   │       └── oled.h        # Interfaz driver OLED
│   ├── led_control/
│   │   ├── led_control.c     # Control de LED
│   │   └── include/
│   │       └── led_control.h # Interfaz control LED
│   ├── websocket_server/
│   │   ├── websocket_server.c # Servidor HTTP/WebSocket
│   │   └── include/
│   │       └── websocket_server.h
│   └── fonts/
│       ├── fonts.c           # Fuente bitmap 5x7
│       └── include/
│           └── fonts.h
├── spiffs/
│   ├── index.html            # Interfaz web principal
│   ├── style.css             # Estilos CSS
│   └── websocket.js          # Cliente WebSocket
├── partitions.csv            # Tabla de particiones
├── CMakeLists.txt            # Configuración CMake principal
├── LICENSE                   # Licencia MIT
└── README.md                 # Este archivo
```

## 🔍 Troubleshooting

### El ESP32 no se conecta a WiFi

- Verifica las credenciales en `websocket_server.c`
- Revisa que la red WiFi esté en modo 2.4GHz (ESP32 no soporta 5GHz)
- Verifica la señal WiFi con un dispositivo móvil

### La interfaz web no carga

- Verifica que SPIFFS se haya montado correctamente (revisa los logs)
- Asegúrate de que los archivos estén en `spiffs/`
- Revisa que la partición `storage` tenga suficiente espacio

### El sensor DHT11 no responde

- Verifica las conexiones físicas
- Asegúrate de tener una resistencia pull-up de 4.7kΩ en el pin de datos
- Verifica que el GPIO4 esté correctamente configurado
- El sensor requiere al menos 2 segundos entre lecturas

### La pantalla OLED no muestra nada

- Verifica las conexiones I2C (SCL y SDA)
- Verifica que la dirección I2C sea `0x3C`
- Revisa que los pines GPIO5 y GPIO6 estén correctos
- Algunos módulos OLED pequeños requieren ajustes en los offsets (ver `oled.h`)

## 📝 Logs del Sistema

El proyecto utiliza ESP_LOG para registro. Para ver los logs:

```bash
idf.py monitor
```

Niveles de log por componente:
- `MAIN`: Inicialización general del sistema
- `WEB_SOCKET`: Servidor HTTP/WebSocket
- `LED_CONTROL`: Control de LED
- `DHT11`: Lecturas del sensor
- `OLED`: Operaciones de pantalla

## 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor:

1. Fork el repositorio
2. Crea una rama para tu característica (`git checkout -b feature/nueva-caracteristica`)
3. Commit tus cambios (`git commit -am 'Agrega nueva característica'`)
4. Push a la rama (`git push origin feature/nueva-caracteristica`)
5. Abre un Pull Request

## 📄 Licencia

Este proyecto está licenciado bajo la **MIT License**. Ver el archivo [LICENSE](LICENSE) para más detalles.

### Nota sobre licencia educativa

Se recomienda encarecidamente, aunque no es obligatorio, que las obras derivadas mantengan este mismo espíritu de código libre y abierto, especialmente cuando se utilicen con fines educativos o de investigación.

## 👤 Autor

**migbertweb**

- GitHub: [@migbertweb](https://github.com/migbertweb)

## 🙏 Agradecimientos

- Espressif Systems por el framework ESP-IDF
- Comunidad de desarrolladores ESP32
- Contribuidores de código abierto

## 📚 Referencias

- [Documentación ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [DHT11 Datasheet](https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf)
- [SSD1306 OLED Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

---

**¡Disfruta del proyecto!** 🚀
