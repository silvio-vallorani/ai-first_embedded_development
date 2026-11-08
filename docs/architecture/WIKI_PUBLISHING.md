# GitHub Wiki Publishing

The GitHub Wiki is a fully generated publication of the Markdown documentation in this repository.

## Source of truth

The source documents remain in the main repository:

- `README.md`
- `docs/**/*.md`
- `examples/*/README.md`

Do not edit generated Wiki pages directly. Update the corresponding source document and merge the change into `main`.

## Automatic publishing

The `Synchronize GitHub Wiki` workflow runs when a documentation source or the generator changes on `main`. It can also be started manually with `workflow_dispatch`.

The generator is `scripts/publish_wiki.py`. It creates stable Wiki page slugs, rewrites links between Markdown documents, and builds `Home.md`.

## Required GitHub secret

Create a Personal Access Token owned by an account with write access to this repository's Wiki. A classic token with the `repo` scope is the broadly compatible option for private repositories.

Add it to the main repository under:

`Settings > Secrets and variables > Actions > New repository secret`

Use this exact secret name:

`WIKI_PUBLISH_TOKEN`

The token is used only by the Wiki publishing workflow. It is not stored in the repository or embedded in the Wiki remote URL.

## First-time Wiki initialization

GitHub may not create the `.wiki.git` repository until the Wiki has been enabled and its first page has been created. If the workflow reports that the Wiki repository cannot be found, open the repository's Wiki tab, enable it if necessary, and create an initial page. Then run the workflow again.

## Publication behavior

Every synchronization replaces the Wiki content with the generated output. Existing manual pages and edits are intentionally deleted. The Wiki is therefore a read-only publication; make all documentation changes in the main repository.

Wiki commits use this identity:

- Name: `Silvio Vallorani`
- Email: `silvio.vallorani.dev@gmail.com`
