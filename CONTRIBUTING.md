# Contributor Guidelines

We're happy to take contributions to Couchbase Lite For C, particularly:

* Bug reports
* Bug fixes
* Portability improvements
* Unit tests
* Sample code

We are unlikely to accept contributions that change the C API, because Couchbase Lite API changes need to go through a review process, and we try to keep the API fairly consistent between platforms. (The C++ and Python APIs are less official, so more flexible.) If you have a good argument for an API change or enhancement, please file an issue and we'll discuss it :)

## Coding Guidelines

**The important stuff:**

* We use C++11, so don't use newer features. We might upgrade to C++14 or even C++17, but not yet.
* The code has to build for as many platforms as possible, with at least Clang and MSVC, so don't use platform-specific or compiler-specific features unless bracketed with `#ifdef`s, or hidden inside a `#define` macro. (Most of these are in `CBL_Compat.h`.)
  - Watch out for common C system headers like `<unistd.h>` that don't exist on Windows.
  - Not all functions in C standard library headers are standard! BSD-ancestry systems like macOS and FreeBSD have some venerable functions that didn't make it into the ANSI spec, like `strlcpy`, while other functions like `asprintf` came from GNU and aren't in Windows.
* Use `_cbl_nonnull` to mark parameters that must be non-NULL, and `_cbl_warn_unused` to mark functions that return values that need to be released/freed.
* Be careful when adding source files or source directories, as it can break other builds. If you use Xcode, try a CMake build and make sure it still works. If you use CMake on a Mac, try an Xcode build. Or at least note in the PR that other build systems may need to be updated.
* PRs should come with unit tests that test their fixes/improvements. Extending an existing unit test is OK.
* Unit tests use [Catch](https://github.com/catchorg/Catch2). (Although we should add some tests written in C, and won't be able to use Catch for those.)

**The naming of things is a serious matter:**

* We tend toward CamelCase, not snake_case.
* Class and type names are capitalized (usually), variable names aren't. The exceptions are some structs and struct-like classes such as `slice`.
* Class member variable names are prefixed with "`_`", unless they're public.

**We're not obsessive about code style, but we do prefer:**

* [K&R style](https://en.wikipedia.org/wiki/Indentation_style#K&R_style) (open brace at end of line, space after `if`, `for`, `while`, ...) Jens has some weird rules about whether a function's open brace goes at the end of the line, but he won't make you follow them.
* 100-character line length limit. (But we're lax in unit test files.)
* 4-character indents
* ...with spaces, not tabs. Please! No one can agree how wide a tab character is.
* It's silly to care about invisible spaces at the end of a line, but given that `git` and other tools do care and show them in diffs, it's better to avoid them. Just tell your editor/IDE to suppress them.
