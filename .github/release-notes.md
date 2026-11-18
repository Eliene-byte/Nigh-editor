## Nigh Editor 0.3 — assistente local e vídeo vertical

Baixe o **ZIP para Windows de 32 bits**, extraia tudo e abra `NighEditor.exe`. Para testar as novidades, entre em **Assistente → Abrir demo de IA**. A demo *Caderno de campo* analisa um vídeo de nove segundos e apresenta sugestões que você pode revisar antes de aplicar.

- **Cortes de cena e fala provável:** navegue pelas sugestões, crie marcadores, use uma fala como trecho da fonte ou divida um clipe nos cortes indicados. Não é transcrição.
- **Reenquadramento estático por rosto:** `F7` aplica a proposta ao clipe V1. `F2` permite editar a posição e a escala normalmente. Não há identificação de pessoas nem rastreamento de movimento.
- **Nova área de montagem:** monitores de fonte e programa lado a lado nas telas mais largas, painel de revisão, interface em português e modo Eco (`F8`), com um monitor e prévia de 6 qps na reprodução.
- **Exportação:** sequência horizontal, quadrada ou vertical, a partir dos originais. MP4 em 24 qps com perfil leve ou detalhado (mais lento); áudio estéreo em WAV.
- **Projetos:** lê arquivos da v0.1 e v0.2; o novo formato preserva a proporção de saída.

A análise é local, dura no máximo 90 segundos de **mídia por trecho analisado**, não depende de GPU e não roda durante a reprodução. O alvo continua sendo XP SP3 x86 com SSE2. Prefira SD no hardware antigo. As verificações do pacote no Windows Server 2022 não comprovam fluidez ou compatibilidade integral em um notebook físico de 2005. Esta versão não replica todos os recursos do Premiere. O ZIP não é assinado; confira o `.sha256` para verificar a integridade.
