/**
 * Controlador WebSocket para comunicación con ESP32
 * Maneja la conexión WebSocket, envío de comandos y actualización de la interfaz
 * Incluye funcionalidades para control LED y monitorización DHT11
 */
class WebSocketController {
    constructor() {
        // Configuración de conexión WebSocket
        this.websocket = null;
        this.reconnectInterval = 3000;        // Intervalo de reconexión en ms
        this.maxReconnectAttempts = 5;        // Máximo de intentos de reconexión
        this.reconnectAttempts = 0;           // Contador de intentos actual
        
        // Configuración de auto-actualización DHT11
        this.autoRefreshInterval = null;      // Referencia al intervalo de auto-refresh
        this.isAutoRefresh = false;           // Estado del auto-refresh
        this.autoRefreshTime = 5000;          // Intervalo de auto-refresh (5 segundos)
        
        console.log('🔄 Inicializando controlador WebSocket...');
        this.initializeEventListeners();
        this.connectWebSocket();
        
        // Activar auto-refresh automáticamente después de 3 segundos de la conexión
        setTimeout(() => {
            if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
                this.startAutoRefresh();
            }
        }, 3000);
    }

    /**
     * Configura los event listeners para los elementos de la interfaz
     */
    initializeEventListeners() {
        // Configurar listeners cuando el DOM esté completamente cargado
        document.addEventListener('DOMContentLoaded', () => {
            const btnOn = document.getElementById('btnOn');
            const btnOff = document.getElementById('btnOff');
            const btnToggle = document.getElementById('btnToggle');
            
            // Asignar event listeners a los botones de control LED
            if (btnOn) btnOn.addEventListener('click', () => this.sendCommand('ON'));
            if (btnOff) btnOff.addEventListener('click', () => this.sendCommand('OFF'));
            if (btnToggle) btnToggle.addEventListener('click', () => this.sendCommand('TOGGLE'));
            
            console.log('✅ Event listeners de botones configurados');
        });

        // Limpiar recursos cuando se cierre la página
        window.addEventListener('beforeunload', () => {
            this.stopAutoRefresh();
            if (this.websocket) {
                this.websocket.close();
            }
        });
    }

    /**
     * Establece la conexión WebSocket con el servidor ESP32
     */
    connectWebSocket() {
        try {
            const host = window.location.host;  // Obtener host actual
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${host}/ws`;
            
            console.log('🔗 Conectando WebSocket a:', wsUrl);
            this.websocket = new WebSocket(wsUrl);
            
            // Evento cuando la conexión se establece correctamente
            this.websocket.onopen = (evt) => {
                console.log('✅ WebSocket CONECTADO correctamente');
                this.updateConnectionStatus(true);
                this.reconnectAttempts = 0;  // Reiniciar contador de reconexiones
                
                // Solicitar estados iniciales después de 1 segundo
                setTimeout(() => {
                    console.log('📋 Solicitando estado inicial del LED...');
                    this.sendCommand('STATUS');
                    
                    console.log('🌡️ Solicitando datos DHT11 iniciales...');
                    this.sendCommand('GET_DHT');
                }, 1000);
            };
            
            // Evento cuando la conexión se cierra
            this.websocket.onclose = (evt) => {
                console.log('❌ WebSocket DESCONECTADO:', evt);
                this.updateConnectionStatus(false);
                this.handleReconnection();  // Intentar reconectar automáticamente
            };
            
            // Evento cuando se recibe un mensaje del servidor
            this.websocket.onmessage = (evt) => {
                console.log('📨 Mensaje recibido del ESP32:', evt.data);
                this.handleMessage(evt.data);
            };
            
            // Evento cuando ocurre un error en la conexión
            this.websocket.onerror = (evt) => {
                console.error('💥 Error en WebSocket:', evt);
                this.updateConnectionStatus(false);
            };
            
        } catch (error) {
            console.error('💥 Error al crear WebSocket:', error);
            this.handleReconnection();
        }
    }

    /**
     * Actualiza el estado de conexión en la interfaz
     * @param {boolean} connected - Estado de la conexión
     */
    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connectionStatus');
        const buttons = document.querySelectorAll('.btn');
        
        if (connected) {
            statusElement.textContent = 'Conectado ✅';
            statusElement.className = 'status connected';
            // Habilitar todos los botones
            buttons.forEach(btn => {
                btn.disabled = false;
                btn.style.opacity = '1';
            });
        } else {
            statusElement.textContent = 'Desconectado ❌';
            statusElement.className = 'status disconnected';
            // Deshabilitar todos los botones
            buttons.forEach(btn => {
                btn.disabled = true;
                btn.style.opacity = '0.6';
            });
        }
    }

    /**
     * Procesa los mensajes recibidos del ESP32
     * @param {string} message - Mensaje recibido del servidor
     */
    handleMessage(message) {
        console.log('📨 Mensaje RAW recibido:', message, 'Tipo:', typeof message);
        
        // Validar que el mensaje sea string
        if (typeof message !== 'string') {
            console.error('❌ Mensaje no es string:', message);
            return;
        }
        
        // Procesar mensajes de estado del LED
        if (message.startsWith('LED:')) {
            const estado = message.split(':')[1];
            console.log('💡 Estado LED:', estado);
            this.updateLEDStatus(estado);
        } 
        // Procesar mensajes de datos DHT11
        else if (message.startsWith('DHT:')) {
            console.log('🌡️ Mensaje DHT detectado');
            
            const parts = message.split(':');
            console.log('🔍 Partes del split:', parts, 'Número de partes:', parts.length);
            
            // Validar formato del mensaje DHT
            if (parts.length >= 3) {
                const tempStr = parts[1];
                const humStr = parts[2];
                
                console.log('📝 Temp string:', tempStr, 'Hum string:', humStr);
                
                // Convertir strings a números
                const temperature = parseFloat(tempStr);
                const humidity = parseFloat(humStr);
                
                console.log('🔢 Temp parsed:', temperature, 'Hum parsed:', humidity);
                
                // Validar que los valores sean números válidos
                if (!isNaN(temperature) && !isNaN(humidity)) {
                    console.log('✅ Datos válidos, actualizando interfaz');
                    this.updateDHTData(temperature, humidity);
                } else {
                    console.error('❌ Error parseando números');
                }
            } else {
                console.error('❌ Formato DHT incorrecto. Se esperaban 3 partes');
            }
        } else {
            console.log('📝 Otro mensaje:', message);
        }
    }

    /**
     * Actualiza los datos del sensor DHT11 en la interfaz
     * @param {number} temperature - Temperatura en °C
     * @param {number} humidity - Humedad en %
     */
    updateDHTData(temperature, humidity) {
        const tempElement = document.getElementById('temperatureValue');
        const humElement = document.getElementById('humidityValue');
        const updateElement = document.getElementById('lastUpdate');
        
        // Verificar que todos los elementos existan
        if (tempElement && humElement && updateElement) {
            // Actualizar valores numéricos
            tempElement.textContent = `${temperature.toFixed(1)} °C`;
            humElement.textContent = `${humidity.toFixed(1)} %`;
            
            // Colores dinámicos según los valores
            tempElement.style.color = temperature > 30 ? '#ff6b6b' :  // Rojo para calor
                                    temperature < 15 ? '#4dabf7' :   // Azul para frío
                                    '#51cf66';                       // Verde para temperatura normal
            
            humElement.style.color = humidity > 80 ? '#4dabf7' :     // Azul para humedad alta
                                   humidity < 30 ? '#ff922b' :      // Naranja para humedad baja
                                   '#51cf66';                       // Verde para humedad normal
            
            // Actualizar timestamp de última actualización
            const now = new Date();
            updateElement.textContent = now.toLocaleTimeString();
            
            console.log('✅ Datos DHT11 actualizados en interfaz');
        } else {
            console.error('❌ No se encontraron todos los elementos DHT en la interfaz');
        }
    }

    /**
     * Inicia la actualización automática de datos DHT11
     */
    startAutoRefresh() {
        this.stopAutoRefresh(); // Limpiar intervalo existente
        
        this.isAutoRefresh = true;
        this.autoRefreshInterval = setInterval(() => {
            console.log('🔄 Actualización automática de datos DHT11');
            this.sendCommand('GET_DHT');
        }, this.autoRefreshTime);
        
        // Actualizar estado del botón en la interfaz
        const btn = document.getElementById('btnAutoRefresh');
        if (btn) {
            btn.textContent = `⏰ Auto: ON (${this.autoRefreshTime/1000}s)`;
            btn.classList.add('active');
        }
        
        console.log('✅ Auto-refresh iniciado');
    }

    /**
     * Detiene la actualización automática de datos DHT11
     */
    stopAutoRefresh() {
        if (this.autoRefreshInterval) {
            clearInterval(this.autoRefreshInterval);
            this.autoRefreshInterval = null;
        }
        this.isAutoRefresh = false;
        
        // Actualizar estado del botón en la interfaz
        const btn = document.getElementById('btnAutoRefresh');
        if (btn) {
            btn.textContent = '⏰ Auto: OFF';
            btn.classList.remove('active');
        }
        
        console.log('🛑 Auto-refresh detenido');
    }

    /**
     * Alterna el estado de auto-actualización
     */
    toggleAutoRefresh() {
        if (this.isAutoRefresh) {
            this.stopAutoRefresh();
        } else {
            this.startAutoRefresh();
        }
    }

    /**
     * Actualiza el estado del LED en la interfaz
     * @param {string} estado - Estado del LED (ENCENDIDO/APAGADO)
     */
    updateLEDStatus(estado) {
        const ledStatusElement = document.getElementById('ledStatus');
        if (ledStatusElement) {
            ledStatusElement.textContent = `LED: ${estado}`;
            
            // Aplicar estilos según el estado
            if (estado === 'ENCENDIDO') {
                ledStatusElement.className = 'led-status led-on';
                ledStatusElement.innerHTML = '💡 LED: ENCENDIDO';
            } else {
                ledStatusElement.className = 'led-status led-off';
                ledStatusElement.innerHTML = '⚫ LED: APAGADO';
            }
            
            console.log('🎯 Estado actualizado en la interfaz:', estado);
        }
    }

    /**
     * Maneja la reconexión automática cuando se pierde la conexión
     */
    handleReconnection() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`🔄 Intentando reconectar... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
            
            // Intentar reconectar después del intervalo configurado
            setTimeout(() => {
                this.connectWebSocket();
            }, this.reconnectInterval);
        } else {
            console.error('❌ Máximo número de intentos de reconexión alcanzado');
            alert('No se pudo conectar al ESP32. Por favor, recarga la página.');
        }
    }

    /**
     * Envía un comando al ESP32 via WebSocket
     * @param {string} command - Comando a enviar
     */
    sendCommand(command) {
        console.log('📤 Intentando enviar comando:', command);
        
        // Verificar que WebSocket esté conectado y listo
        if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            console.log('✅ WebSocket listo, enviando:', command);
            this.websocket.send(command);
            console.log('✅ Comando enviado correctamente:', command);
        } else {
            console.warn('⚠️ WebSocket no conectado. Estado:', 
                        this.websocket ? this.websocket.readyState : 'no inicializado');
            this.updateConnectionStatus(false);
            alert('WebSocket no conectado. Intentando reconectar...');
            this.connectWebSocket();  // Intentar reconectar automáticamente
        }
    }
}

// ============================================================================
// FUNCIONES GLOBALES PARA USO DESDE HTML
// ============================================================================

/**
 * Función global para enviar comandos desde botones HTML
 * @param {string} cmd - Comando a enviar
 */
function sendCommand(cmd) {
    console.log('🎯 Función sendCommand llamada con:', cmd);
    if (window.wsController) {
        window.wsController.sendCommand(cmd);
    } else {
        console.error('❌ Controlador WebSocket no disponible en sendCommand');
        alert('Controlador no inicializado. Por favor, recarga la página.');
    }
}

/**
 * Función global para solicitar datos del sensor DHT11
 */
function requestDHTData() {
    console.log('🌡️ Solicitando datos DHT11...');
    sendCommand('GET_DHT');
}

/**
 * Función global para alternar auto-actualización DHT11
 */
function toggleAutoRefresh() {
    if (window.wsController) {
        window.wsController.toggleAutoRefresh();
    } else {
        console.error('❌ Controlador WebSocket no disponible');
    }
}

// ============================================================================
// INICIALIZACIÓN DE LA APLICACIÓN
// ============================================================================

let wsController;  // Instancia global del controlador WebSocket

/**
 * Inicializa la aplicación cuando el DOM está completamente cargado
 */
document.addEventListener('DOMContentLoaded', function() {
    console.log('🚀 Inicializando aplicación...');
    wsController = new WebSocketController();
    window.wsController = wsController;  // Hacer disponible globalmente
});