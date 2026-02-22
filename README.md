# C Universe - A collection of header-only C libraries

This repository contains a set of header-only C libraries that I've been developing.

## Preface

I'm not a C developer, but I wanted to force myself to constrain myself to something simple and C99 felt just like the thing. Currently I have no other intent with them than to have fun developing them and improve my C skills, but I'm glad if others find it useful, even if just as a learning resource.

## Usability

Please note, that I haven't heavily used these libraries other than a few utility programs I've been writing. I'm aiming to dogfood myself here and even write all tools using these libraries when possible, but currently I can't guarantee correctness or performance. There is a basic set of unit tests for the libraries, aiming to cover basic functionality but usually nothing more. There have been no benchmarks - idea for another library, perhaps? -, no tweaks for performance anywhere.

## Why header-only?

I'm spoiled. I come from an ecosystem where package and dependency management is more or less a solved problem. It is unfortunately not in C - or even C++, which I was primarily using before. Header-only libraries felt like the most convenient things ever without tying myself to a specific technology or having to distribute binaries for each platform and compiler combination. While I'm aware they are more of a workaround than a real solution, I find it a fun design pattern.

## Design philosophy

While there's not a hard set of rules, I try to aim for the following:
 * Portable C99, unless there's a really good reason to be platform/compiler specific for a worthy convenience. An example of this is auto-registration for the unit testing library test cases.
 * No dependencies on other libraries, not even among each other.
 * A single header contains the API declaration, implementation, tests and sample code with the established pattern.
 * If the library allocates memory, make it customizable. The customization interface should be small, but complete. For example, no need to require `malloc`, `realloc` and `free`, it's enough to only depend on `realloc` and `free`.
 * The API of a library should be idiomatic to C and more or less consistent among each other. The public API surface should also be relatively minimal. I'm probably still very bad at both of these.
 * The API can break. These are single-header libraries, not even in the phase where I version them. If there is an obvious improvement that has to break API, then so be it.

## The libraries

 * [argparse.h](./src/argparse.h): Command-line argument parsing with the goal to copy the feature set (or at least the syntax) of [System.CommandLine](https://learn.microsoft.com/en-us/dotnet/standard/commandline/).
 * [collections.h](./src/collections.h): Generic collection macros for common data structures. Currently only a dynamic array and a hash table.
 * [ctest.h](./src/ctest.h): A unit testing library with auto-registration of test cases. This is the backbone for testing all the other libraries here.
 * [gc.h](./src/gc.h): A mark-and-sweep garbage collector.
 * [json.h](./src/json.h): A JSON parser and serializer with support for basic common extensions like comments or trailing commas.
 * [string_builder.h](./src/string_builder.h): Dynamic string builder with an additional API specific to emitting formatted code.

## Credits

There are a few projects that gave lots of inspiration and motivation to start, learn and design. Thank you for every single one of you for sharing your work. Without trying to be exhaustive, let me mention a few:
 * [stb libraries](https://github.com/nothings/stb): The obvious, well-known and awesome set of single-header libs by Sean Barrett.
 * [gc library](https://github.com/mkirchner/gc): A very simple and readable GC implementation for C. This library gave me the idea for the API and the idea to only track the root pointers for allocations. Really, the only "extra" mine adds is scanning global sections and automatically detecting the stack base pointer. (And I probably do everything else worse)
 * [btmc blog](https://btmc.substack.com/): A blog titled "Burning the Midnight Coffee" by Sir Whinesalot. [One of the blog posts](https://btmc.substack.com/p/implementing-generic-types-in-c?utm_source=publication-search) inspired me to redesign the [old collections utilities](https://gist.github.com/LPeter1997/0c9e7f51a10243b7db9cd03f5d6c1887) I had to the current form, apart from teaching me some [absolutely cursed C tricks](https://btmc.substack.com/p/implementing-generators-yield-in?utm_source=publication-search).

## Contribution

If for some reason you want to contribute, feel free to open an issue or a pull request. If you found a bug, a reproduction is greatly appreciated.
