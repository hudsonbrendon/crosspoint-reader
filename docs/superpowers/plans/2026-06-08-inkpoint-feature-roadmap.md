# inkpoint — Roadmap de Funcionalidades (pesquisado)

> Backlog pesquisado para o inkpoint, cruzando referências com o **KOReader** (o e-reader FOSS de referência / superconjunto de funcionalidades), tendências do mercado de e-readers, projetos de e-reader e-ink em ESP32 e os **pedidos de funcionalidade abertos do upstream CrossPoint**. Filtrado pela viabilidade no ESP32-C3 (~380 KB de RAM, single-core, e-ink em tons de cinza, armazenamento em SD, entrada por botões, RTC apenas no X3) e pelo `SCOPE.md` do projeto (leitor focado; rejeita jogos / conectividade ativa / áudio / anotação digitada).
>
> **Legenda** — Esforço **P/M/G**. Adequação: ✅ no escopo & viável · ⚠️ viável com ressalva · 🍴 rejeitado pelo upstream (diferencial do fork) · ⛔ impraticável neste HW. Links de fontes no rodapé.

## Já no inkpoint
Renderização de EPUB/XTC/TXT/BMP, hifenização, notas de rodapé, marcadores, ir-para-porcentagem, virada automática de página, leitura focada, orientação, temas, fontes no SD, remapeamento de botões, **estatísticas de leitura** (engine + telas, recém-construído), OPDS, WebDAV, Calibre sem fio, sincronização de progresso com KOReader, transferência de arquivos web + otimizador de EPUB, OTA (agora self-hosted), 24 idiomas + RTL, capturas de tela, virada de página por inclinação (X3).

---

## Tier 0 — Vitórias rápidas (pequeno, alta adequação)
| Funcionalidade | Fonte | Esforço | Adequação |
|---|---|---|---|
| KOReader DEVICE_NAME configurável | CP [#2272](https://github.com/crosspoint-reader/crosspoint-reader/issues/2272) | P | ✅ (está hardcoded — virar configuração) |
| Abrir livro em capítulo aleatório | CP [#2265](https://github.com/crosspoint-reader/crosspoint-reader/issues/2265) | P | ✅ |
| Número de página do livro na barra de status | CP [#1833](https://github.com/crosspoint-reader/crosspoint-reader/issues/1833) | P | ✅ |
| Toggle global de negrito | CP [#1174](https://github.com/crosspoint-reader/crosspoint-reader/issues/1174) | P–M | ✅ |
| Refresh completo antes de dormir (anti-ghost) | CP [#1334](https://github.com/crosspoint-reader/crosspoint-reader/issues/1334) | P | ✅ |
| Exibir relógio no leitor/sleep (RTC do X3; NTP no X4) | CP [#2276](https://github.com/crosspoint-reader/crosspoint-reader/issues/2276) | P–M | ✅ (X3) / ⚠️ (deriva no X4) |
| Backup / restauração de configurações no SD | CP [#1722](https://github.com/crosspoint-reader/crosspoint-reader/issues/1722) | P–M | ✅ |

## Tier 1 — Leitura de alto valor (paridade com KOReader)
| Funcionalidade | Fonte | Esforço | Adequação |
|---|---|---|---|
| **Consulta a dicionário** (StarDict offline no SD) | KOReader · roadmap CP/SCOPE Reference Tools | G | ✅ (a lacuna principal) |
| **Busca de texto completo no livro** | KOReader · CP [#1984](https://github.com/crosspoint-reader/crosspoint-reader/issues/1984) | G | ⚠️ (custo de RAM/varredura) |
| **Menu de configurações do leitor no livro** (fonte/margens/espaçamento, por livro) | KOReader · CP [#1678](https://github.com/crosspoint-reader/crosspoint-reader/issues/1678) | M | ✅ |
| **Destaques** (selecionar + marcar, arquivo sidecar, exportar) | KOReader | M | 🍴/⚠️ (SCOPE rejeita notas *digitadas*; destaques sem texto são o meio-termo) |
| **Timer de sessão de leitura ao vivo no leitor** | KOReader (read timer) · CP [#1416](https://github.com/crosspoint-reader/crosspoint-reader/issues/1416) | M | ✅ (estende nossa engine de stats) |
| **Modo noturno / invertido** | KOReader (night mode) | P–M | ✅ (inversão e-ink) |
| Atalhos de pressionar-e-segurar configuráveis | KOReader (keymapping) · CP [#2175](https://github.com/crosspoint-reader/crosspoint-reader/issues/2175) | M | ✅ |
| **Perfis** de configuração | KOReader (profiles) | M | ✅ |
| Construtor de vocabulário (depende do dicionário) | KOReader | M | ✅ (após dicionário) |

## Tier 2 — Renderização & navegação
| Funcionalidade | Fonte | Esforço | Adequação |
|---|---|---|---|
| Renderização de tabela `<table>` | CP [#876](https://github.com/crosspoint-reader/crosspoint-reader/issues/876) | G | ✅ |
| Tratamento de `<pre>` / bloco de código | CP [#953](https://github.com/crosspoint-reader/crosspoint-reader/issues/953), [#1519](https://github.com/crosspoint-reader/crosspoint-reader/issues/1519) | M | ✅ |
| Navegação por âncora / `noteref` | CP [#650](https://github.com/crosspoint-reader/crosspoint-reader/issues/650), [#1313](https://github.com/crosspoint-reader/crosspoint-reader/issues/1313) | M | ✅ |
| Visualizador de imagem + zoom/pan | KOReader (image viewer) · CP [#1387](https://github.com/crosspoint-reader/crosspoint-reader/issues/1387), [#1623](https://github.com/crosspoint-reader/crosspoint-reader/issues/1623) | M | ⚠️ (UX de pan no e-ink) |
| **Mapa do Livro / Navegador de Páginas** (navegação visual) | KOReader | G | ⚠️ (poderoso, pesado em RAM) |
| Biblioteca em grade de miniaturas de capa | KOReader (cover browser) · mercado | M | ✅ |
| Navegação de arquivos em árvore | CP [#1639](https://github.com/crosspoint-reader/crosspoint-reader/issues/1639) | M | ✅ |
| Formatos FB2 / CBZ-CBR (quadrinhos) | KOReader · forks | G | ⚠️ (CBZ = imagens, tons de cinza) |

## Tier 3 — Sincronização / transferência / conectividade / confiabilidade
| Funcionalidade | Fonte | Esforço | Adequação |
|---|---|---|---|
| Busca OPDS | CP [#2107](https://github.com/crosspoint-reader/crosspoint-reader/issues/2107) | M | ✅ |
| Download de série OPDS | CP [#2049](https://github.com/crosspoint-reader/crosspoint-reader/issues/2049) | M | ✅ |
| Exportar destaques (Markdown/Readwise/Calibre) | KOReader (highlight export) | M | ✅ (após destaques) |
| OTA robusto / "Safe Update" com recuperação | CP [#1915](https://github.com/crosspoint-reader/crosspoint-reader/issues/1915) | M | ✅ (relevante: OTA agora self-hosted) |
| WiFi WPA2-Enterprise | CP [#532](https://github.com/crosspoint-reader/crosspoint-reader/issues/532) | M | ⚠️ |
| PIN de desbloqueio | CP [#1933](https://github.com/crosspoint-reader/crosspoint-reader/issues/1933) | P–M | ✅ |
| Estatísticas de bateria / estimativa de autonomia | KOReader (battery stats) | P–M | ✅ |
| Upload de pastas / nomes longos (web) | CP [#1382](https://github.com/crosspoint-reader/crosspoint-reader/issues/1382), [#1170](https://github.com/crosspoint-reader/crosspoint-reader/issues/1170) | P–M | ✅ |

## Tier 4 — Diferenciais do inkpoint (fora do escopo upstream → sua vantagem)
*O `SCOPE.md` rejeita estes para a missão focada do CrossPoint; como fork, são onde o inkpoint pode se diferenciar — consciente de bateria/escopo.*
| Funcionalidade | Fonte | Esforço | Adequação |
|---|---|---|---|
| **Home Assistant / MQTT** (publica bateria, livro atual, stats de leitura, online; HA discovery) | seu pedido | G | 🍴 (tensão com "conectividade ativa" do SCOPE; design push-on-wake) |
| Ler-depois: artigos **Wallabag** / RSS → no dispositivo | KOReader (Wallabag) · CP [#271](https://github.com/crosspoint-reader/crosspoint-reader/issues/271) | G | 🍴 |
| **Consulta Wikipedia** (seleção → artigo) | KOReader | M | 🍴/⚠️ (precisa de WiFi-on-wake) |
| Enviar-para-dispositivo por email (watcher → download) | mercado/paridade Kindle | G | 🍴 |
| Traduzir seleção (Google Translate) | KOReader | M | 🍴/⚠️ (rede) |

## Não recomendado (HW/escopo)
- **Reflow de PDF** — layout fixo → pan/zoom no e-ink (SCOPE "tecnicamente não suportado"). ⛔
- **Jogos / bloco de notas / calculadora / áudio / TTS** — SCOPE rejeita firmemente; ESP32-C3 fraco demais para TTS. ⛔
- **Notas em cor / caneta** — Xteink é tons de cinza, apenas botão (sem toque/caneta). ⛔
- **Sequências diárias / calendário de leitura** — precisa de relógio de parede confiável; X4 não tem (esbarramos nisso com stats de leitura). ⛔ a menos que pareado com uma camada de relógio semeada por NTP.
- **Terminal / SSH / shell (KOReader tem)** — fora do escopo de um leitor focado. ⛔

---

## Sequenciamento recomendado
1. **Lote Tier 0** (DEVICE_NAME, capítulo aleatório, números de página, negrito, refresh-antes-de-dormir) — um primeiro PR rápido e satisfatório.
2. **Consulta a dicionário** (Tier 1) — a maior lacuna de paridade com KOReader e explicitamente no escopo. Funcionalidade âncora.
3. **Menu de configurações no livro + timer de sessão ao vivo** — alto valor de uso diário, construído sobre código/stats existentes.
4. Depois escolha um item de renderização do Tier 2 (tabelas ou âncoras) e/ou o diferencial **Home Assistant** se quiser que o inkpoint se destaque.

> Escolha qualquer item(ns) e eu escrevo um plano completo de implementação TDD (`writing-plans`) e executo.

## Fontes
- Lista de funcionalidades do KOReader — [wiki koreader/koreader: Features list](https://github.com/koreader/koreader/wiki/Features-list), [guia do usuário KOReader](https://koreader.rocks/user_guide/), [koreader.com](https://koreader.com/), [wiki MobileRead](https://wiki.mobileread.com/wiki/KOReader)
- Desejos do mercado de leitores (liberdade de biblioteca/formato, botões de página, cor, caneta, reparabilidade) — [TechRadar: Kindle vs Kobo](https://www.techradar.com/versus/kindle-vs-kobo), [TechRadar: melhores e-readers 2026](https://www.techradar.com/best/best-ereader)
- Projetos de e-reader e-ink em ESP32 — [Inkplate (portal dev Espressif)](https://developer.espressif.com/blog/2026/05/inkplate-esp32-epaper-development-boards/), [EPub-InkPlate (GitHub)](https://github.com/turgu1/EPub-InkPlate), [itsfoss: opções de e-reader open-source](https://itsfoss.com/open-source-ebook-readers-options/)
- Pedidos de funcionalidade abertos do upstream CrossPoint — [issues crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader/issues) (números de issue linkados acima)
