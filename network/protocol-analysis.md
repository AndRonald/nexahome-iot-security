# Análise de Comunicação — NexaSense

## 1. Visão geral

| Item | Valor observado |
|---|---|
| Rede | Wi-Fi 2.4 GHz `Wokwi-GUEST` (rede aberta do simulador) |
| IP do dispositivo | `10.10.0.2` (NAT do Wokwi) |
| Protocolo de aplicação | **MQTT 3.1.1** (biblioteca PubSubClient) |
| Destino | `broker.hivemq.com:1883` (broker público de testes) |
| Transporte | TCP **sem TLS** |
| Client ID | `NXS-000123` (igual ao ID do dispositivo) |
| Tópicos | `nexahome-lab/NXS-000123/telemetry` (publica a cada 5 s) e `nexahome-lab/NXS-000123/cmd` (assina) |
| Outro canal previsto | OTA via `http://update.nexahome.local/fw/nexasense.bin` (HTTP, presente no firmware, não exercitado) |

### Mensagem de telemetria

```json
{"id":"NXS-000123","token":"nxh_live_9f8e7d6c5b4a3210","temp":24.0,"hum":40.0}
```

(Evidência: `evidence/01-mqtt-enviado.png`. O token é fictício.)

## 2. Avaliação de segurança do protocolo

**Identificar o protocolo não basta.** Saber que o dispositivo usa MQTT diz apenas *como* os dados trafegam. A pergunta de segurança é se esse uso garante **autenticação, confidencialidade e integridade** no contexto do produto. O mesmo MQTT pode ser seguro (TLS 1.2+, certificado por dispositivo, ACL por tópico) ou totalmente aberto, como neste protótipo.

| Propriedade | Situação | Evidência |
|---|---|---|
| **Confidencialidade** | ❌ Ausente — porta 1883, texto puro; qualquer ponto da rede (Wi-Fi, provedor, broker) lê o conteúdo | `MQTT_PORT = 1883` no código |
| **Autenticação do dispositivo** | ❌ Ausente — `mqtt.connect(DEVICE_ID)` sem usuário, senha ou certificado | `firmware/src/nexasense.ino` |
| **Autenticação do servidor** | ❌ Ausente — sem TLS, o dispositivo não verifica se fala com o broker legítimo | — |
| **Autorização (ACL)** | ❌ Ausente — broker aceita publicação e assinatura anônimas em qualquer tópico | `evidence/02-teste-broker.png` |
| **Integridade** | ❌ Ausente — mensagens podem ser alteradas em trânsito ou forjadas por qualquer cliente | — |
| **Dados sensíveis no payload** | ❌ Token da API enviado em toda mensagem | `evidence/01-mqtt-enviado.png` |
| **Mensagens de controle** | ❌ Tópico `cmd` aceita `config` e `reboot` sem validação de origem | função `onMessage()` |
| **Exposição de identificadores** | ⚠️ ID do dispositivo no client ID, no tópico e no payload — facilita enumeração | — |

## 3. Testes realizados

### 3.1 Observação do tráfego pelo dispositivo
A serial do Wokwi mostrou o dispositivo conectando ao broker e publicando a telemetria com o token a cada 5 segundos (`01-mqtt-enviado.png`).

### 3.2 Acesso ao broker como terceiro não autenticado
Com o cliente web da HiveMQ (`hivemq.com/demos/websocket-client`), sem nenhuma credencial:

1. Conexão a `broker.hivemq.com` — **aceita sem autenticação**.
2. Assinatura de `nexahome-lab/#` — **aceita**.
3. Publicação de `help` em `nexahome-lab/NXS-000123/cmd` — **aceita**, e a mensagem foi entregue de volta ao próprio assinante.

Isso comprova que o broker usado pelo produto **não exige autenticação nem aplica ACL**: qualquer pessoa na internet pode ler e publicar nos tópicos do dispositivo (`02-teste-broker.png`).

### 3.3 Limitação encontrada
A telemetria do dispositivo simulado e o comando publicado pelo cliente web **não cruzaram** entre o Wokwi e o cliente HiveMQ, embora os dois lados reportassem conexão ao mesmo broker. A causa provável é a rede simulada do Wokwi (gateway `Wokwi-GUEST`). Também não foi possível capturar pacotes com o Wireshark no ambiente simulado. Por isso, a ausência de criptografia está fundamentada na configuração do firmware (porta 1883, sem TLS) e não em uma captura de rede.

**Com placa física**, o teste seria repetido com um broker Mosquitto local (`mosquitto -v`) e captura no Wireshark (filtro `mqtt`), mostrando o payload em texto puro no campo *Message* do pacote *Publish*.

## 4. Conclusão

O dispositivo usa MQTT de forma **insegura no contexto analisado**: não há confidencialidade, autenticação nem integridade, o payload carrega uma credencial e o canal de comando aceita ordens de qualquer origem. Achados relacionados: **F02** e **F04** (ver `findings/findings.md`).

## Evidências

| Arquivo | O que mostra |
|---|---|
| `evidence/01-mqtt-enviado.png` | Serial do dispositivo publicando o JSON com o token |
| `evidence/02-teste-broker.png` | Cliente anônimo assinando `nexahome-lab/#` e publicando no tópico `cmd` |
