# Achados (Findings) — NexaSense v0.9.3-beta

Cada achado segue a cadeia **Evidência → Fragilidade → Ameaça → Impacto → Risco → Mitigação**.

**Critério de classificação**

- **Probabilidade:** considera o acesso que o atacante precisa (rede/remoto > mesma rede > físico com ferramenta simples > físico com equipamento especializado) e o esforço técnico.
- **Impacto:** considera o que se perde (segredos da frota, controle do dispositivo, integridade do firmware) e quantos dispositivos são afetados.
- **Risco:** combinação das duas (matriz 3×3: alto+alto = alto; alto+média = alto; média+média = médio; qualquer + baixa = no máximo médio).

## Tabela resumida

| Achado | Evidência | Impacto | Probabilidade | Risco | Recomendação |
|---|---|---|---|---|---|
| **F01** — Debug na UART imprime segredos no boot | `hardware/evidence/02-config-vazada-no-boot.png`; strings `[DEBUG] API token: %s` no binário | Vazamento do token (válido para toda a frota) e da topologia | Média (acesso físico + cabo USB-serial) | **Alto** | Remover logs de debug do build de produção; nunca imprimir segredos |
| **F02** — Interface de comandos sem autenticação (serial e MQTT) | `hardware/evidence/03-comando-debug-sem-auth.png`; função `onMessage()` chama `runCommand()` sem verificação | Leitura da config e `reboot` remoto (negação de serviço) | Alta (tópico `cmd` em broker aberto) | **Alto** | Remover comandos em produção; exigir autenticação e autorização por dispositivo |
| **F03** — Token de API fixo e em texto puro no firmware | `firmware/analysis-notes.md` §3.1 (`grep` encontrou `nxh_live_...`) | Token único para toda a frota; vazar um = vazar todos | Média (firmware obtido por UART/flash/OTA) | **Alto** | Credencial única por dispositivo, provisionada na fábrica e guardada em NVS cifrada |
| **F04** — MQTT sem TLS e broker sem autenticação | `firmware/src/nexasense.ino` (porta 1883, `mqtt.connect(DEVICE_ID)` sem usuário/senha); `network/evidence/01-mqtt-enviado.png`; `network/evidence/02-teste-broker.png` | Leitura da telemetria e do token por terceiros; injeção de comandos | Alta (qualquer cliente MQTT na internet) | **Alto** | MQTT sobre TLS (8883) com autenticação mútua por certificado; ACL por tópico |
| **F05** — Atualização OTA via HTTP, sem assinatura | `firmware/analysis-notes.md` §3.2 (`http://update.nexahome.local/...`) | Instalação de firmware malicioso (comprometimento total) | Baixa-Média (atacante na mesma rede, MITM) | **Médio** | HTTPS com validação de certificado + Secure Boot V2 / imagem assinada + rollback |
| **F06** — Flash sem cifragem e sem Secure Boot | Boot log (`hardware/evidence/01-boot-log-uart.png`) sem mensagens de Secure Boot/Flash Encryption; strings legíveis no binário | Extração do firmware e dos segredos; gravação de firmware alterado | Média (acesso físico, `esptool` em modo download) | **Alto** | Habilitar Flash Encryption (release) e Secure Boot V2 na produção |
| **F07** — Boot log da ROM exposto na UART | `hardware/evidence/01-boot-log-uart.png` (`rst`, `boot:0x13`, endereços `load:0x...`, `entry 0x400805dc`) | Informação de reconhecimento (modo de boot, layout de memória) | Alta (basta ler a serial) | **Baixo** | Reduzir log do bootloader; silenciar log da ROM quando suportado |

---

## F01 — Debug na UART imprime segredos no boot

- **Evidência:** ao ligar, a serial exibe `[DEBUG] API token: nxh_live_9f8e7d6c5b4a3210`, a URL de OTA, o broker e o SSID/senha (print `02-config-vazada-no-boot.png`). As mesmas strings de formato estão no binário (`analysis-notes.md` §3.5), o que prova que o comportamento é do firmware.
- **Fragilidade:** o build de produção mantém o código de diagnóstico, que imprime parâmetros sensíveis.
- **Ameaça:** um atacante com acesso físico conecta um adaptador USB-serial (ou o próprio USB da placa) nos pinos TX/RX e lê os segredos durante o boot.
- **Impacto:** o token é o mesmo em todos os dispositivos (ver F03), então um único aparelho comprometido expõe a frota inteira.
- **Risco:** **Alto** (impacto alto × probabilidade média).
- **Mitigação:** compilar a produção sem logs de debug (`CORE_DEBUG_LEVEL=0`, macros de log removidas); nunca enviar segredos para a serial, nem em debug.

## F02 — Interface de comandos sem autenticação

- **Evidência:** ao digitar `config` na serial, o dispositivo devolve toda a configuração sem pedir credencial (`03-comando-debug-sem-auth.png`). No código, `onMessage()` repassa qualquer mensagem do tópico `.../cmd` para `runCommand()`, que aceita `config` e `reboot`.
- **Fragilidade:** canal de controle sem autenticação nem autorização, tanto local (UART) quanto remoto (MQTT).
- **Ameaça:** qualquer cliente conectado ao broker publica `reboot` em `nexahome-lab/NXS-000123/cmd` e reinicia o dispositivo em loop (negação de serviço), ou publica `config` para forçar o vazamento na serial.
- **Impacto:** perda de disponibilidade do monitoramento e exposição de configuração.
- **Risco:** **Alto**.
- **Limitação:** no ambiente Wokwi, a mensagem publicada pelo cliente web da HiveMQ não chegou ao dispositivo simulado (ver `network/protocol-analysis.md`). A parte remota está comprovada pelo código e pelo fato de o broker aceitar publicação anônima (`02-teste-broker.png`); a parte local foi demonstrada na prática.
- **Mitigação:** remover a interface de comandos em produção; se for necessária, exigir comandos assinados ou autenticação por dispositivo e ACL que só permita à plataforma publicar em `cmd`.

## F03 — Token de API fixo e em texto puro

- **Evidência:** `grep -a "nxh_live_..."` encontrou o token no binário (`analysis-notes.md` §3.1).
- **Fragilidade:** credencial "hard-coded" no código-fonte e igual para todos os dispositivos.
- **Ameaça:** quem obtiver o firmware (UART, leitura da flash, interceptação da OTA) extrai o token com um simples `strings`/`grep`.
- **Impacto:** acesso à API com a identidade de qualquer dispositivo; revogar o token exige atualizar a frota inteira.
- **Risco:** **Alto**.
- **Mitigação:** credencial única por dispositivo, gerada no provisionamento e guardada em partição NVS cifrada (com Flash Encryption ligada); nunca no código-fonte.

## F04 — MQTT sem TLS e broker sem autenticação

- **Evidência:** `MQTT_PORT = 1883` (MQTT em texto puro) e `mqtt.connect(DEVICE_ID)` sem usuário/senha nem certificado. O payload enviado contém o token (`01-mqtt-enviado.png`). O teste no broker mostrou que um cliente anônimo publica e assina no tópico `nexahome-lab/#` (`02-teste-broker.png`).
- **Fragilidade:** sem confidencialidade (texto puro), sem autenticação do dispositivo e sem autenticação/ACL no broker.
- **Ameaça:** leitura da telemetria e do token por qualquer assinante; injeção de telemetria falsa ou de comandos; interceptação na rede local.
- **Impacto:** perda de confidencialidade e integridade dos dados de monitoramento; vazamento de credencial.
- **Risco:** **Alto**.
- **Mitigação:** MQTT sobre TLS (porta 8883) com validação do certificado do servidor e autenticação mútua (certificado de cliente por dispositivo); broker próprio com ACL por tópico; remover o token do payload.

## F05 — Atualização OTA via HTTP, sem assinatura

- **Evidência:** URL `http://update.nexahome.local/fw/nexasense.bin` no binário; nenhuma referência a verificação de assinatura.
- **Fragilidade:** canal de atualização sem TLS e sem verificação de autenticidade da imagem.
- **Ameaça:** atacante na mesma rede (ex.: ARP/DNS spoofing) entrega uma imagem modificada.
- **Impacto:** comprometimento completo e persistente do dispositivo.
- **Risco:** **Médio** — o impacto é alto, mas a rotina de OTA não está implementada neste protótipo e o ataque exige posição de MITM.
- **Mitigação:** `esp_https_ota` com certificado fixado; Secure Boot V2 para que só imagens assinadas executem; rollback automático e anti-rollback de versão.

## F06 — Flash sem cifragem e sem Secure Boot

- **Evidência:** o boot log não mostra mensagens de Secure Boot ou Flash Encryption; o conteúdo do firmware é legível em texto (F03). Em um ESP32 com configuração padrão, o modo download (botão BOOT + EN) permite `esptool.py read_flash` e gravação livre.
- **Fragilidade:** a configuração padrão de desenvolvimento foi mantida.
- **Ameaça:** extração completa da flash (firmware + NVS com credenciais) e gravação de firmware alterado com acesso físico de poucos minutos.
- **Impacto:** perda de segredos e da integridade do software.
- **Risco:** **Alto**.
- **Mitigação:** Flash Encryption em modo release, Secure Boot V2, desabilitar JTAG e o modo download inseguro via eFuse (proposta conceitual — não executada, por ser irreversível).

## F07 — Boot log da ROM exposto

- **Evidência:** `rst:0x1 (POWERON_RESET), boot:0x13 (SPI_FAST_FLASH_BOOT)`, `mode:DIO`, endereços `load:0x3fff0030`, `entry 0x400805dc` (`01-boot-log-uart.png`).
- **Fragilidade:** a ROM e o bootloader imprimem informações internas na UART.
- **Ameaça:** reconhecimento — o atacante descobre o modo de boot e o layout de carregamento.
- **Impacto:** baixo isoladamente; não expõe segredos.
- **Risco:** **Baixo**. A interface UART, por si só, não é a vulnerabilidade; o problema real é o que o firmware envia por ela (F01, F02).
- **Mitigação:** `CONFIG_BOOTLOADER_LOG_LEVEL` em nível mínimo; silenciar o log da ROM por eFuse ou strapping quando o chip suportar.

---

## Relação com as referências

| Achado | ETSI EN 303 645 | NIST IR 8259A | OWASP ISTG | MITRE EMB3D |
|---|---|---|---|---|
| F01, F07 | 5.6 — minimizar superfícies expostas (interfaces de debug) | Logical Access to Interfaces | Interfaces físicas / Firmware | **TID-115** Firmware/Data Extraction via Hardware Interface |
| F02 | 5.6 — minimizar superfícies expostas; 5.5 — comunicação segura | Logical Access to Interfaces | Interfaces de comunicação de dados | **TID-310** Remotely Accessible Unauthenticated Services; **TID-406** Unauthorized Messages or Connections |
| F03 | 5.4 — armazenar parâmetros sensíveis com segurança (sem credenciais fixas no código) | Data Protection; Device Identification | Firmware | **TID-328** Hardcoded Credentials |
| F04 | 5.5 — comunicar com segurança | Data Protection; Logical Access to Interfaces | Comunicação de dados / rede | **TID-408** Unencrypted Sensitive Data Communication; **TID-406** Unauthorized Messages or Connections |
| F05 | 5.3 — manter o software atualizado (atualização segura e autenticada) | Software Update | Firmware (mecanismo de atualização) | **TID-211** Device Allows Unauthenticated Firmware Installation; **TID-215** Unencrypted SW/FW Updates |
| F06 | 5.7 — garantir a integridade do software; 5.4 | Device Security; Data Protection | Interfaces físicas / Memória | **TID-115** Firmware/Data Extraction via Hardware Interface; **TID-119** Latent Hardware Debug Port Allows Memory/Code Manipulation; **TID-201** Inadequate Bootloader Protection and Verification |

> As cláusulas da ETSI e as capacidades do NIST estão referenciadas pelo título da seção. Identificadores do EMB3D conforme o catálogo em https://emb3d.mitre.org/threats/ (consultado em 22/11/2026).
