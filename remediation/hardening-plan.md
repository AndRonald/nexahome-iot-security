# Plano de Hardening — NexaSense (versão comercial)

O plano é organizado por prioridade. Medidas irreversíveis (eFuses) são **propostas conceitualmente** e não foram executadas, como permite o enunciado.

## Prioridade 1 — antes do piloto com clientes

| # | Medida | Trata | Como (ESP32 / ESP-IDF) | Referência |
|---|---|---|---|---|
| H1 | **Remover debug do build de produção** e nunca imprimir segredos | F01, F07 | Build `release` com `CORE_DEBUG_LEVEL=0` / `CONFIG_LOG_DEFAULT_LEVEL_NONE`; `CONFIG_BOOTLOADER_LOG_LEVEL` mínimo; remover `printConfig()` | ETSI 5.6 |
| H2 | **Remover a interface de comandos** (`help`/`config`/`reboot`) da serial e do MQTT | F02 | Compilar só em builds de desenvolvimento (`#ifdef DEV_BUILD`); comandos remotos legítimos devem ser assinados pela plataforma | ETSI 5.6, NIST *Logical Access to Interfaces* |
| H3 | **Comunicação protegida**: MQTT sobre TLS com autenticação mútua | F04 | Porta 8883; validar o certificado do broker (CA fixada); certificado de cliente **único por dispositivo** | ETSI 5.5, NIST *Data Protection* |
| H4 | **Broker próprio com ACL** por dispositivo | F02, F04 | Cada dispositivo só publica em `.../<id>/telemetry` e só assina `.../<id>/cmd`; só a plataforma publica em `cmd` | ETSI 5.5 |
| H5 | **Credencial individual por dispositivo**, fora do código-fonte | F03 | Provisionar na fábrica (chave/certificado por unidade) e guardar em partição **NVS cifrada**; remover o token do payload | ETSI 5.4, NIST *Device Identification* |

## Prioridade 2 — antes da produção em série

| # | Medida | Trata | Como | Referência |
|---|---|---|---|---|
| H6 | **Flash Encryption** em modo *release* | F03, F06 | Cifra o conteúdo da flash com chave no eFuse; despejo via `esptool` passa a sair ilegível | ETSI 5.4, NIST *Data Protection* |
| H7 | **Secure Boot V2** (assinatura RSA-PSS) | F05, F06 | Bootloader só executa aplicações assinadas pela chave privada da NexaHome (chave mantida em HSM, nunca no repositório). Requer ESP32 revisão v3 ou superior | ETSI 5.7 |
| H8 | **Desabilitar JTAG** e ativar o **modo download seguro** | F06 | eFuses `JTAG_DISABLE`; `CONFIG_SECURE_UART_ROM_DL_MODE` (download mode só aceita comandos limitados, sem leitura de flash) | ETSI 5.6, EMB3D TID-115 / TID-119 |
| H9 | **Processo seguro de atualização** | F05 | `esp_https_ota` com certificado fixado; imagem assinada (H7); verificação de versão (anti-rollback) | ETSI 5.3, NIST *Software Update* |
| H10 | **Recuperação em caso de falha** de atualização | F05 | Partições OTA duplas (`ota_0`/`ota_1`) + `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`: se a nova imagem não se validar, volta para a anterior | ETSI 5.3 |

> **Secure Boot e Flash Encryption são complementares.** O Secure Boot garante que só firmware **autêntico** executa (integridade). A Flash Encryption garante que o conteúdo da flash **não seja lido** (confidencialidade). Um sem o outro deixa uma lacuna: com Secure Boot sem cifragem, os segredos são lidos; com cifragem sem Secure Boot, um firmware malicioso pode ser gravado e executado.

## Prioridade 3 — robustez e operação

| # | Medida | Referência |
|---|---|---|
| H11 | Mover o DHT22 do GPIO15 (pino de strapping/JTAG) para um GPIO comum | Boas práticas de hardware |
| H12 | Usar o chip em revisão ≥ v3, com contramedidas na ROM contra *fault injection* (CVE-2019-17391), e **chaves únicas por dispositivo** | Advisory da Espressif |
| H13 | Proteção física: não expor os pinos UART/JTAG no produto final (sem header, pads cobertos, invólucro com lacre) | ETSI 5.6 |
| H14 | Consciência de estado: reportar versão de firmware e falhas de boot/atualização para a plataforma | NIST *Cybersecurity State Awareness* |
| H15 | Política de divulgação de vulnerabilidades e suporte de atualizações publicados | ETSI 5.2 |

## Roteiro resumido

```
Protótipo (hoje)                 Piloto                          Produção
─────────────────               ─────────────────               ─────────────────
debug + comandos   ──H1/H2──►   build release limpo   ──H6/H7─►  flash cifrada
token fixo         ──H5─────►   credencial por unidade ──H8───►  JTAG off / DL seguro
MQTT 1883 aberto   ──H3/H4──►   MQTT TLS + ACL         ──H9/10►  OTA assinada + rollback
```

## O que foi implementado nesta atividade

Nada foi gravado em eFuse. As medidas H1–H5 podem ser validadas no próprio Wokwi (build sem debug, MQTT na porta 8883 com TLS). As medidas H6–H10 são descritas conforme a documentação de segurança do ESP-IDF e ficam como proposta para a versão comercial.
