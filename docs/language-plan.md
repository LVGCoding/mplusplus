# M++ language: implementation plan

M++ documents (`.mpp`) are programs written in math notation. Running one
transpiles the document to C++, compiles it with the system compiler against a
small runtime built on [Eigen](https://eigen.tuxfamily.org) (linear algebra) and
[Boost.Math](https://www.boost.org/doc/libs/release/libs/math/) (distributions,
special functions, quadrature), and runs it. The original ideas are in
`lang_ideas.pdf`; this document records the decisions made since and the plan.

## Decisions so far

- `=` is always equality; there is no `==`. `:=` defines, `←` mutates.
- A run of letters is one name: `AB` is a variable called `AB`; write `A·B` to
  multiply. Juxtaposition multiplies only when it can't mean anything else:
  next to a structure (`A\frac{1}{2}`, `2√x`, `x²y`, `A` next to a matrix),
  after a number (`2x`, `2(x+1)`), or between parentheses (`(a+b)(c−d)`).
- `name(…)` is always a function call.
- Blocks use braces. One statement per row; `;` is optional, to put several
  on one row. A row ending in a binary operator, `,` or `{` continues on the next.
- Subscripts are *indices* when they are numbers or use only declared names,
  otherwise *labels* that are part of the name (`v_{max}`). Assigning to an
  index of an undeclared name creates an implicit array (`α_1 := 1`, `α_2 := 2`).
- Arrays are 1-indexed. `x_{i,*}` is row i, `x_{*,j}` is column j.
- Maps use the exponent form: `ℝ^{Σ*}` maps strings to reals (so `ℝⁿ` is a map
  from `1..n`).
- Alternatives are written as sets: `scale ∈ {"linear", "log"}`.
- Transpose is `⊤` (`A^\top`), so a variable named `T` stays usable.
- `/` and fractions always give ℝ; use `⌊a/b⌋` and `mod` for integers.
- A graph in a document is configured in the editor (GraphNode and its menu);
  running the document renders it in a Graph tab.
- Libraries: Eigen + Boost.Math, header-only, fetched by CMake.

## Architecture

```
FileMpp::content (one LineNode per row)
  │  src/lang/Lower      node tree → tokens (each structure node becomes one token)
  │  src/lang/Parser     tokens → AST (Pratt parser, one statement per row, { } blocks)
  │  src/lang/Checker    names, types, dimensions, families, index vs label
  │  src/lang/CodeGen    AST → C++ source that calls the mpp runtime
  ▼
src/run/Driver           write temp .cpp → c++ (precompiled header) → run on a background thread
  ├─ stdout/stderr ──────────→ terminal panel (errors carry a row and highlight it)
  └─ results file (JSON lines) → Graph tabs and tables
runtime/include/mpp/*.hpp  header-only runtime built on Eigen + Boost.Math
```

Every token and AST node carries a `SourceRef { CursorPath path; size_t begin,
end; }`, so any error, from the checker or at run time, points at the exact
place in the editor.

### Toolchain

- Eigen 3.4.0 and Boost.Math (standalone) come from `FetchContent`. The build
  copies their headers and `runtime/include` to `<build>/runtime/include`.
- At run time the compiler is `$CXX`, else the first of `c++`, `clang++`, `g++`
  on `PATH`. Flags: `-std=c++20 -O1`.
- The runtime's precompiled header is built once and cached in
  `~/.cache/mplusplus/`, keyed by compiler version and runtime contents.
- Programs run in a child process on a background thread. Output streams to
  the terminal; Run → Stop kills it, and there is a time limit.

### Lowering (`src/lang/Lower`)

Walks each row's `LineNode` into tokens:

| Source | Token |
|---|---|
| Digits in `TextNode` | `Number` (`12`, `3.5`, `.5`, `1e-3`) |
| Letter runs | `Word`; keywords are reserved: `if else while for return break continue mod in` |
| `GreekNode` / `SymbolNode` | `Word` (α) or `Symbol` (∈, ←, …) |
| ASCII operators | `:=`, `<-`, `->`, `..`, `+ - * / ( ) [ ] , ; . { } \| <= >= !=` |
| `"…"` | `String`, with `${…}` parts kept as nested token lists |
| `FracNode`, `SqrtNode`, `ExpoNode`, `MatrixNode`, later Subscript/BigOp/Cases/Delim | `Structure` token: the node plus a lowered token list per slot |
| `GraphNode`, `TableNode` | `Structure` tokens that are statements of their own |
| End of row | `Newline`, skipped after `{`, `,` or a binary operator |

`ExpoNode` holds its own base, so `ℝ^n`, `A^T` and `x^2` arrive grouped.

### Parser (`src/lang/Parser`)

```
decl     := name ∈ Type [:= expr]   |   name := expr
mutate   := lvalue ← expr            (lvalue: x, x_i, x_{i,j}, p.name)
funcdef  := name(params) [→ Type] := expr  |  name(params) [→ Type] block
            params: x ∈ T, y ∈ T
if       := if expr block {else if expr block} [else block]
while    := while expr block
for      := for name ∈ expr block
return / break / continue / expression statement
block    := { statements }
```

Precedence, lowest to highest:

| Level | Operators |
|---|---|
| 1 | `∨ ⊕` |
| 2 | `∧` |
| 3 | `¬` (prefix) |
| 4 | `= ≠ < ≤ > ≥ ≈ ∈ ∉ ⊂ ⊆`, chainable (`0 ≤ x < n`) |
| 5 | `..` range, optional step `a..(s)..b` |
| 6 | `∪ ∖ Δ` |
| 7 | `∩` |
| 8 | `+ −` |
| 9 | `· * × / ⊙ mod`, implicit multiplication |
| 10 | unary `−` |
| 11 | postfix: power/structure, subscript, `ᵀ`, `!`, call, `.name`, `[key]` |

Two names separated only by a space are an error: "write `A·B` to multiply".

### Checker (`src/lang/Checker`)

- Types: Int (ℤ; ℕ is Int with a ≥0 check), Real, Complex, Bool, Char,
  String, `Vector(elem, n?)`, `Matrix(elem, r?, c?)`, `List(T)` for non-numeric
  `T^*`, `Map(K, V)`, `Set(T)`, `Enum`, `Struct`, `Function`, `Family`.
- Blocks open scopes. Functions see globals and their parameters only.
- ℕ → ℤ → ℝ → ℂ widen automatically; narrowing needs `⌊⌋`, `⌈⌉`, `round()`.
- Dimensions are checked at compile time when known, at run time otherwise.
- Families: indices that are constants covering 1..n become a fixed vector.
  Reading an unassigned element is a runtime error.
- Declarations take types. `for`, Σ, ∀ and set-builder need finite sets.
- Return types are inferred; recursive functions declare `→ T`.

### Code generation (`src/lang/CodeGen`)

- One `.cpp`: `#include <mpp/mpp.hpp>`, structs, functions, then `main()` with
  the top-level statements in document order.
- Each statement is preceded by `mpp::row = N;`. Runtime errors throw
  `mpp::Error`; `main` prints `error:ROW:COL: message` to stderr, which the
  editor parses to highlight the row.
- Names: `α` → `u_alpha`, `v_{max}` → `u_v_max` (the `u_` prefix avoids C++ keywords).
- 1-based indices: `x_i` → `mpp::at(x, i)` (bounds-checked).

| M++ | C++ |
|---|---|
| ℤ, ℕ | `int64_t` |
| ℝ | `double` |
| ℂ | `std::complex<double>` |
| 𝔹 | `bool` |
| Σ* | `std::string` |
| ℝⁿ (n known) | `Eigen::Vector<double,n>` |
| ℝ^* | `Eigen::VectorXd` |
| ℝ^{n×m} | `Eigen::Matrix<double,n,m>` / `MatrixXd` |
| `T^*` (non-numeric T) | `std::vector<T>` |
| `V^K` | `std::map<K,V>` |
| finite sets | `mpp::Set<T>` |
| structs | C++ structs with a static field table holding `@tags` |

Σ/Π/∀/∃ become immediately invoked lambdas; piecewise becomes a `?:` chain;
string templates become `mpp::format(…)`.

### Runtime (`runtime/include/mpp/`)

`print` for every type, `at`/`slice` with bounds checks, `Family<T>`,
`Set<T>`, `format`, `≈` (relative 1e-9), errors and `row`, and linear algebra
helpers (`solve`, `eigenvalues`, `rank`, norms, identity/ones/zeros).

`output.hpp` writes result records as JSON lines to the file named by
`MPP_RESULTS`:

```json
{"kind":"graph","node":"<GraphNode id>","series":[{"type":"line","x":[…],"y":[…],"legend":"y=x^2"}]}
{"kind":"table","name":"people","columns":[…],"rows":[…]}
```

### Graphs

A `GraphNode` runs as a statement in document order, so it can use variables
defined above it. Each function line (`y = x^2`) becomes a lambda sampled over
`xMin..xMax`; data series from the graph menu name program variables. After
the run, the editor opens or refreshes one Graph tab per graph. Tabs are
currently `FileBase`; split out a `Tab` base so `GraphView : Tab` needn't fake
`Load`/`Save`.

## Phases

Each phase ends runnable and tested. **Lang** is compiler work, **Editor** is UI work.

### Phase 0: infrastructure
- Lang: FetchContent for Eigen and Boost.Math; runtime skeleton; `Toolchain`
  (find the compiler, build the PCH); `Driver` (compile and run on a
  background thread, stream output to the terminal); Stop and time limit.
- Editor: SymbolNode (ℝ ℕ ℤ ℚ ℂ 𝔹 Σ ∈ ∉ ⊂ ⊆ ∪ ∩ ∖ Δ ← → ↦ ⋅ × ⊙ ⊤ ∞ ∅ ≠ ≤ ≥ ≈
  ∧ ∨ ¬ ⊕ ∀ ∃), drawn with Noto Sans Math; fix `\alpha` + `b` saving as `\alphab`.
- Done when: Run shows output from a real compiled program without freezing the UI.

**Status: done.** Notes:
- `SymbolNode` is a separate class from `GreekNode`: Greek letters are names,
  symbols are operators and built-in sets, and lowering treats them differently.
  Each symbol has a kind (ordinary, operator, relation) used for spacing now
  and parsing later.
- Like TeX, one space after a command word is dropped when it is followed by a
  letter or another space (`\alpha b` is α then b); `LineNode::ToText` writes
  it back in exactly those cases, so documents round-trip.
- Error underlines and clicking terminal errors moved to phase 1, where
  diagnostics with source locations first exist.

### Phase 1: scalars (vertical slice)
- Lang: lowering, parser, checker, codegen for ℤ/ℝ/𝔹; `:=`, `∈ T :=`, `←`;
  `+ − · / mod`, fraction, √ and ⁿ√, powers; chained comparisons, ∧∨¬;
  `print`, strings; compile-time and runtime errors mapped to nodes.
- Editor: error underline with hover; clicking a terminal error jumps to it.
- Done when: `quadratic.mpp` computes roots with √ and fractions.

**Status: done.** Notes:
- `src/lang`: `Lower` → `Parse` → `Check` → `Generate`, wrapped by `Compile`.
  `SourceRef` is a pair of full cursor paths (like a selection), so the
  editor underlines with the same range code as selections.
- Runtime errors print `error:N: message`, where N indexes the locations the
  code generator recorded (statements, denominators, roots, ...), not a
  row/column; the editor maps N back to a range.
- ℤ arithmetic is overflow-checked; `/` by zero, √ of a negative, `mod 0`,
  domain errors in ln/arcsin and negative ℕ values stop with an error.
- `print(a, b)` separates arguments with spaces. Built-ins: π, e, ∞, sin…tanh,
  arcsin/arccos/arctan, exp, ln, log10, log2, sqrt, abs, floor, ceil, round,
  sign, min, max.
- `:=` and `←` don't continue onto the next row (a half-typed row would
  swallow the next one); binary operators and open parentheses do.
- An ExpoNode with an empty base (what typing `^` inserts) raises the value
  before it; `^` typed as text works too.
- Editor: diagnostics live on `FileMpp` and are cleared on every edit; after
  a document's first run it is re-checked 0.4 s after each edit. Terminal
  error lines link to their range. Messages are drawn with
  `ui/rich_text.h`, which sends symbols the body font lacks (ℝ, ⋅, ←) to the
  math font.
- Golden programs: `tests/programs/*.mpp` + `.out`, run by `programs_test.cpp`.

### Phase 2: control flow and functions
- Lang: if/else, while, `for` over ranges with steps, break/continue,
  functions (one-line, block, recursive), piecewise.
- Editor: Cases node; delimiters `|x|`, `⌊x⌋`, `⌈x⌉`.
- Done when: `collatz.mpp`, `piecewise.mpp` pass.

**Status: done.** Notes:
- Braces are typed as text and saved escaped (`\{`, `\}`), so they never
  open a LaTeX group. Enter keeps a row's indentation (one level more after
  `{`, and `{|}` splits into three rows); typing `}` on a blank indented row
  dedents it.
- `if c { … } else if d { … } else { … }`; `else` may start the row after
  the `}`. `while c { … }`. `for i ∈ a..b { … }` or `for i in a..s..b`
  (step s, may be negative; ℝ steps are computed as a + k·s). The loop
  variable can't be changed in the loop. Blocks have their own names.
- Functions: `f(x) := expr`, `f(x ∈ ℝ, n ∈ ℕ) → ℝ := expr`, or
  `f(x) { … return … }`. Defined only at top level; a function sees the
  globals defined before it, its parameters, and functions defined before
  it. Parameters without a type take the argument's type: each combination
  of argument types is checked separately and becomes its own C++
  `std::function` (so `f(2)` is ℤ and `f(2.5)` is ℝ). A function nobody calls
  is still checked, with ℝ for untyped parameters.
- Recursion needs no `→ T` when an earlier case or return already gives the
  type (`fib`, `fact`); otherwise the error asks for one. Calls nest at most
  100000 deep (runtime error, not a crash); programs run on a 1 GiB stack.
- Piecewise: the `cases` matrix environment (left brace), rows of
  `value & condition`; the condition may start with `if`, and the last may be
  `otherwise`/`else`. No matching case is a runtime error at the cases.
- Delimiters: `DelimNode` (`\abs{}`, `\floor{}`, `\ceil{}`), in the math popup;
  `|x|` typed with bars also works.

### Phase 3: vectors, matrices, families
- Lang: ℝⁿ, ℝ^{n×m}, ℝ^*; matrix literals; subscripts and slices; implicit
  families; `A·B`, `⊙`, `ᵀ`, `⁻¹`, det, trace, rank, `A\b`, eigen, norms,
  sized I/1/0; matrix printing.
- Editor: Subscript node; `‖·‖`; labels and indices drawn differently.
- Done when: `linsolve.mpp`, `alpha_family.mpp` pass.

### Phase 4: sets and big operators
- Lang: set literals, ranges as sets, ∪ ∩ ∖ Δ ∈ ∉ ⊂; set-builder; Σ/Π/⋃/⋂ with
  limits or `x ∈ S`; ∀/∃ with one separator; `|S|`.
- Editor: BigOp node.
- Done when: `primes.mpp` passes.

### Phase 5: data
- Lang: Σ/Σ*, template strings, `in`, string `+`/`*`; structs `⟨…⟩`; lists
  with `push`; maps; enums; `@tags`; tables printed in the terminal.
- Editor: angle-bracket struct node; TableNode connected to run results.
- Done when: `people_table.mpp` reproduces the table from `lang_ideas.pdf`.

### Phase 6: graphs
- Lang: GraphNode lowering, sampling, data series, results protocol;
  Figure with Line/Scatter/Histogram/Bar.
- Editor: `Tab` refactor; Graph tab (axes, ticks, legend, linear/log); data
  fields in the graph menu.
- Done when: running a document with a graph opens a Graph tab.

### Phase 7: probability
- Lang: `X ~ Normal(μ, σ²)` and other Boost distributions, `𝔼[X]`, `Var(X)`,
  `P(X ≤ a)`, sampling with `<random>`.
- Editor: `~`, 𝔼, ℙ.
- Done when: `dice.mpp`, `normal.mpp` pass.

## Testing

- Unit tests per stage. Inputs are written as LaTeX and turned into nodes
  with `Parser::ParseText`, so `Parse("x := \\frac{1}{2}")` is a test input.
- Golden programs: `tests/programs/name.mpp` + `name.out`, compiled and run.
  Labelled `slow` and skipped when no compiler is found.
- Each diagnostic has a test for its message and `SourceRef`.

## Later

Quaternions; set complement; bitwise operators; decorated mutation arrows
(←⁺); printing bare expressions notebook-style; TableNodes as data; Windows/MSVC.
