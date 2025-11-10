# Lemon Parser Generator

This directory contains the Lemon LALR(1) parser generator from the SQLite project.

## Overview

Lemon is a parser generator that reads a grammar specification and produces a C function to parse that grammar. It is similar to Yacc/Bison but has several advantages:

- **Thread-safe** - Generated parsers are reentrant
- **Fast** - Uses LALR(1) parsing algorithm
- **Memory efficient** - No malloc calls in generated parser
- **Simple** - Easier to use than Yacc/Bison
- **Battle-tested** - Used in SQLite for decades

## Source

- **Source Repository**: https://github.com/sqlite/sqlite
- **Original Files**:
  - `lemon.c` - Parser generator source code (6056 lines)
  - `lempar.c` - Parser template file (1086 lines)
- **Documentation**: https://sqlite.org/src/doc/trunk/doc/lemon.html
- **License**: Public Domain

## Building

```bash
make
```

This will compile `lemon.c` and produce the `lemon` executable.

## Usage

```bash
./lemon grammar.y
```

This will:
1. Read the grammar specification from `grammar.y`
2. Generate `grammar.c` (parser implementation)
3. Generate `grammar.h` (parser interface)

**Important**: The template file `lempar.c` must be in the current directory or Lemon will fail.

### Command Line Options

- `-b` - Print only the basename of the output file
- `-c` - Don't compress the action table
- `-d` - Debug mode - print lots of details
- `-g` - Don't generate a parser - just show the grammar
- `-m` - Output a makeheaders compatible file
- `-q` - Quiet mode - don't print statistics
- `-s` - Show parser statistics
- `-x` - Show version and exit
- `-Ttemplate` - Use alternative template file instead of lempar.c
- `-Ddirectory` - Look for template in specified directory

## Integration with Stream2 Parser

The Tick compiler uses Lemon for parsing. The grammar is defined in:
```
../../stream2-parser/grammar.y
```

To build the parser:
```bash
cd ../../stream2-parser
../vendor/lemon/lemon -dbuild grammar.y
```

This will generate:
- `build/grammar.c` - Parser implementation
- `build/grammar.h` - Parser interface

The `-dbuild` option tells Lemon to place the output in the `build/` directory.

## File Descriptions

### lemon.c (182KB, 6056 lines)
The main parser generator program. Contains:
- Grammar parser (parses .y files)
- LALR(1) parser generator algorithm
- State machine construction
- Conflict resolution (shift/reduce, reduce/reduce)
- Code generation

### lempar.c (37KB, 1086 lines)
The parser template. Contains:
- Skeleton parser code
- Token processing logic
- Error handling
- Stack management
- Memory management

The generator copies this template and fills in grammar-specific details.

## Advantages Over Yacc/Bison

1. **Thread Safety**: Generated parsers don't use global variables
2. **No Malloc**: Parser uses a fixed-size stack (configurable)
3. **Better Error Messages**: More helpful error reporting
4. **Cleaner Output**: Generated code is more readable
5. **Simpler Grammar Syntax**: Less boilerplate than Yacc
6. **Automatic Memory Management**: Destructors for non-terminals

## Example Grammar

```c
%include {
#include <stdio.h>
#include "ast.h"
}

%token_type {Token}
%default_type {AstNode*}

%type program {AstNode*}

program ::= statement_list(L). {
    parser->root = L;
}

statement_list(A) ::= statement(B). {
    A = B;
}

statement_list(A) ::= statement_list(B) statement(C). {
    A = ast_append(B, C);
}
```

## Documentation

For complete documentation, see:
- https://sqlite.org/lemon.html (Tutorial)
- https://sqlite.org/src/doc/trunk/doc/lemon.html (Full documentation)

## License

All of the source code to Lemon, including the template parser file `lempar.c` and documentation, are in the **public domain**. You can use, modify, and distribute this code freely without restriction.

## Version Information

This version was retrieved from the SQLite GitHub repository on 2025-11-10.

Latest commit at time of retrieval:
- Repository: https://github.com/sqlite/sqlite
- Branch: master
- Path: tool/lemon.c, tool/lempar.c

## Maintenance

To update to a newer version:
```bash
curl -o lemon.c "https://raw.githubusercontent.com/sqlite/sqlite/master/tool/lemon.c"
curl -o lempar.c "https://raw.githubusercontent.com/sqlite/sqlite/master/tool/lempar.c"
make clean
make
```

## Support

For questions about Lemon:
- SQLite Mailing List: https://sqlite.org/support.html
- Documentation: https://sqlite.org/lemon.html
- Source Code: https://github.com/sqlite/sqlite
