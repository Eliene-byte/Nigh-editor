# Modelos locais do Nigh Editor

- `facefinder.bin`: classificador de rostos frontais **Pico**, Nenad Markuš, MIT. Pesos originais de `rnt/cascades/facefinder` no commit [`7d550c7`](https://github.com/nenadmarkus/pico/tree/7d550c78b2c31a4e1dfc5bcdfe9da013297b5cc8). SHA-256: `d8014993e7298c7b1865d1f8b855d6dbf4ec5c808bf879e2091ab6837abf90cd`. O Nigh usa uma implementação de inferência de escala fixa, derivada do runtime MIT, e propõe um enquadramento estático; não identifica pessoas.
- `src/vendor/vad/webrtc`: detector de atividade de voz do WebRTC (modelo estatístico GMM, licença BSD). Vendorizado de [`wiseman/py-webrtcvad` 2.0.10](https://github.com/wiseman/py-webrtcvad/tree/ba13ceea9dd3f3a068e869b2e94413205bd87f7d/cbits/webrtc), sem a extensão Python. A amostra de teste `tests/fixtures/speech-8k.raw` vem do mesmo commit. Avisos em `src/vendor/vad/LICENSE`.
- `src/cut_weights.hpp`: classificador linear do Nigh, treinado com cenas sintéticas geradas por `python3 tools/train-cuts.py`. Distingue mudanças abruptas de imagem de pequenos deslocamentos, fades e flashes. Os dados sintéticos permitem reproduzir o treino e os testes; não medem a precisão em filmagens reais.

Os pesos são usados apenas quando você pede uma análise. O editor não faz download de modelos, não usa GPU ou serviços remotos e não treina com os seus arquivos. Nenhum dos modelos é da Adobe.
