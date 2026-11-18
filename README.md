# Nigh Editor

Editor de vídeo portátil para Windows x86, com interface em português e pensado para a geração do Pentium M. O Nigh Editor segue o fluxo **biblioteca → monitor de fonte → linha do tempo → exportação**, sem tentar reproduzir a interface ou todos os recursos do Premiere Pro.

## Baixar e começar

1. Baixe o [Nigh Editor 0.2.0 para Windows de 32 bits](https://github.com/Eliene-byte/Nigh-editor/releases/download/v0.2.0/Nigh-Editor-0.2.0-Windows-x86.zip) e extraia o ZIP inteiro. Mantenha `ffmpeg.exe` e `ffprobe.exe` ao lado de `NighEditor.exe`.
2. Abra o executável. Use **Abrir demo** para explorar *Maré baixa*, com uma transição, vídeo sobreposto, títulos e áudio. Ou clique em **Importar** para abrir suas mídias.
3. Na biblioteca, selecione um arquivo e use `I` e `O` no monitor de fonte para marcar o trecho. Arraste para a faixa desejada ou use **Ferramentas → Inserir fonte em V1 no cursor** (`,`). O botão **+** adiciona vídeo ao fim de V1 ou coloca áudio em A2.
4. Na linha do tempo, arraste o corpo do clipe para mover ou as bordas para aparar. Use **F2** ou dê duplo clique para abrir **Propriedades**. O botão **Título** cria texto em T1.
5. Salve o `.nigh`. Em **Exportar**, escolha MP4 360p, 480p ou 720p, ou o mix estéreo em WAV. Use `Alt+I` e `Alt+O` para limitar a exportação ao intervalo marcado.

## Recursos

| Área | O que você pode fazer |
| --- | --- |
| **V1 + A1** | Edição de montagem com cortes consecutivos: inserir no cursor, aparar, dividir, mover, copiar, colar, duplicar e remover. A1 acompanha o vídeo; remover de V1 fecha a lacuna dessa faixa, sem deslocar as outras. |
| **V2** | Faixa de sobreposição sem áudio com escala, posição e opacidade. Por padrão, um clipe novo entra como imagem sobre imagem. Importe PNG com transparência, JPG ou BMP e ajuste sua duração. |
| **Imagem** | Brilho, contraste e saturação por clipe. Em V1, a dissolução na entrada começa no clipe anterior (usa o trecho após o corte, se disponível, e repete o último quadro se acabar) ou no preto, no começo da sequência. Não altera a duração da montagem. |
| **T1** | Títulos e legendas manuais gravados na imagem. Edite texto, duração, posição, tamanho, cor e fundo. |
| **A1, A2, A3** | Volume, pan estéreo, fades de entrada/saída e mudo por faixa. A2 e A3 são independentes e também aceitam só o áudio de um vídeo. |
| **Navegação** | Prévia composta, avanço quadro a quadro (`←`/`→`), zoom (`Ctrl+roda`), encaixe magnético (segure `Shift` para ignorar), marcadores nomeados (`M`, `Page Up`/`Page Down`) e desfazer/refazer (`Ctrl+Z`/`Ctrl+Y`). |
| **Exportação** | H.264 + AAC estéreo a 24 qps, ou WAV PCM estéreo 48 kHz/16 bits. Exporte toda a sequência ou apenas o intervalo In/Out. A exportação parte dos originais, não dos proxies. |

### Arquivos de projeto

- O `.nigh` guarda referências aos originais, não cópias dos vídeos. As referências ficam relativas para mídias na pasta do projeto. Para mover um projeto, preserve essa estrutura ou aponte os arquivos quando o editor pedir para **religar mídia**.
- A versão 0.2 lê arquivos `.nigh` da 0.1. Ao salvar, usa o novo formato; use **Salvar como** para manter uma cópia que continue abrindo na 0.1.
- Depois do primeiro salvamento, o programa tenta gravar uma cópia `.nigh.recuperacao` junto do projeto a cada 45 segundos se houver mudanças. Em caso de interrupção, abra o `.nigh` original para recuperar a cópia. O original só muda com **Salvar**. Deixe espaço livre e permissão de gravação na pasta.

## Perfil de hardware

- **Alvo:** Windows XP SP3 de 32 bits ou Windows posterior capaz de executar Win32, em CPU **SSE2** (por exemplo, Pentium M). Recomendação inicial: **1 GB de RAM** e **1024 × 600**. Não há medição de desempenho em um notebook físico de 2005.
- Sem conta, GPU dedicada, navegador embutido ou conexão de rede. A prévia usa Win32/GDI, proxies **320 × 180 a 12 qps** e áudio PCM estéreo a 24 kHz. O disco guarda os proxies temporários e os intermediários de exportação; mantenha espaço livre.
- Use mídia SD e MP4 360p/480p no hardware antigo. O 720p é uma opção mais lenta para máquinas com folga. HEVC/4K e projetos longos podem exceder o tempo, a memória ou o disco disponível.
- Até **2 h** por mídia/sequência, **128** mídias e **256** clipes por faixa editável. PNG/JPG/BMP até **4096 × 4096** e **32 MB** por imagem. O suporte de vídeo e áudio depende dos codecs contidos no arquivo.
- Esta versão não inclui multicâmera, keyframes, máscaras, estabilização, legendas automáticas, gradação avançada ou edição colaborativa. É um editor de montagem leve, não uma medida de paridade com o Premiere.

## Compilação e testes

O fluxo [Windows x86](.github/workflows/windows-x86.yml) compila o pacote no GitHub, verifica PE32/5.01 e ZIP/SHA-256 e testa o **mesmo pacote** em Windows Server 2022. Depois de uma execução bem-sucedida, commits com primeira linha `release: vX.Y.Z` (a versão de `tools/package.sh`) publicam o binário em [Releases](https://github.com/Eliene-byte/Nigh-editor/releases). Com o fluxo na branch padrão, também é possível usar **publicar** na execução manual.

No Linux x86-64, use GNU Make, Bash, `curl`, `tar`, `zip`, Python 3 e um compilador C++ nativo. Prepare o MinGW-w64 i686/MSVCRT compatível com XP e compile:

```sh
export PATH="$(bash tools/bootstrap-mingw.sh):$PATH"
make -j2 all test smoke
make -j2 package
```

Os codecs são processos separados e sem rede; o ZIP inclui os avisos e o código-fonte correspondente. O pacote não é assinado; confira a integridade com o `.sha256` publicado.

### Verificação local

```sh
make audit
mkdir -p build/smoke-out
LC_ALL=C.UTF-8 wine build/smoke.exe build/codecs demo build/smoke-out
```

O teste Win32 verifica projetos legados, a persistência de várias faixas, os pixels da prévia e do MP4 composto, pan/fades, MP4 360p/480p/720p, WAV, exportação de intervalo, cancelamento e proteção da saída. A CI também abre o executável com a demonstração, ajusta propriedades, insere títulos e recupera uma cópia de salvamento. Esses testes não substituem rodar o pacote no Windows XP físico.
