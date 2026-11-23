# Análise do Firmware — NexaSense v0.9.3-beta

## 1. Identificação da imagem

| Item | Valor |
|---|---|
| Arquivo | `nexasense-v0.9.3.bin` |
| Tamanho | 902.000 bytes |
| Origem | Compilado no simulador Wokwi (ESP32 + Arduino core) |
| SHA-256 | `c796e03c0dac45bcee3885db8486acdbf9f7581630bc5ed8b80ed670e1d8cf36` |
| Cópia analisada | `firmware/work/nexasense-work.bin` (mesmo hash) |

Detalhes de preservação em [`hashes.txt`](hashes.txt). Toda a análise foi feita na cópia de trabalho; o original não foi alterado.

## 2. Metodologia

Análise estática básica, sem descompilação, buscando texto legível dentro do binário com `grep -a` (trata o binário como texto):

```bash
grep -a -o -E "nxh_live_[A-Za-z0-9]+" firmware/work/nexasense-work.bin
grep -a -o -E "https?://[[:print:]]+" firmware/work/nexasense-work.bin
grep -a -o -E "[a-z0-9.-]+\.(com|org|local)" firmware/work/nexasense-work.bin | sort -u
grep -a -o -E "nexahome-lab/[[:print:]]+|\[DEBUG\][[:print:]]*" firmware/work/nexasense-work.bin | sort -u
```

## 3. Informações encontradas

### 3.1 Token de API em texto puro
```
nxh_live_9f8e7d6c5b4a3210
```
**Relevância:** o segredo está gravado no binário sem nenhuma ofuscação ou cifragem. Quem obtiver o firmware (download de atualização, leitura da flash via UART/JTAG) obtém o token. Como o mesmo firmware vai para todos os dispositivos, **o token é compartilhado por toda a frota** — vazar um equivale a vazar todos.
*(Valor fictício, criado para o ambiente educacional.)*

### 3.2 Endpoint de atualização (OTA) via HTTP
```
http://update.nexahome.local/fw/nexasense.bin
```
**Relevância:** a atualização usa HTTP, sem TLS. Um atacante na mesma rede pode interceptar ou substituir o arquivo baixado (man-in-the-middle). Não há referência a verificação de assinatura no firmware, então uma imagem adulterada poderia ser aceita.

### 3.3 Domínios e brokers
```
broker.hivemq.com
update.nexahome.local
```
**Relevância:** revela a infraestrutura com a qual o dispositivo conversa. Ajuda o atacante a mapear a superfície de ataque e a saber onde procurar os dados do dispositivo.

### 3.4 Tópicos MQTT
```
nexahome-lab/NXS-000123/cmd
nexahome-lab/NXS-000123/telemetry
```
**Relevância:** expõe a estrutura de tópicos, inclusive o tópico de **comando**. Junto com a ausência de autenticação no broker (ver `network/`), permite ler a telemetria e enviar comandos ao dispositivo.

### 3.5 Mensagens de depuração
```
[DEBUG] ===== CONFIG =====
[DEBUG] API token: %s
[DEBUG] WiFi: %s / senha: '%s'
[DEBUG] MQTT: %s:%d
[DEBUG] OTA: %s
[DEBUG] comandos: help, config, reboot
...
```
**Relevância:** confirma que o firmware de produção carrega código de debug que imprime segredos (token, senha do Wi-Fi) e expõe uma interface de comandos (`help`, `config`, `reboot`) pela serial e pelo MQTT. Essas strings comprovam que o vazamento observado na UART (`hardware/evidence/02` e `03`) vem do próprio firmware, e não de um ajuste do ambiente de teste.

## 4. Resumo

| # | Informação | Onde | Risco |
|---|---|---|---|
| 1 | Token de API fixo | binário | Alto — credencial compartilhada pela frota |
| 2 | OTA via HTTP, sem assinatura | binário | Alto — atualização pode ser adulterada |
| 3 | Domínios / broker | binário | Baixo — reconhecimento |
| 4 | Tópicos MQTT (incl. `cmd`) | binário | Médio — combinado com broker sem auth |
| 5 | Debug que imprime segredos | binário | Alto — vazamento via UART |

## 5. Limitações

- Não foi feita análise de partições com `esptool.py image_info` nem descompilação.
- O firmware é educacional, com fragilidades inseridas de propósito; os valores sensíveis são fictícios.
- Os achados detalhados estão em [`findings/findings.md`](../findings/findings.md).
