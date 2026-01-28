# liblpm Manual Pages

This directory contains the manual pages for the liblpm library.

## Structure

```
man/
├── man3/              # English (default)
│   ├── liblpm.3       # Main library overview
│   ├── lpm_create.3
│   ├── lpm_lookup.3
│   ├── lpm_add.3
│   ├── lpm_delete.3
│   ├── lpm_destroy.3
│   ├── lpm_algorithms.3
│   └── *.3            # Aliases for function variants
├── pt_BR/             # Brazilian Portuguese (when available)
│   └── man3/
│       └── *.3
└── ...                # Other languages
```

## Viewing Man Pages

After installation (`sudo make install`), view man pages with:

```bash
man liblpm          # Library overview
man lpm_create      # Creation functions
man lpm_lookup      # Lookup functions
man lpm_lookup_ipv4 # Redirects to lpm_lookup
man 3 liblpm        # Explicitly request section 3

# View in specific language (if installed)
LANG=pt_BR.UTF-8 man liblpm
LANG=de_DE.UTF-8 man liblpm
```

## Local Testing

To test man pages without installing:

```bash
# View a specific man page
man -l docs/man/man3/liblpm.3

# Check for formatting errors
groff -man -z docs/man/man3/liblpm.3
man --warnings -l docs/man/man3/liblpm.3
```

## Linting

Use the linting script to validate all man pages:

```bash
# Run linter (requires: apt install mandoc)
./scripts/lint-manpages.sh

# Strict mode (treat style warnings as errors)
./scripts/lint-manpages.sh --strict
```

Man pages are automatically linted in CI on pull requests.

## Man Page Format

Man pages are written in groff format using standard macros:

- `.TH` - Title header
- `.SH` - Section header
- `.SS` - Subsection header
- `.B` - Bold text
- `.I` - Italic text
- `.TP` - Tagged paragraph (for options/arguments)
- `.EX/.EE` - Example blocks (preformatted)
- `.PP` - Paragraph break

## Maintenance

When updating the API:

1. Update the corresponding man page in `man3/`
2. Add new `.so` redirect files for new function variants
3. Update the version in `.TH` headers if needed
4. Run `./scripts/lint-manpages.sh` before committing
5. Test with `man -l` before committing

## Contributing Translations

To contribute a translation:

1. Create the language directory: `docs/man/<LANG>/man3/`
   - Use standard locale codes: `pt_BR`, `de`, `es`, `fr`, `zh_CN`, etc.

2. Copy the English man pages as templates:
   ```bash
   mkdir -p docs/man/pt_BR/man3
   cp docs/man/man3/liblpm.3 docs/man/pt_BR/man3/
   ```

3. Translate the content, updating the header:
   ```groff
   .\" liblpm.3 - Portuguese (Brazil) translation
   .\" Translated from English version 2.0.0
   .\" Translator: Your Name <your.email@example.com>
   .\"
   .TH LIBLPM 3 "2026-01-28" "liblpm 2.0.0" "Funcoes da Biblioteca liblpm"
   ```

4. Run the linter to validate: `./scripts/lint-manpages.sh`

5. Submit a pull request with your translation

### Translation Guidelines

- Keep technical terms (function names, types) in English
- Translate section headers and descriptions
- Update examples if they contain comments
- Maintain the same structure as the English version
- Include translator credit in the header comments
