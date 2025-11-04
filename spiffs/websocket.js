class WebSocketController {
    constructor() {
        this.websocket = null;
        this.reconnectInterval = 3000;
        this.maxReconnectAttempts = 5;
        this.reconnectAttempts = 0;
        
        console.log('🔄 Inicializando controlador WebSocket...');
        this.initializeEventListeners();
        this.connectWebSocket();
    }

    initializeEventListeners() {
        // Agregar event listeners a los botones por si acaso
        document.addEventListener('DOMContentLoaded', () => {
            const btnOn = document.getElementById('btnOn');
            const btnOff = document.getElementById('btnOff');
            const btnToggle = document.getElementById('btnToggle');
            
            if (btnOn) btnOn.addEventListener('click', () => this.sendCommand('ON'));
            if (btnOff) btnOff.addEventListener('click', () => this.sendCommand('OFF'));
            if (btnToggle) btnToggle.addEventListener('click', () => this.sendCommand('TOGGLE'));
            
            console.log('✅ Event listeners de botones configurados');
        });

        window.addEventListener('beforeunload', () => {
            if (this.websocket) {
                this.websocket.close();
            }
        });
    }

    connectWebSocket() {
        try {
            const host = window.location.host;
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${host}/ws`;
            
            console.log('🔗 Conectando WebSocket a:', wsUrl);
            this.websocket = new WebSocket(wsUrl);
            
            this.websocket.onopen = (evt) => {
                console.log('✅ WebSocket CONECTADO correctamente');
                this.updateConnectionStatus(true);
                this.reconnectAttempts = 0;
                
                // Solicitar estado actual después de 1 segundo
                setTimeout(() => {
                    console.log('📋 Solicitando estado inicial...');
                    this.sendCommand('STATUS');
                }, 1000);
            };
            
            this.websocket.onclose = (evt) => {
                console.log('❌ WebSocket DESCONECTADO:', evt);
                this.updateConnectionStatus(false);
                this.handleReconnection();
            };
            
            this.websocket.onmessage = (evt) => {
                console.log('📨 Mensaje recibido del ESP32:', evt.data);
                this.handleMessage(evt.data);
            };
            
            this.websocket.onerror = (evt) => {
                console.error('💥 Error en WebSocket:', evt);
                this.updateConnectionStatus(false);
            };
            
        } catch (error) {
            console.error('💥 Error al crear WebSocket:', error);
            this.handleReconnection();
        }
    }

    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connectionStatus');
        const buttons = document.querySelectorAll('.btn');
        
        if (connected) {
            statusElement.textContent = 'Conectado ✅';
            statusElement.className = 'status connected';
            buttons.forEach(btn => {
                btn.disabled = false;
                btn.style.opacity = '1';
            });
        } else {
            statusElement.textContent = 'Desconectado ❌';
            statusElement.className = 'status disconnected';
            buttons.forEach(btn => {
                btn.disabled = true;
                btn.style.opacity = '0.6';
            });
        }
    }

    handleMessage(message) {
        console.log('🔄 Procesando mensaje:', message);
        if (message.startsWith('LED:')) {
            const estado = message.split(':')[1];
            console.log('💡 Estado del LED recibido:', estado);
            this.updateLEDStatus(estado);
        } else {
            console.log('📝 Mensaje recibido:', message);
        }
    }

    updateLEDStatus(estado) {
        const ledStatusElement = document.getElementById('ledStatus');
        if (ledStatusElement) {
            ledStatusElement.textContent = `LED: ${estado}`;
            
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

    handleReconnection() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`🔄 Intentando reconectar... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
            
            setTimeout(() => {
                this.connectWebSocket();
            }, this.reconnectInterval);
        } else {
            console.error('❌ Máximo número de intentos de reconexión alcanzado');
            alert('No se pudo conectar al ESP32. Recarga la página.');
        }
    }

    sendCommand(command) {
        console.log('📤 Intentando enviar comando:', command);
        
        if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            console.log('✅ WebSocket listo, enviando:', command);
            this.websocket.send(command);
            console.log('✅ Comando enviado correctamente:', command);
        } else {
            console.warn('⚠️ WebSocket no conectado. Estado:', this.websocket ? this.websocket.readyState : 'no inicializado');
            this.updateConnectionStatus(false);
            alert('WebSocket no conectado. Intentando reconectar...');
            this.connectWebSocket();
        }
    }
}

// Inicializar cuando se carga la página
let wsController;

document.addEventListener('DOMContentLoaded', function() {
    console.log('🚀 Inicializando aplicación...');
    wsController = new WebSocketController();
    window.wsController = wsController; // Hacerlo global
});

// Función global para los botones HTML
function sendCommand(cmd) {
    console.log('🎯 Función sendCommand llamada con:', cmd);
    if (window.wsController) {
        window.wsController.sendCommand(cmd);
    } else {
        console.error('❌ Controlador WebSocket no disponible en sendCommand');
        alert('Controlador no inicializado. Recarga la página.');
    }
}