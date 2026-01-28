# liblpm Manual Pages

This directory contains the manual pages for the liblpm library.

## Structure

```
man/
└── man3/           # Library functions (section 3)
    ├── liblpm.3    # Main library overview
    ├── lpm_create.3
    ├── lpm_lookup.3
    ├── lpm_add.3
    ├── lpm_delete.3
    ├── lpm_destroy.3
    ├── lpm_algorithms.3
    └── *.3         # Aliases for specific function variants
```

## Viewing Man Pages

After installation (`sudo make install`), view man pages with:

```bash
man liblpm          # Library overview
man lpm_create      # Creation functions
man lpm_lookup      # Lookup functions
man lpm_lookup_ipv4 # Redirects to lpm_lookup
man 3 liblpm        # Explicitly request section 3
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
4. Test with `man -l` before committing
