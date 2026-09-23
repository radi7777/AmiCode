# AmigaDOS shell commands
Description: AmigaDOS commands, paths, wildcards and scripts (this is not Unix). Read before running shell commands or writing build/Execute scripts.

## Paths
- Absolute: `Volume:dir/file` or `Assign:dir/file` (Work:, SYS:, S:, C:, T:, RAM:, ENV:). Case-insensitive, case-preserving.
- Relative paths start at the current directory. Parent is a leading `/` (`/include/x.h`, `//` two levels up). There is NO `..`, `.`, `~` or leading `/` for root.
- `Volume:` alone is the root of that volume. T: = temporary files, RAM: = ram disk, NIL: = discard output.
- Names with spaces need double quotes: `"Work:My Project/main.c"`.

## Wildcards (AmigaDOS patterns, not globs)
- `#?` = any characters (like `*`), `?` = one character, `(a|b)` = alternatives, `~(x)` = not x.
- `#?.c` all C files, `#?.(c|h)` sources and headers, `~(#?.info)` everything except icons.

## Unix -> AmigaDOS
| Unix | AmigaDOS |
| ls -l / ls | `List` / `Dir` |
| cat f | `Type f` |
| cp -r a b | `Copy a TO b ALL CLONE` |
| mv a b | `Rename a TO b` (same volume) |
| rm -rf d | `Delete d ALL QUIET FORCE` |
| mkdir -p a/b | `MakeDir a/b ALL` |
| grep -r text dir | `Search dir text ALL` (all files, recursive; `QUIET` = only file names). One directory, one file type: `Search dir/#?.c text`. There is no recursive file-type filter: in AmiCode use the search_files tool instead. `PATTERN` means the *search text* is a wildcard pattern. |
| find -name | `List dir PAT=#?.c ALL LFORMAT "%p%n"` |
| cat a b > c | `Join a b AS c` |
| which | `Which cmd` |
| export V=x / $V | `SetEnv V x` (global), `Set V x` (local) - read with `$V` |
| echo -e | `Echo "a*Nb"` (`*N` newline, `*"` quote, `*E` escape) |
| $(cmd) | backticks: `Echo "Heute: `Date`"` |
| chmod +x script | `Protect script +s` (then run it by name) |
| sh script | `Execute script` |
| expr | `Eval 3 * 4` |
| sleep 5 | `Wait 5` |
| cmd & | `Run >NIL: cmd` |
| cmd > f 2>&1 | `cmd >f` (stderr usually goes to the same console) |

- Pipes (`|`) are not reliable: redirect to a file in T: and read that file instead (`List >T:out.txt`, then `Type T:out.txt`).
- Put redirections directly after the command name: `List >T:x SYS:C`, `Copy >NIL: a TO b`.

## Useful templates (key words are optional in order)
- `List DIR/M,P=PAT/K,FILES/S,DIRS/S,ALL/S,QUICK/S,NOHEAD/S,LFORMAT/K,SORT/K`
- `Copy FROM/M,TO,ALL/S,CLONE/S,QUIET/S,MAKEDIR/S`   `Delete FILE/M/A,ALL/S,QUIET/S,FORCE/S`
- `Search FROM/M,SEARCH/A,ALL/S,NONUM/S,QUIET/S,FILE/S,PATTERN/S,CASE/S` (FILE: search file names, PATTERN: text is a pattern)
- `Assign NAME,TARGET/M,ADD/S,REMOVE/S,DEFER/S,PATH/S,EXISTS/S`   `Path DIR/M,ADD/S,SHOW/S,REMOVE/S`
- `Protect FILE/A,FLAGS,ADD/S,SUB/S,ALL/S`   `Version NAME,FULL/S,FILE/S`   `Stack SIZE/N`
- `cmd ?` prints a command's template.

## Return codes
- 0 OK, 5 WARN, 10 ERROR, 20 FAIL. `$RC` holds the last code. A script stops when a code reaches the fail limit (`FailAt 21` to keep going).

## Scripts (Execute)
```
.KEY name/A,opt/S        ; optional argument template, use <name> and <opt> in the body
FailAt 21
If EXISTS "<name>.c"
  Echo "baue <name>"
Else
  Echo "fehlt"
  Skip ende
EndIf
If $RC GT 0 VAL          ; numeric compare needs VAL
  Echo "Fehler"
EndIf
If WARN                  ; true when the last return code was >= 5
  Echo "Warnung"
EndIf
Lab ende
```
- Comments start with `;`. Script variables: `Set x 1`, then `$x`. `Quit 10` ends with a code.

## Important
- Commands run non-interactively: never start programs that wait for keyboard input or `Ask`.
- Programs need enough stack; many compilers need `Stack 200000` first.
- Every file has an icon companion `name.info` only if created from Workbench; ignore `.info` files.
