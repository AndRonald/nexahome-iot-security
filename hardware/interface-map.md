# Mapa de Interfaces Físicas — NexaSense (ESP32 DevKitC)

**Ambiente:** a análise foi feita no simulador Wokwi, com a placa `board-esp32-devkit-c-v4` (módulo ESP32-WROOM-32). Por não haver placa física, os pinos foram mapeados a partir do pinout oficial da placa e do comportamento observado na serial simulada.

## Interfaces identificadas

| Interface | Pinos | Estado no protótipo | O que permite fazer | Risco em produção |
|---|---|---|---|---|
| **UART0** | GPIO1 (TX), GPIO3 (RX); ligada ao conector USB pela ponte USB-serial | **Ativa** — boot log, logs `[DEBUG]` e entrada de comandos | Ler tudo o que o firmware imprime; enviar comandos (`help`, `config`, `reboot`); com GPIO0 em nível baixo no reset, entrar no **modo download** da ROM | **Alto** — vazamento de segredos (F01), comandos sem autenticação (F02), leitura/gravação da flash (F06) |
| **Modo download (ROM)** | GPIO0 (BOOT) + EN (reset) via UART0 | **Habilitado** (padrão) | `esptool.py read_flash` (despejo completo da flash) e `write_flash` (gravar firmware arbitrário) | **Alto** — sem Flash Encryption, o despejo sai em texto puro |
| **JTAG** | GPIO12 (TDI), GPIO13 (TCK), GPIO14 (TMS), GPIO15 (TDO) | **Não desabilitado** por eFuse | Depuração completa: parar a CPU, ler/escrever RAM e registradores, ler a flash | **Alto** com acesso físico — controle total do dispositivo |
| **SPI (flash)** | GPIO6–11 (barramento interno da flash) | Interno ao módulo | Leitura direta do chip de flash com clipe/programador | **Médio** — exige dessoldar ou usar clipe; mitigado por Flash Encryption |
| **Sensor DHT22** | GPIO15 (protocolo 1-wire proprietário) | Ativo | Leitura de temperatura/umidade | **Baixo** — um atacante poderia injetar leituras falsas com acesso físico |
| **I²C** | — | Não utilizado neste protótipo | — | — |
| **Botões BOOT / EN** | GPIO0 / EN | Expostos na placa | Reset e entrada no modo download | Complementa o risco da UART |

### Observação sobre o GPIO15

O sensor está ligado no **GPIO15**, que também é o **TDO do JTAG** e um *pino de strapping*: em nível baixo no boot, ele silencia o log da ROM na UART. Em um produto, convém mover o sensor para um GPIO sem função especial, para evitar conflito com a depuração durante o desenvolvimento e comportamento inesperado no boot.

## Análise: uma interface acessível não é automaticamente uma vulnerabilidade

> "A interface **UART** permanece fisicamente acessível (pelo próprio USB da placa), **fornece** o token da API, a senha do Wi-Fi e a URL de atualização no boot, e **não possui** nenhuma autenticação para os comandos `config` e `reboot`. Considerando um atacante com **acesso físico por poucos minutos**, isso permite **extrair a credencial compartilhada da frota**, produzindo **impacto alto em todos os dispositivos**, e não só no aparelho atacado."

A UART em si é legítima: ela é necessária na fábrica e na manutenção. O risco vem de três coisas somadas:

1. o firmware **envia segredos** por ela;
2. ela **aceita comandos** sem autenticação;
3. o **modo download** continua aberto e a flash **não é cifrada**.

Com as mitigações do `remediation/hardening-plan.md` (logs removidos, comandos desativados, Flash Encryption + Secure Boot, modo download seguro), a mesma UART passa a expor só o boot log, que tem risco baixo (F07).

## Evidências

| Arquivo | O que mostra |
|---|---|
| `evidence/01-boot-log-uart.png` | Boot log da ROM: `rst:0x1`, `boot:0x13 (SPI_FAST_FLASH_BOOT)`, endereços de carga, `entry 0x400805dc` |
| `evidence/02-config-vazada-no-boot.png` | Bloco `[DEBUG] ===== CONFIG =====` com token, broker, OTA e Wi-Fi |
| `evidence/03-comando-debug-sem-auth.png` | **Demonstração controlada:** comando `config` digitado na serial devolve a configuração sem credencial |

## Limitações

- Sem placa física: não foi possível medir tensões, usar analisador lógico ou testar JTAG/`esptool` de verdade. O comportamento desses itens é descrito a partir da documentação da Espressif.
- O estado do eFuse (JTAG, modo download) corresponde ao padrão de fábrica de uma placa de desenvolvimento.
