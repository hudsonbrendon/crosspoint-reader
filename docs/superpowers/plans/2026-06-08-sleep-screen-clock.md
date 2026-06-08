# Relógio na Tela de Sleep — Plano de Implementação

> **Para quem vai executar (agentes ou humano):** SUB-SKILL OBRIGATÓRIA: use `superpowers:subagent-driven-development` (recomendado) ou `superpowers:executing-plans` para implementar este plano tarefa por tarefa. Os passos usam checkbox (`- [ ]`) para rastreamento.

**Objetivo:** Exibir o horário (HH:MM) na tela de sleep padrão do inkpoint, abaixo do texto "sleeping", controlado por um novo toggle de configuração, usando a infraestrutura de relógio (DS3231 + NTP) que já existe.

**Arquitetura:** Reutiliza `halClock` (HAL do RTC DS3231, já presente) e `formatTime()` — os mesmos usados pelo relógio da barra de status. Adiciona um novo setting booleano `sleepScreenClock` e desenha o horário em `SleepActivity::renderDefaultSleepScreen()`, antes do `invertScreen()`, para que herde a cor (branco no escuro / preto no LIGHT). Nada novo de hardware, nenhuma alocação de heap nova.

**Escopo confirmado com o usuário:**
- Só a tela de sleep **padrão** (logo + "INKPOINT" + "sleeping"). Modos custom/cover/blank/quick-resume ficam intactos.
- **Toggle novo** dedicado (`sleepScreenClock`), independente do `statusBarClock`.
- **Esconde** o relógio quando: toggle desligado, OU sem RTC (`!halClock.isAvailable()`), OU nunca sincronizado (`!clockHasBeenSynced`). Nesses casos a tela fica idêntica à de hoje.
- Formato segue `SETTINGS.clockFormat` (24h / 12h) e fuso `SETTINGS.clockUtcOffsetQ`, igual à barra de status.

**Stack:** C++20 (ESP-IDF/Arduino), PlatformIO, e-ink GfxRenderer, i18n via YAML + `scripts/gen_i18n.py`.

**Layout validado (ASCII):**
```
┌──────────────────────────────┐
│           ████████           │
│        ██  LOGO120  ██        │
│           ████████           │
│          I N K P O I N T      │
│            sleeping          │
│             14:32            │  ← relógio novo, pageHeight/2 + 125
└──────────────────────────────┘
```

**Restrição de verificação:** Este é firmware embarcado sem harness de teste de host. O "teste" de cada tarefa é a compilação limpa (`pio run`) e a revisão das condições de guarda. Validação visual final (4 orientações, modo escuro/claro, com e sem RTC) é tarefa do testador humano no hardware X3.

---

## Estrutura de Arquivos

| Arquivo | Mudança | Responsabilidade |
|---|---|---|
| `lib/I18n/translations/english.yaml` | Modificar | Nova string `STR_SLEEP_SCREEN_CLOCK` (rótulo do toggle) |
| `lib/I18n/I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` | Gerado (não commitar) | Regenerados por `gen_i18n.py` |
| `src/InkPointSettings.h` | Modificar | Declarar o setting `sleepScreenClock` |
| `src/SettingsList.h` | Modificar | Registrar o toggle na categoria Display |
| `src/activities/boot_sleep/SleepActivity.cpp` | Modificar | Desenhar o relógio em `renderDefaultSleepScreen()` |

> **Nota i18n:** Só o YAML é versionado. `I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` estão no `.gitignore` e são regenerados no build. Nunca commite os gerados nem os edite à mão.

---

## Tarefa 1: Adicionar a string i18n do rótulo do toggle

**Arquivos:**
- Modificar: `lib/I18n/translations/english.yaml:63`
- Gerado (NÃO commitar): `lib/I18n/I18nKeys.h`, `lib/I18n/I18nStrings.h`, `lib/I18n/I18nStrings.cpp`

- [ ] **Passo 1: Adicionar a chave no YAML inglês (referência)**

No arquivo `lib/I18n/translations/english.yaml`, logo após a linha 63 (`STR_SLEEP_SCREEN: "Sleep Screen"`), inserir:

```yaml
STR_SLEEP_SCREEN_CLOCK: "Sleep Screen Clock"
```

Resultado esperado (contexto):
```yaml
STR_SLEEP_SCREEN: "Sleep Screen"
STR_SLEEP_SCREEN_CLOCK: "Sleep Screen Clock"
STR_QUICK_RESUME_TIMEOUT: "Quick Resume on Timeout"
```

- [ ] **Passo 2: Regenerar as tabelas de string**

Rodar:
```bash
python scripts/gen_i18n.py lib/I18n/translations lib/I18n/
```
Esperado: sai sem erro; `lib/I18n/I18nKeys.h` passa a conter `STR_SLEEP_SCREEN_CLOCK`.

- [ ] **Passo 3: Confirmar que a chave foi gerada**

Rodar:
```bash
grep -n "STR_SLEEP_SCREEN_CLOCK" lib/I18n/I18nKeys.h
```
Esperado: uma linha com o enum `STR_SLEEP_SCREEN_CLOCK` (PASS). Se vazio, o gerador não rodou — repetir Passo 2.

- [ ] **Passo 4: Commit (apenas o YAML)**

```bash
git add lib/I18n/translations/english.yaml
git commit -m "feat: add STR_SLEEP_SCREEN_CLOCK i18n key for sleep clock toggle"
```
> NÃO adicionar `lib/I18n/I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` — estão no `.gitignore`. Rodar `git status` antes para confirmar que nenhum gerado entrou no stage.

---

## Tarefa 2: Declarar o setting `sleepScreenClock`

**Arquivos:**
- Modificar: `src/InkPointSettings.h:169`

- [ ] **Passo 1: Adicionar o campo na struct de settings**

No arquivo `src/InkPointSettings.h`, logo após a linha 169 (`uint8_t sleepScreenCoverFilter = NO_FILTER;`), inserir:

```cpp
  // Show wall-clock time on the default sleep screen (X3 only, requires DS3231 RTC + NTP sync)
  uint8_t sleepScreenClock = 0;
```

Resultado esperado (contexto):
```cpp
  // Sleep screen cover filter
  uint8_t sleepScreenCoverFilter = NO_FILTER;
  // Show wall-clock time on the default sleep screen (X3 only, requires DS3231 RTC + NTP sync)
  uint8_t sleepScreenClock = 0;
  // Status bar settings (statusBar retained for migration only)
  uint8_t statusBar = FULL;
```

> Default `0` (desligado). Persistência é automática: `saveToFile()`/`loadFromFile()` em `InkPointSettings.cpp` usam `JsonSettingsIO`, que serializa todo campo registrado em `SettingsList.h` (Tarefa 3) pela sua chave JSON. Nenhuma mudança em `.cpp` necessária.

- [ ] **Passo 2: Compilar para garantir que a struct ainda compila**

```bash
pio run
```
Esperado: build conclui sem erro (o campo novo ainda não é referenciado em lugar nenhum, então só valida sintaxe). Se demorar demais para iterar, esta verificação pode ser unificada com a da Tarefa 4 — mas confirme zero erros até o fim.

- [ ] **Passo 3: Commit**

```bash
git add src/InkPointSettings.h
git commit -m "feat: add sleepScreenClock setting field"
```

---

## Tarefa 3: Registrar o toggle no menu de configurações

**Arquivos:**
- Modificar: `src/SettingsList.h:114`

- [ ] **Passo 1: Adicionar a entrada Toggle na categoria Display**

No arquivo `src/SettingsList.h`, logo após a entrada `sleepScreenCoverFilter` (que termina na linha 114 com `"sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY),`), inserir:

```cpp
        SettingInfo::Toggle(StrId::STR_SLEEP_SCREEN_CLOCK, &InkPointSettings::sleepScreenClock,
                            "sleepScreenClock", StrId::STR_CAT_DISPLAY),
```

Resultado esperado (contexto):
```cpp
        SettingInfo::Enum(StrId::STR_SLEEP_COVER_FILTER, &InkPointSettings::sleepScreenCoverFilter,
                          {StrId::STR_NONE_OPT, StrId::STR_FILTER_CONTRAST, StrId::STR_INVERTED},
                          "sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY),
        SettingInfo::Toggle(StrId::STR_SLEEP_SCREEN_CLOCK, &InkPointSettings::sleepScreenClock,
                            "sleepScreenClock", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_QUICK_RESUME_TIMEOUT, &InkPointSettings::quickResumeSleepScreen,
```

> A assinatura usada é `SettingInfo::Toggle(StrId nameId, uint8_t InkPointSettings::* ptr, const char* key, StrId category)` — exatamente o mesmo padrão do `statusBarClock` em `SettingsList.h:241`. A chave JSON `"sleepScreenClock"` deve bater com o nome do campo da Tarefa 2 para a persistência funcionar. A categoria `STR_CAT_DISPLAY` agrupa junto das outras configs de sleep screen.

- [ ] **Passo 2: Compilar**

```bash
pio run
```
Esperado: build conclui sem erro. O setting agora aparece no menu Display do dispositivo e na API web, mas ainda não afeta o render.

- [ ] **Passo 3: Commit**

```bash
git add src/SettingsList.h
git commit -m "feat: register sleepScreenClock toggle under Display settings"
```

---

## Tarefa 4: Desenhar o relógio na tela de sleep padrão

**Arquivos:**
- Modificar: `src/activities/boot_sleep/SleepActivity.cpp:16` (include) e `:159` (render)

- [ ] **Passo 1: Adicionar o include do HalClock**

No arquivo `src/activities/boot_sleep/SleepActivity.cpp`, na lista de includes (linhas 3-9, que usa `<...>` para libs), adicionar após a linha 5 (`#include <GfxRenderer.h>`):

```cpp
#include <HalClock.h>
```

Resultado esperado (contexto):
```cpp
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
```

> `halClock` é um singleton global `extern` declarado em `HalClock.h:9` — não precisa instanciar nada.

- [ ] **Passo 2: Desenhar o relógio antes do invertScreen()**

No mesmo arquivo, na função `renderDefaultSleepScreen()` (linhas 152-167), inserir o bloco do relógio **depois** da linha 159 (`drawCenteredText(... STR_SLEEPING ...)`) e **antes** do comentário/bloco do `invertScreen()` na linha 161. O bloco final deve ficar assim:

```cpp
void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_INKPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_SLEEPING));

  // Clock: only when enabled, RTC present, and time has been synced at least once.
  // Drawn before invertScreen() so it inherits the dark/light colour like the rest.
  if (SETTINGS.sleepScreenClock && halClock.isAvailable() && SETTINGS.clockHasBeenSynced) {
    char timeBuf[9];  // 12h "HH:MM PM" needs >= 9 bytes (see HalClock::formatTime)
    if (halClock.formatTime(timeBuf, sizeof(timeBuf), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1)) {
      renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 + 125, timeBuf, true, EpdFontFamily::BOLD);
    }
  }

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != InkPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
```

> Pontos técnicos justificados:
> - **Sem heap novo:** `timeBuf` é `char[9]` na stack (9 bytes, bem abaixo do limite de 256 bytes de variáveis locais). `formatTime` escreve em buffer fornecido pelo chamador — nenhuma `String`/`std::string`.
> - **Guarda tripla** honra a decisão "esconder quando não sincronizado": toggle + RTC presente + `clockHasBeenSynced`. Se qualquer um falhar, nada é desenhado e a tela fica idêntica à atual.
> - **`formatTime` retorna `false`** se o RTC sumir entre o `isAvailable()` e a leitura — o `if` aninhado garante que nada quebrado seja desenhado.
> - **Posição `pageHeight/2 + 125`:** ~30px abaixo de "sleeping" (que está em +95), usando `UI_12_FONT_ID` (maior que o SMALL do "sleeping"), centralizado horizontalmente por `drawCenteredText`.
> - **`SETTINGS`, `tr()`, `UI_12_FONT_ID`** já estão disponíveis neste arquivo (via `InkPointSettings.h`, `I18n.h`/`Txt.h`, `fontIds.h` — includes 11, 7-8, 15).

- [ ] **Passo 3: Compilar do zero e verificar 0 erros/warnings**

```bash
pio run -t clean && pio run
```
Esperado: `SUCCESS`, 0 erros, 0 warnings novos. Se aparecer "halClock not declared", revisar o include do Passo 1.

- [ ] **Passo 4: Formatar o código**

```bash
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```
Esperado: sem mudanças, ou mudanças mínimas de espaçamento (re-stage se houver).

- [ ] **Passo 5: Commit**

```bash
git add src/activities/boot_sleep/SleepActivity.cpp
git commit -m "feat: show clock on default sleep screen when enabled"
```

---

## Tarefa 5: Verificação final (handoff)

- [ ] **Passo 1: Build limpo completo**

```bash
pio run -t clean && pio run
```
Esperado: SUCCESS, 0 erros/warnings.

- [ ] **Passo 2: Análise estática**

```bash
pio check
```
Esperado: sem novos findings em `SleepActivity.cpp`, `InkPointSettings.h`, `SettingsList.h`.

- [ ] **Passo 3: Confirmar que nenhum arquivo gerado/ignorado entrou nos commits**

```bash
git log --oneline -5
git show --stat HEAD~3..HEAD | grep -i "I18nKeys\|I18nStrings\|\.generated\.h\|\.pio/" || echo "OK: nenhum gerado commitado"
```
Esperado: `OK: nenhum gerado commitado`.

- [ ] **Passo 4: Itens para o testador humano (no hardware X3)**

Sinalizar ao usuário que falta validar no dispositivo:
- 🔲 Ligar o toggle "Sleep Screen Clock" em Settings → Display.
- 🔲 Sleep screen padrão (modo DARK): relógio aparece branco abaixo de "sleeping".
- 🔲 Modo LIGHT: relógio aparece preto (sem `invertScreen`).
- 🔲 Trocar `clockFormat` 24h ↔ 12h: formato muda (`14:32` ↔ `2:32 PM`).
- 🔲 Toggle desligado: tela idêntica à de antes (nada de relógio).
- 🔲 Hardware sem RTC (não-X3) ou antes de sincronizar NTP: nada de relógio.
- 🔲 Testar nas 4 orientações: relógio centralizado e dentro da área visível.
- 🔲 `ESP.getFreeHeap()` estável ao entrar/sair do sleep (não deve haver alocação nova).

---

## Auto-revisão

**Cobertura do escopo confirmado:**
- "Só tela padrão" → Tarefa 4 mexe apenas em `renderDefaultSleepScreen()`. ✅
- "Toggle novo" → Tarefas 2 (campo) + 3 (UI). ✅
- "Esconde sem RTC / não sincronizado" → guarda tripla na Tarefa 4, Passo 2. ✅
- Formato 24h/12h e fuso → `formatTime(... clockUtcOffsetQ, clockFormat == 1)`. ✅
- Cor escuro/claro → desenho antes do `invertScreen()`. ✅

**Consistência de tipos/nomes:**
- Campo `sleepScreenClock` (Tarefa 2) = ponteiro-membro na Tarefa 3 = guarda na Tarefa 4. ✅
- Chave JSON `"sleepScreenClock"` (Tarefa 3) == nome do campo (Tarefa 2), exigido pela persistência. ✅
- `STR_SLEEP_SCREEN_CLOCK` (Tarefa 1) usada na Tarefa 3. ✅
- `UI_12_FONT_ID`, `SMALL_FONT_ID`, `UI_10_FONT_ID` confirmados em `src/fontIds.h`. ✅
- Assinaturas `halClock.isAvailable()`, `halClock.formatTime(buf, size, offset, use12h)` conferem com `HalClock.h:25,36`. ✅

**Sem placeholders:** todos os passos têm código/comando concreto. ✅

**Restrição de RAM:** nenhuma alocação de heap nova; só `char[9]` na stack. ✅
