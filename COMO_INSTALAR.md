# Como colocar estes arquivos no seu repositório (pelo celular)

O repositório precisa ficar com estes arquivos **na raiz** (`README.md`, `ps4/`, `tools/`, `.github/`...).
Atenção: a pasta `.github` começa com ponto e alguns gerenciadores de arquivos do celular a escondem.
Por isso a pasta **`workflows_copiar/`** tem uma cópia visível dos dois workflows.

## Opção A — app de Git no celular (mais simples)

- iPhone: **Working Copy** · Android: **MGit** ou **Termux** (`pkg install git`).
- Descompacte o zip dentro da pasta do repositório clonado, faça commit e push.

## Opção B — só pelo navegador (sem app de Git)

O navegador do celular não envia pastas, só arquivos. Então:

1. No repositório, **Add file → Create new file**. Nome: `.github/workflows/instalar-zip.yml` e cole:

```yaml
name: "0 - Instalar do ZIP"
on:
  workflow_dispatch:
    inputs:
      zip_path:
        description: "Nome do zip enviado para a raiz do repositorio"
        default: "fnafworld-ps4.zip"
permissions:
  contents: write
jobs:
  install:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Descompactar
        env:
          ZIP_PATH: ${{ inputs.zip_path }}
        run: |
          unzip -o "$ZIP_PATH" -d _zip
          rm -rf _zip/.github      # o GITHUB_TOKEN nao pode gravar workflows: voce cria os 2 a mao (passo 4)
          cp -a _zip/. .
          rm -rf _zip "$ZIP_PATH"
      - name: Commit
        run: |
          git config user.name "github-actions"
          git config user.email "actions@github.com"
          git add -A
          git commit -m "Instala FNaF World PS4 (do zip)" || echo "nada para commitar"
          git push
```

2. **Add file → Upload files** e envie o **`fnafworld-ps4.zip`** (é um arquivo só; precisa ter menos de 25 MB).
3. **Actions → "0 - Instalar do ZIP" → Run workflow.** Os arquivos aparecem na raiz.
4. Crie **a mão** os dois workflows (Add file → Create new file), colando o conteúdo de:
   - `workflows_copiar/1-extract.yml`  → nome do arquivo novo: `.github/workflows/1-extract.yml`
   - `workflows_copiar/2-build-ps4.yml` → nome do arquivo novo: `.github/workflows/2-build-ps4.yml`
5. Pode apagar `.github/workflows/instalar-zip.yml` depois, e a pasta `workflows_copiar/`.

Depois siga o **README.md** (Release `assets` com o `FNaFWorld.exe` → rodar os workflows).
