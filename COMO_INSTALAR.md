# Como instalar o projeto no seu repositório (só pelo celular)

O navegador do celular não envia pastas, só arquivos. Por isso o jeito mais simples é enviar **1 arquivo (o zip)**
e deixar o GitHub Actions extrair tudo na raiz do repositório.

## Passo a passo

1. **Crie o extrator de ZIP (uma vez só).** No repositório: **Add file → Create new file**.
   Nome do arquivo: `.github/workflows/0-instalar-zip.yml`
   Cole o conteúdo do arquivo `0-instalar-zip.yml` (veio junto com o zip, e também está dentro dele em `workflows_copiar/`).
   Toque em **Commit changes**.
2. **Envie o zip:** **Add file → Upload files** → escolha `fnafworld-ps4.zip` → **Commit changes**.
   Fica na **raiz** do repositório (não dentro de pasta).
3. O workflow **"0 - Instalar ZIP na raiz"** roda sozinho (aba **Actions**), extrai tudo na raiz, apaga o zip e salva.
   Se não rodar sozinho: Actions → "0 - Instalar ZIP na raiz" → **Run workflow**.
4. **Passo manual (só se você NÃO fez a "Opção turbo" abaixo):** o GitHub não deixa um workflow criar outros workflows.
   Então crie a mão estes dois arquivos (Add file → Create new file), colando o conteúdo das cópias que agora estão no repositório:

   | Nome do arquivo novo | Cole o conteúdo de |
   |---|---|
   | `.github/workflows/1-extract.yml` | `workflows_copiar/1-extract.yml` |
   | `.github/workflows/2-build-ps4.yml` | `workflows_copiar/2-build-ps4.yml` |

   Dica: abra o arquivo em `workflows_copiar/`, toque nos 3 pontinhos (⋯) → **Copy raw file**, e cole no arquivo novo.

Pronto. Siga o **README.md** (Release `assets` com o `FNaFWorld.exe` → rodar os workflows).

## Opção turbo (opcional): instalar os workflows 1 e 2 automaticamente

Se criar um token de acesso, o passo 4 some e o extrator instala tudo sozinho.

1. GitHub → foto do perfil → **Settings → Developer settings → Personal access tokens → Fine-grained tokens → Generate new token**.
2. **Repository access:** *Only select repositories* → escolha este repositório.
3. **Permissions → Repository permissions:** *Contents* = **Read and write** e *Workflows* = **Read and write**.
4. Gere e **copie o token**.
5. No repositório: **Settings → Secrets and variables → Actions → New repository secret**.
   Nome: `PAT_TOKEN` · Valor: o token. Salve.
6. Faça o passo 2 acima (enviar o zip). Agora a pasta `.github/workflows` também é instalada.

## Atualizações futuras

Quando eu mandar um zip novo, é só repetir o **passo 2** (enviar o zip para a raiz). Os arquivos com o mesmo nome são substituídos.
Seus arquivos que não estão no zip (como a Release `assets` com o `.exe`) não são tocados.
