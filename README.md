# Nigh Editor

Editor de vídeo portátil em português para Windows de 32 bits. O Nigh usa uma interface nativa com **biblioteca, monitores de fonte e programa, linha do tempo e propriedades**. A edição trabalha com proxies no disco; a exportação usa os originais. O assistente roda sem Internet, GPU dedicada ou dependências de Python no computador do usuário.

## Baixar e começar

1. Baixe o [Nigh Editor 0.3.0 para Windows x86](https://github.com/Eliene-byte/Nigh-editor/releases/download/v0.3.0/Nigh-Editor-0.3.0-Windows-x86.zip). Extraia o ZIP inteiro e abra `Nigh Editor/NighEditor.exe`; mantenha os codecs e a pasta `models` junto dele.
2. Clique em **Abrir demo** para explorar *Maré baixa*, com transição, sobreposição, títulos e trilha. Em **Assistente → Abrir demo de IA**, experimente *Caderno de campo*: um vídeo curto com dois cortes secos, duas falas sintéticas e um rosto fora do centro.
3. Importe suas mídias. Escolha a fonte na biblioteca, use `I`/`O` para definir um trecho e arraste-o para a faixa desejada. `,` insere em V1 no cursor; o botão `+` acrescenta vídeo ao fim de V1 ou coloca áudio em A2.
4. Arraste o corpo dos clipes para mover e as bordas para aparar. `F2` ou duplo clique abre as propriedades. `T` cria um título. O botão de proporção do monitor de programa alterna entre **16:9, 1:1 e 9:16**.
5. Salve o projeto `.nigh` e escolha MP4 ou WAV em **Exportar**. Use `Alt+I` e `Alt+O` para exportar só uma parte da sequência.

### Assistente local

Selecione um clipe na sequência (ou abra uma fonte e marque In/Out) e pressione **F4**. A análise pausa a reprodução, roda em segundo plano sobre os proxies e pode ser cancelada. Examina no máximo **90 s** a partir do ponto de entrada. Nenhuma sugestão muda o projeto sozinha.

| Ferramenta | O que entrega |
| --- | --- |
| **Cortes de cena** | Classificador leve do Nigh indica cortes secos com precisão da prévia de 12 qps. Revise cada ponto, crie marcadores ou confirme a divisão do clipe em lote, sem apagar quadros. |
| **Atividade de voz** | O WebRTC VAD indica intervalos com fala provável. Marque a fala em revisão como trecho da fonte ou adicione marcadores na sequência. Não reconhece palavras, não separa interlocutores e não gera legendas. |
| **Reenquadrar pelo rosto** | O Pico localiza um rosto frontal em amostras do trecho. `F7` aplica uma proposta **estática** de escala e posição a um clipe V1; combine com 9:16 para um vídeo vertical. Não é identificação nem rastreamento. |
| **Ajuste leve** | Uma medição de brilho e volume do proxy sugere valores editáveis. Não é gradação de cor nem isolamento de voz. |

`F5` abre **Revisar sugestões**; `F6` e `Shift+F6` percorrem as ocorrências. A linha do tempo mostra cortes prováveis em azul e trechos de fala em verde. Edite os parâmetros aplicados com `F2` ou reverta com `Ctrl+Z`. Mudanças rápidas de câmera, música, rostos de perfil e pouca luz podem confundir os detectores. Confira os resultados antes de aplicar. [Modelos, pesos e licenças](models/README.md).

### Fluxo de montagem

| Área | O que você pode fazer |
| --- | --- |
| **Monitores** | Fonte e programa lado a lado quando há pelo menos 1110 pixels de área útil horizontal. Cada um mantém seu próprio cursor. Clique no monitor para ativar seus atalhos. Em telas estreitas ou no modo Eco, alterne por **Fonte** e **Programa**. |
| **V1 + A1** | Edição de montagem com cortes consecutivos: inserir no cursor, aparar, dividir, mover, copiar, colar, duplicar e remover. A1 acompanha o vídeo; remover de V1 fecha a lacuna dessa faixa, sem deslocar as outras. |
| **V2** | Faixa de sobreposição sem áudio com escala, posição e opacidade. Importe PNG transparente, JPG ou BMP e ajuste a duração na linha do tempo. |
| **Imagem** | Brilho, contraste e saturação por clipe. Dissolução na entrada de V1 a partir do vídeo anterior (ou do preto no início), sem alterar a duração da montagem. |
| **T1 / A1–A3** | Títulos e legendas manuais gravados na imagem. Volume, pan estéreo, fades e mudo por faixa. A2 e A3 também aceitam só o áudio de um vídeo. |
| **Navegação** | Quadro a quadro (`←`/`→`), zoom (`Ctrl+roda`), encaixe magnético (`Shift` para ignorar), marcadores (`M`, `Page Up`/`Page Down`), desfazer/refazer (`Ctrl+Z`/`Ctrl+Y`). |
| **Exportação** | MP4 H.264/AAC a **24 qps** (lado curto de 360, 480 ou 720 px) ou WAV PCM estéreo 48 kHz/16 bits. **Qualidade detalhada** usa CRF mais baixo e encoda mais devagar; é independente do modo Eco. |

### Projetos e recuperação

O `.nigh` guarda referências aos originais. Se um arquivo mudar de lugar, o editor pode pedir para religá-lo. A versão 0.3 lê os projetos das versões 0.1 e 0.2. Ao salvar, usa `NIGHED3`, que guarda também a proporção de saída. Use **Salvar como** se quiser preservar uma cópia para as versões anteriores. Depois do primeiro salvamento, o editor tenta manter uma cópia `.nigh.recuperacao` ao lado do projeto; abra o `.nigh` para recuperar as edições após uma interrupção. Sugestões não aplicadas não são salvas.

## Perfil de hardware

- **Alvo:** Windows XP SP3 de 32 bits ou Windows posterior capaz de executar Win32, com **SSE2** (como Pentium M). Ponto de partida: **1 GB de RAM**, **1024 × 600** e espaço no disco para os proxies e a exportação. Ainda não há medição num notebook físico de 2005.
- A prévia nativa usa proxies de **320 × 180 a 12 qps**, com PCM estéreo a 24 kHz no disco. **Modo Eco (`F8`)** usa um monitor e amostra a imagem a 6 qps durante a reprodução, sem alterar a edição de 24 qps nem o MP4 final.
- Prefira mídia SD e exportação leve de 360/480 px no hardware antigo. Decodificar HEVC/4K ou exportar em HD e qualidade detalhada leva mais tempo e pode esgotar a memória ou o espaço disponível.
- Limites: **2 h** por mídia/sequência, **128** mídias e **256** clipes por faixa editável; PNG/JPG/BMP até **4096 × 4096** e **32 MB** por imagem.
- Não há paridade com o Premiere: transcrição, multicâmera, keyframes, máscaras, estabilização e gradação avançada não fazem parte desta versão.

## Compilação e testes

O [GitHub Actions](.github/workflows/windows-x86.yml) compila e audita os executáveis Win32 (PE32/5.01), confere ZIP e SHA-256 e testa o **mesmo ZIP** em Windows Server 2022. Um push com a mensagem `release: vX.Y.Z` publica o pacote em [Releases](https://github.com/Eliene-byte/Nigh-editor/releases) somente se a compilação e os testes passarem. A auditoria e os testes não substituem execução no XP físico. O pacote não é assinado; confira o `.sha256` publicado.

No Linux x86-64, use GNU Make, Bash, `curl`, `tar`, `zip`, Python 3 e compiladores C e C++ nativos. Para compilar os binários Windows:

```sh
export PATH="$(bash tools/bootstrap-mingw.sh):$PATH"
make -j2 all test smoke
python3 tools/train-cuts.py --check
make -j2 package
```

Para testar o pacote localmente com Wine configurado para Win32:

```sh
mkdir -p build/zip-check build/smoke-out
unzip -qo artifacts/Nigh-Editor-0.3.0-Windows-x86.zip -d build/zip-check
LC_ALL=C.UTF-8 wine build/smoke.exe 'build/zip-check/Nigh Editor' \
  'build/zip-check/Nigh Editor/demo' build/smoke-out
```

Os testes incluem importação, formatos `.nigh` legados, composição, áudio, exportação 16:9 e vertical, sugestões de corte/fala/rosto e cancelamento. O teste da interface abre o executável empacotado e verifica aplicação de reenquadramento em uma propriedade editável, modo Eco, títulos e recuperação. Os codecs são processos separados, sem rede. O ZIP inclui avisos e o código-fonte correspondente dos codecs.
